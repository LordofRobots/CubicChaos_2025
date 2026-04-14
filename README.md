# CubicChaos — Smart Cube Robotics Game System

## Overview
CubicChaos is a distributed robotics game platform built around intelligent cubes, a centralized Field Management System (FMS), and a real-time web-based control interface.

The system combines embedded sensing, wireless communication, and game-state orchestration to create a dynamic, interactive competition environment where physical game elements actively participate in scoring and feedback.

At its core, CubicChaos transforms passive game pieces into autonomous, networked devices that:
- detect their own orientation
- communicate with a central controller
- display game state visually and audibly
- contribute directly to real-time scoring

---

## System Architecture

CubicChaos is composed of two primary embedded systems:

### 1. Smart Cubes
Each cube is an ESP32-based device with:
- IMU (MPU6050) for orientation detection
- Addressable LEDs (6 faces, 18 total)
- Piezo speaker for identification feedback
- ESP-NOW wireless communication

**Responsibilities:**
- Detect active face and stability
- Pair and communicate with the FMS
- Display color/state feedback
- Respond to identify commands
- Report telemetry (face, uptime, status)

---

### 2. Field Management System (FMS)
The FMS is the central controller responsible for:
- Managing all cube connections
- Running the match timeline
- Calculating scores
- Sending commands to cubes
- Hosting the operator interface

**Capabilities:**
- Supports up to 16 cubes
- Event-driven ESP-NOW scheduling
- Real-time scoring engine
- Web-based control + diagnostics
- Live telemetry streaming (SSE)

---

## Communication System

### Wireless Protocol
- Transport: **ESP-NOW (low latency, peer-to-peer)**
- Custom lightweight message protocol

### Message Types
- `PAIR_REQ` / `PAIR_ACK` — cube pairing
- `POLL` — FMS requests status
- `STATUS` — cube telemetry response
- `CMD` — FMS sends state/color/actions

### Behavior
1. Cube powers on and requests pairing
2. FMS accepts and registers cube
3. FMS polls cubes in a scheduled loop
4. Cubes respond with real-time state
5. FMS sends commands (color, state, identify)

---

## Game Engine

### System States
- `STANDBY`
- `GAME_START`
- `IN_GAME`
- `TIME_GATE`
- `END_GAME`
- `RESET`

These states drive:
- cube LED behavior
- scoring logic
- match flow timing

---

### Scoring Model
The FMS calculates scores based on:
- cube orientation (which face is up)
- assigned alliance color
- level/state of cube

**Output:**
- Blue alliance score
- Orange alliance score
- Real-time updates during gameplay

---

## Operator Interface

The FMS hosts a browser-based control system.

### Features
- Match start / reset controls
- Live timer
- Real-time scoreboard
- Cube count and status
- Per-cube diagnostics
- Identify cube control (audio + visual)

### Access
- Hosted directly on FMS device
- Accessible via local IP or mDNS

---

### API Endpoints

| Endpoint | Description |
|----------|-------------|
| `/control` | Main operator interface |
| `/diag` | Diagnostics page |
| `/api/state` | JSON snapshot of game state |
| `/api/start` | Start match |
| `/api/reset` | Reset match |
| `/api/identify` | Trigger cube identify |
| `/events` | Live SSE stream |

---

## Hardware Summary

### Cube Hardware
- ESP32-S3
- MPU6050 IMU
- 18x WS2812 LEDs
- Piezo speaker

### FMS Hardware
- ESP32
- Wi-Fi (station mode)
- Diagnostic LEDs
- Optional audio output

---

## Key Design Principles

### Distributed Intelligence
Cubes are not passive objects — each unit:
- senses its environment
- communicates independently
- contributes to system behavior

### Low-Latency Communication
- ESP-NOW avoids traditional Wi-Fi overhead
- deterministic polling model ensures consistent updates

### Real-Time Feedback
- LEDs provide immediate visual state
- audio enables physical identification
- web UI reflects live system state

### Robustness
- pairing retry logic
- link timeout handling
- per-cube health tracking
- event-driven scheduling

---

## Typical System Flow

1. Power on FMS
2. Power on cubes
3. Cubes auto-pair to FMS
4. Operator opens control interface
5. Start match
6. FMS:
   - polls cubes
   - updates score
   - drives cube visuals
7. End match → reset system

---

## Use Cases

- Robotics competitions (MiniBots / CubicChaos events)
- STEM education and demonstrations
- Interactive exhibits and public showcases
- Rapid prototyping of distributed robotic systems

---

## Technology Stack

### Embedded
- ESP32 / ESP32-S3
- ESP-NOW
- FastLED
- Wire (I2C)
- LEDC (audio PWM)

### Networking / UI
- AsyncWebServer
- Server-Sent Events (SSE)
- mDNS

---

## Project Structure
/Cubes_.ino → Cube firmware
/FMS_.ino → Field Management System firmware

---

## Purpose

This project exists to enable:
- fast deployment of a fully interactive robotics game system
- scalable multi-device coordination
- real-time scoring and visualization
- accessible, high-impact STEM experiences

CubicChaos bridges embedded systems, networking, and game design into a single cohesive platform.

---