# CubicChaos Cube Firmware

## Overview
This firmware runs on the CubicChaos game cube hardware using an ESP32-S3.  
Each cube detects its current face orientation with an MPU6050, drives a 6-face LED system, plays identify tones through a piezo, and communicates wirelessly with the CubicChaos Field Management System (FMS) over ESP-NOW.

The cube is designed to act as an autonomous game element that can:
- determine which face is currently up
- report orientation and stability to the FMS
- receive color and state commands from the FMS
- provide strong visual and audible feedback during gameplay
- automatically pair to the correct FMS at startup

## Main Functions
The cube firmware handles five primary jobs:

1. **Orientation sensing**
   - Uses an MPU6050 over I2C
   - Detects which face is active
   - Determines whether the cube is stable within a defined angular threshold

2. **Wireless FMS communication**
   - Uses ESP-NOW with a lightweight custom protocol
   - Supports pairing, polling, status reporting, and command reception
   - Maintains link health and automatically re-enters pairing if the connection is lost

3. **LED state rendering**
   - Drives 18 addressable LEDs across 6 faces
   - Shows game colors, idle states, identify feedback, disconnect indication, and endgame effects
   - Includes face-aware rendering logic tied to current cube orientation

4. **Audio feedback**
   - Uses LEDC tone generation on a piezo output
   - Supports identify beeps triggered by the FMS

5. **Runtime resilience**
   - ISR-safe receive queue for ESP-NOW packets
   - link timeout handling
   - pairing retry/backoff logic
   - periodic status reporting tied to poll requests

## Hardware
### Pin Assignment
- `SCL` = GPIO 1
- `SDA` = GPIO 2
- `MPU_INT` = GPIO 3
- `PIEZO` = GPIO 4
- `LED` = GPIO 5

### LED Layout
- 6 faces
- 3 LEDs per face
- 18 LEDs total

### Sensor
- MPU6050 accelerometer/gyro
- used primarily for cube face detection and stability measurement

## Wireless Protocol
The cube uses protocol version `5` and supports the following packet types:
- `MSG_PAIR_REQ`
- `MSG_PAIR_ACK`
- `MSG_POLL`
- `MSG_STATUS`
- `MSG_CMD`

### Typical communication flow
1. Cube powers up
2. Startup face determines preferred FMS
3. Cube broadcasts pair requests
4. Matching FMS responds with a pair acknowledgment
5. FMS polls cube
6. Cube returns face, stability, and uptime
7. FMS sends state/color/identify commands

## Game State Support
The firmware supports these FMS-controlled system states:
- `SYS_STANDBY`
- `SYS_GAME_START`
- `SYS_IN_GAME`
- `SYS_TIME_GATE`
- `SYS_END_GAME`
- `SYS_RESET`

These states affect both LED behavior and overall cube presentation.

## LED Behavior Summary
The LED system is built to provide clear game-state feedback. Depending on commands and link condition, the cube can show:
- team/level color states
- orientation-aware display patterns
- identify flashing
- disconnect overlay
- standby/game/endgame visual effects

## Audio Behavior
The cube includes an identify sound routine:
- triggered by FMS command
- uses a 3000 Hz tone
- plays short repeated beeps for easy physical identification

## Startup Behavior
On boot, the cube:
1. initializes sound, LEDs, IMU, and ESP-NOW
2. samples orientation to determine startup face
3. selects the desired FMS based on that face
4. begins pairing and normal game communication

## Intended Use
This firmware is intended for CubicChaos smart game cubes in a multi-cube field environment where each cube must:
- be orientation-aware
- respond to centralized field control
- present clear feedback to players and operators
- maintain lightweight, robust wireless communication

## Notes
- Built for ESP32-S3
- Uses FastLED, ESP-NOW, Wire, and MPU6050 libraries
- Debug logging can be enabled with `CUBE_DEBUG`