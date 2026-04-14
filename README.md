# CubicChaos Cube Firmware

Firmware for a **CubicChaos game cube node** built on an **ESP32-S3**.  
This code drives a self-contained interactive cube that detects its orientation, renders face lighting effects, plays sound cues, and communicates wirelessly with a central hub over **ESP-NOW**.

## Purpose

This firmware exists to make each cube an intelligent field element that can:

- determine which face is currently active using an **MPU6050 IMU**
- display game state through **addressable LEDs**
- play event-based **audio cues** through a piezo buzzer
- exchange status and commands with the **CubicChaos hub** over ESP-NOW
- remain responsive using a **nonblocking, connection-first architecture**

In practice, each cube acts as a wireless, orientation-aware game object that reports its state to the hub and visually reflects commands sent back from the game system.

## What This Firmware Does

### Core functions

- Initializes and manages an **MPU6050** with interrupt-driven data-ready handling
- Calculates cube orientation and resolves the active face using normalized accelerometer data
- Drives **18 addressable LEDs** arranged as **6 faces × 3 LEDs per face**
- Supports multiple lighting behaviors such as:
  - static color
  - breathing
  - sparkle
  - sharp sparkle
  - rainbow two-point mirrored effect
- Supports a configurable **center LED behavior** on each face
- Plays predefined nonblocking sound sequences for gameplay events
- Communicates with a hub using **ESP-NOW** packets
- Broadcasts status when unpaired or disconnected
- Monitors link health and attempts to re-pin to the hub Wi-Fi channel if communication is lost

## Hardware Summary

Target platform and peripherals used by this code:

- **ESP32-S3**
- **MPU6050** IMU over I2C
- **WS2812 / NeoPixel-style LEDs** via FastLED
- **Piezo buzzer** for sound output

### Pin assignments

These are the default GPIO assignments currently used in the code:

| Function | GPIO |
|---|---:|
| I2C SCL | 1 |
| I2C SDA | 2 |
| MPU INT | 3 |
| Piezo | 4 |
| LED Data | 5 |

## Wireless Protocol Overview

The cube communicates with the hub using a lightweight ESP-NOW protocol.

### Status sent by the cube

The cube reports:

- protocol version
- cube ID
- current detected face
- current color and mode
- center LED configuration
- stability flag

### Commands received from the hub

The hub can command:

- cube ID
- target color
- target animation mode
- center LED follow/manual behavior
- center LED color and mode
- sound effect trigger

## Orientation Detection

Face detection is based on averaged accelerometer readings from the MPU6050.

### Method used

- IMU runs with interrupt-driven data-ready signaling
- multiple acceleration samples are averaged in bounded time
- the acceleration vector is normalized to a unit vector
- the dominant axis is selected to determine the active face
- a **20° entry threshold** is used before accepting a face transition

This gives a stable face result while reducing noise and false switching.

## LED Behavior

The cube contains **18 LEDs total**, arranged as **3 LEDs per face** across **6 faces**.

### Supported colors

- White
- Blue
- Orange
- Red
- Green
- Off

### Supported modes

- `M_STATIC`
- `M_BREATH`
- `M_SPARKLE`
- `M_RAINBOW`
- `M_SPARKLE_SHARP`

### Special rendering behavior

- Rainbow mode lights exactly **two mirrored LEDs** across opposite faces
- Center LEDs can either:
  - follow the face lighting automatically, or
  - be independently controlled with their own color and mode
- When disconnected, the cube enters a clear **fault / connection-loss indication state**

## Sound Cues

The piezo output supports nonblocking tone sequences for gameplay feedback.

Included sound events:

- Start
- Timegate
- Approach
- Reset
- Endgame

The tone player is designed so it does not stall the rest of the firmware while sounds are playing.

## Connection Handling

This firmware is structured to prioritize reliable operation.

### Link behavior

- tracks recent successful communication with the hub
- marks the link unhealthy after repeated missed frame windows
- broadcasts status while disconnected
- periodically rescans for the `CubeHub` SSID to recover the correct channel
- re-aligns ESP-NOW peers after re-pinning the channel

This makes the cube more resilient to startup order issues, temporary dropouts, or channel changes.

## Main Execution Flow

### `setup()`

Initializes:

- serial debug output
- piezo output
- FastLED
- I2C and MPU6050
- interrupt handling for IMU data-ready
- Wi-Fi station mode
- hub channel scan
- ESP-NOW callbacks and peers

### `loop()`

Continuously performs:

- face detection update at fixed interval
- connection health monitoring
- channel recovery logic when disconnected
- full LED rendering update
- periodic cube status transmission
- nonblocking tone sequence updates

## Code Structure Highlights

Key sections in the file include:

- **protocol definitions** for cube status and cube commands
- **face-to-LED mapping helpers**
- **accelerometer sampling and face detection logic**
- **rendering functions** for each lighting mode
- **center LED control logic**
- **nonblocking tone sequence player**
- **ESP-NOW send/receive callbacks**
- **link monitoring and recovery logic**

## Dependencies

This sketch depends on:

- `WiFi.h`
- `esp_now.h`
- `esp_wifi.h`
- `Wire.h`
- `MPU6050.h`
- `FastLED.h`
- `Arduino.h`

## Configuration Notes

Before deploying, verify these items for your hardware:

- ESP32-S3 pin assignments
- MPU6050 wiring and interrupt pin
- LED count and physical face ordering
- axis remap matrix if the IMU is mounted in a rotated orientation
- hub SSID and expected wireless behavior
- desired LED brightness and color calibration

## Summary

This firmware turns an ESP32-S3-based cube into a robust wireless game element for **CubicChaos**. It combines orientation sensing, synchronized lighting, sound feedback, and hub communication in a compact nonblocking design suitable for interactive field gameplay.
