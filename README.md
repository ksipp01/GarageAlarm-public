# GarageAlarm

Battery-powered garage-door monitor built around a Seeed Studio XIAO ESP32-C3 and PlatformIO.

## Current features

- Door open/closed monitoring with debounce
- Wi-Fi status/configuration web page
- Configurable open-door alert delay
- Pushover notifications
- Optional notification when the door closes after an alert
- ESP32-C3 deep sleep while the door is closed
- Wake when the door opens
- Preferences/NVS storage for settings
- JSON status endpoint at `/status`

## Hardware

- Seeed Studio XIAO ESP32-C3
- Door switch on D2
- Setup button on D1
- LiPo battery support planned
- Battery voltage monitoring on A0 planned using a 1 MΩ / 1 MΩ divider

The current bench-test configuration uses a normally-open momentary switch from D2 to GND:

- Released/HIGH = CLOSED
- Pressed/LOW = OPEN

## Setup

This is a PlatformIO Arduino project.

Copy:

`include/secrets.example.h`

to:

`include/secrets.h`

and enter your own Wi-Fi and Pushover credentials. The real `secrets.h` file is excluded from Git.

## Security

Do not commit Wi-Fi passwords or Pushover credentials. `include/secrets.h` is intentionally ignored.

## Status

The core wake / monitor / notify / close / deep-sleep cycle has been bench tested successfully. Battery measurement code is currently disabled until the battery hardware is connected and calibrated.
