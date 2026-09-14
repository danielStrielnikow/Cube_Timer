# Cube Timer

A physical cube that tracks work time. The cube has 6 sides. You turn it to
one side, and a timer starts for that activity. The cube sends data over
WiFi to a backend, which saves work sessions in a database.

## How it works

1. Inside the cube there is an ESP32 with an MPU6050 accelerometer. The
   sensor detects which side is currently facing up (1-6).
2. The ESP32 sends the side number over WiFi (UDP) to the backend every
   time the side changes.
3. The backend (Java) receives this data, saves a work session in a
   PostgreSQL database, and sends an update over WebSocket.
4. A small OLED screen on the cube shows live which side is up and how
   long the cube has been on that side.

Side 6 is "sleep mode" - when the cube lies on this side, we treat it as
"not working right now", not as an activity.

```
┌─────────────────┐       UDP        ┌─────────────────┐
│   ESP32 +       │  ────────────►   │   Java Spring   │
│   MPU6050       │                  │   Boot Backend  │
└─────────────────┘                  └────────┬────────┘
                                               │
                                               ▼
                                      ┌─────────────────┐
                                      │   PostgreSQL    │
                                      │   (Docker)      │
                                      └─────────────────┘
```

## Hardware

- ESP32 (esp32dev board)
- MPU6050 accelerometer, I2C address: 0x68
- OLED SSD1306 display, 128x32, I2C address: 0x3C

## Wiring (I2C)

- SCL -> GPIO 22
- SDA -> GPIO 21

The MPU6050 and the OLED share the same I2C bus (they have different
addresses, so this is fine).

![Wiring diagram](docs/images/Cube_Timer.drawio.png)

## Photos

![The parts on a breadboard](docs/images/IMG_0180.jpg)

![Close-up of the display](docs/images/IMG_0179.jpg)

![MPU6050 connected to the ESP32](docs/images/IMG_0181.jpg)

## Project structure

```
Cube_Timer/
├── src/main.c                 # ESP32 firmware (C, ESP-IDF)
├── lib/cube_logic/            # side detection + timer logic (no hardware code, testable on a computer)
├── test/test_cube_logic/      # unit tests for lib/cube_logic
├── components/                # OLED display library
├── include/
│   ├── wifi_config.h          # your WiFi config (not in git)
│   └── wifi_config.example.h  # example config to copy
├── platformio.ini             # PlatformIO config (esp32 + native/test env)
├── Cube_Backend/               # Java Spring Boot backend
└── docker-compose.yml          # PostgreSQL in Docker
```

## How to run the firmware (ESP32)

You need PlatformIO installed.

1. Copy `include/wifi_config.example.h` to `include/wifi_config.h` and fill
   in your own values (WiFi, the IP of the computer running the backend):
   ```
   cp include/wifi_config.example.h include/wifi_config.h
   ```
2. Upload the program to the board:
   ```
   pio run --target upload
   ```
3. Watch the logs live:
   ```
   pio device monitor
   ```
   (baud rate 115200 is already set in `platformio.ini`)

## How to run the tests

The side-detection and timer logic (`lib/cube_logic`) does not touch any
hardware, so it can be tested on your computer, without the board:

```
pio test -e native
```

## How to run the backend

You need Java 21+, Maven (or `./mvnw` from the project) and Docker.

1. Start the database:
   ```
   docker compose up -d
   ```
2. Start the backend:
   ```
   cd Cube_Backend
   ./mvnw spring-boot:run
   ```
   The backend listens on port 8080 (HTTP/WebSocket) and port 5000
   (UDP - data from the cube).

## How side detection works

Besides movement, the accelerometer also feels gravity - the axis that
points down or up shows a reading close to "1g". The program checks which
of the X/Y/Z axes has a reading closest to that value, and uses it to
guess which side of the cube is currently facing up.

Side numbers: Z+ = 1, Z- = 2, X+ = 3, X- = 4, Y+ = 5, Y- = 6 (6 is the
sleep side). If no axis is close enough to 1g (the cube is moving or
tilted), the program does not report any side as "certain".

## How the on-screen timer works

This is a separate timer running on the ESP32 itself (it does not depend
on the backend). It counts how long the cube stays on the same side (1-5)
- as long as the side does not change, the time keeps going, even if the
cube is standing completely still. Changing the side (or side 6 - sleep)
resets the timer and it starts counting from zero again.

## Offline buffering

If the WiFi connection drops for a moment, the ESP32 does not throw away
the data. It saves the last 20 side changes in a small buffer in memory,
and sends them to the backend as soon as the connection comes back. If
the buffer fills up (WiFi is down for a long time), the oldest entry is
dropped to make room for the newest one.

## Known issues

- The OLED display sometimes reports an I2C write error after running for
  a while (`ssd1306_i2c_write ... i2c write failed`). This is most likely
  a loose wire to the display, not a software bug. It does not stop data
  from reaching the backend, it only breaks the screen.
- The movement/side thresholds (`SIDE_MIN_THRESHOLD` in
  `lib/cube_logic/cube_logic.h`) are set by eye, not measured - they may
  need tuning on real hardware.
- WiFi connection at startup has no timeout, so if there is no WiFi
  available, the device will wait forever before it even starts reading
  the sensor.

## Author

Daniel Strielnikow
