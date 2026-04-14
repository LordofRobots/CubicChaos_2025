# CubicChaos Field Management System (FMS)

## Overview
This firmware runs the CubicChaos Field Management System on an ESP32-based controller.  
It manages wireless communication with all game cubes, runs the match timeline, calculates scores, hosts the operator web interface, and provides live diagnostics over Wi-Fi.

The FMS acts as the central controller for the CubicChaos game by:
- pairing with cubes over ESP-NOW
- polling cubes for live state
- sending color and game-state commands
- tracking score based on cube face and color state
- serving a browser-based control and diagnostics interface
- broadcasting real-time match data with Server-Sent Events (SSE)

## Main Functions
The FMS firmware handles six primary jobs:

1. **Cube management**
   - Tracks up to 16 cubes
   - Handles pairing and peer registration
   - Monitors presence, health, uptime, face, stability, and protocol version

2. **Match control**
   - Controls game states such as standby, start, in-game, endgame, and reset
   - Runs a timed match sequence
   - Applies system-wide state and color behavior to all connected cubes

3. **Scoring**
   - Calculates blue and orange alliance scores
   - Evaluates cube face state and assigned color level
   - Maintains live scoreboard values throughout the match

4. **Wireless scheduling**
   - Uses event-driven ESP-NOW communication
   - Polls cubes in slots
   - applies response timeout and inter-slot gap timing
   - sends command packets shortly after polling

5. **Web interface**
   - Hosts a control page and diagnostics page over Wi-Fi
   - Supports start, reset, and cube identify actions
   - pushes live state to the browser using SSE

6. **Diagnostics and operator feedback**
   - Local status LEDs
   - onboard sound output
   - diagnostic endpoints for field state and Wi-Fi details
   - mDNS support for easy local access

## Network and Access
### Wi-Fi
The FMS connects as a station to:
- SSID: `minibot_fms`

It retries connection robustly and caches its assigned IP.

### mDNS
The firmware starts an mDNS service so the interface can be reached by local hostname instead of IP.

## Hardware
### Main Pins
- `SOUND_PIN` = GPIO 32
- `DIAG_LED_PIN` = GPIO 33
- `FMS_SEL_PIN` = GPIO 36

### FMS ID Selection
The FMS ID is selected from the boot state of `GPIO 36`.  
This allows multiple field identities or field-side selection behavior without reflashing firmware.

## Wireless Protocol
The FMS uses protocol version `5` and communicates with cubes using:
- `MSG_PAIR_REQ`
- `MSG_PAIR_ACK`
- `MSG_POLL`
- `MSG_STATUS`
- `MSG_CMD`

### Communication model
- cubes request pairing
- FMS accepts matching cubes
- FMS polls cubes one at a time
- cubes respond with live status
- FMS sends command packets containing system state, color, and optional identify beep requests

## Cube Data Tracked
For each cube, the FMS maintains:
- MAC address / cube ID
- current face
- stability
- uptime
- current assigned color
- last seen time
- protocol version
- poll/response health history
- identify status
- presence and communication quality

## Match Timeline
The firmware includes built-in timeline control for a full match cycle.  
It manages:
- standby
- game start
- in-game progression
- timed gates
- endgame
- reset

Score and UI state are updated live during the match.

## Scoring Model
The FMS computes alliance scores from connected cube states.  
It distinguishes between:
- blue alliance cubes
- orange alliance cubes
- level 1 vs level 2 color assignments

This allows real-time scoreboard tracking directly from cube telemetry.

## Web Interface
The firmware hosts a browser-based operator interface with:
- match start button
- match stop/reset button
- live score display
- match timer
- current game state
- cube count
- per-cube status cards
- cube identify control
- diagnostics access

## Web Endpoints
### Main pages
- `/` or `/control` — main control interface
- `/diag` — diagnostic page

### API endpoints
- `/api/state` — authoritative JSON snapshot of current field state
- `/api/diag` — text-based diagnostic summary
- `/api/wifi` — Wi-Fi technical details
- `/api/start` — start match
- `/api/reset` — reset match
- `/api/identify?i=<index>` — trigger identify on a specific cube

### Live updates
- `/events` — Server-Sent Events stream for real-time UI updates

## Operator Features
The web UI allows the operator to:
- start a match
- stop/reset a match
- view live score and match time
- monitor cube health and communication quality
- identify a specific cube with audible/visual feedback
- inspect diagnostic and Wi-Fi state

## Diagnostic Features
The FMS includes:
- onboard diagnostic LED ring behavior
- Wi-Fi connection monitoring
- peer health tracking from poll history
- live JSON snapshot output
- separate diagnostics page for troubleshooting

## Intended Use
This firmware is intended for a CubicChaos field controller where one embedded device must manage:
- all game cubes
- match timing
- live scoring
- operator control
- diagnostics
- field-side visibility

## Notes
- Built around ESP32 Wi-Fi + ESP-NOW communication
- Uses AsyncWebServer, AsyncTCP, FastLED, mDNS, and LEDC audio
- Supports up to 16 cubes
- Designed for low-latency field control with browser-based operation