# CycleGuard Architecture Diagrams


---

## 1. System overview (block diagram)

```mermaid
graph LR
    subgraph BIKE["BIKE UNIT - Arduino Nano 33 IoT"]
        APDS["APDS-9960<br/>proximity (I2C)"]
        IMU["LSM6DS3 IMU<br/>impact (I2C)"]
        GPS["GPS NEO-6M<br/>location (UART)"]
        BTN["Push button<br/>power toggle"]
        NANO["Nano 33 IoT<br/>firmware"]
        LED["WS2812B<br/>LED strip"]
        BUZ["Piezo buzzer"]
        OLED["SSD1309 OLED"]
        FLASH["Onboard flash<br/>event log"]

        APDS --> NANO
        IMU --> NANO
        GPS --> NANO
        BTN --> NANO
        NANO --> LED
        NANO --> BUZ
        NANO --> OLED
        NANO --> FLASH
    end

    subgraph HUB["CENTRAL HUB - Raspberry Pi 5"]
        BROKER["Mosquitto<br/>MQTT broker"]
        PY["Python hub service<br/>3 threads"]
        DB["SQLite<br/>database"]
        WEB["Flask<br/>web server"]
    end

    BROWSER["Browser<br/>dashboard"]

    NANO -->|"WiFi / MQTT"| BROKER
    BROKER --> PY
    PY --> DB
    PY --> WEB
    WEB -->|"HTTP"| BROWSER
```

---

## 2. Bike node internal architecture

```mermaid
graph TD
    LOOP["Main loop - 50ms cycle"]

    LOOP --> BTN["checkButton<br/>power toggle"]
    LOOP --> SENSE["Read sensors<br/>APDS + IMU + GPS"]
    LOOP --> HEALTH["checkSensorHealth<br/>heartbeat"]
    LOOP --> STATE["Alert state machine"]
    LOOP --> NET["manageNetwork<br/>non-blocking"]
    LOOP --> TIMING["recordLoopTime<br/>instrumentation"]

    SENSE --> STATE
    HEALTH --> STATE

    STATE --> OUT["Outputs:<br/>LED strip + buzzer + OLED"]
    STATE --> DETECT["Event detection<br/>close-call + impact"]

    DETECT --> LOG["logEvent<br/>write to flash FIRST"]
    LOG --> PUB["publishEvent<br/>best-effort MQTT"]

    NET --> SYNC["syncPendingEvents<br/>replay unpublished"]
    SYNC --> PUB

    LOG -.->|"if publish fails"| PENDING["stays pending<br/>in flash"]
    PENDING -.->|"on reconnect"| SYNC
```

---

## 3. Central hub threading model

```mermaid
graph LR
    BROKER["Mosquitto broker<br/>port 1883"]

    subgraph PYTHON["Python hub service"]
        T1["Thread 1<br/>MQTT subscriber"]
        Q["queue.Queue<br/>thread-safe"]
        T2["Thread 2<br/>SQLite writer"]
        T3["Thread 3<br/>Flask web server"]
    end

    DB["SQLite<br/>cycleguard.db"]
    BROWSER["Browser dashboard"]

    BROKER --> T1
    T1 -->|"decode JSON<br/>enqueue"| Q
    Q -->|"dequeue"| T2
    T2 -->|"insert"| DB
    DB --> T3
    T3 -->|"HTTP poll 2s"| BROWSER
```

---

## 4. Close-call event flow (sequence diagram)

```mermaid
sequenceDiagram
    participant V as Vehicle
    participant S as APDS-9960
    participant N as Nano firmware
    participant F as Flash storage
    participant B as MQTT broker
    participant P as Pi hub
    participant D as Dashboard

    V->>S: passes close to rear
    S->>N: proximity reading rises
    N->>N: state -> DANGER/CRITICAL
    N->>N: held > 300ms?
    N->>F: write event (published=false)
    N->>B: publish JSON (best-effort)
    alt broker reachable
        B->>P: forward event
        P->>P: enqueue, write to SQLite
        N->>F: mark published=true
        D->>P: poll /api/events
        P->>D: return new event
    else broker unreachable
        N->>F: event stays pending
        Note over N,F: syncs later on reconnect
    end
```

---

## 5. Offline-first fault tolerance (state diagram)

```mermaid
stateDiagram-v2
    [*] --> Online

    Online --> Offline: WiFi/MQTT lost<br/>or button OFF
    Offline --> Online: connection restored<br/>or button ON

    state Online {
        [*] --> Publishing
        Publishing --> Publishing: event -> flash + MQTT
        Publishing --> Syncing: pending events exist
        Syncing --> Publishing: all synced
    }

    state Offline {
        [*] --> Queueing
        Queueing --> Queueing: event -> flash only<br/>(published=false)
    }

    Offline --> Syncing: reconnect triggers replay
```

---

## 6. Alert state machine

```mermaid
stateDiagram-v2
    [*] --> CLEAR

    CLEAR --> CAUTION: prox >= 20
    CAUTION --> DANGER: prox >= 80
    DANGER --> CRITICAL: prox >= 180
    CRITICAL --> DANGER: prox < 180
    DANGER --> CAUTION: prox < 80
    CAUTION --> CLEAR: prox < 20

    CLEAR: CLEAR (green)
    CAUTION: CAUTION (yellow)
    DANGER: DANGER (solid red)
    CRITICAL: CRITICAL (flash red + buzzer)

    DANGER --> IMPACT: IMU >= 1.8g
    CRITICAL --> IMPACT: IMU >= 1.8g
    CLEAR --> IMPACT: IMU >= 1.8g
    IMPACT: IMPACT (white flash + high tone, 2s)
    IMPACT --> CLEAR: banner expires

    CLEAR --> FAULT: sensor stuck 30s
    FAULT: FAULT (magenta + beep)
    FAULT --> CLEAR: sensor recovers
```

---

## 7. Communication protocols

```mermaid
graph TD
    subgraph L1["Sensor layer"]
        I2C["I2C - APDS, OLED, IMU"]
        UART["UART - GPS"]
    end
    subgraph L2["Network layer"]
        WIFI["WiFi - TCP/IP"]
    end
    subgraph L3["Transport layer"]
        MQTT["MQTT - pub/sub"]
    end
    subgraph L4["Presentation layer"]
        HTTP["HTTP - dashboard"]
    end

    I2C --> NANO["Nano firmware"]
    UART --> NANO
    NANO --> WIFI
    WIFI --> MQTT
    MQTT --> PI["Pi hub"]
    PI --> HTTP
    HTTP --> BROWSER["Browser"]
```