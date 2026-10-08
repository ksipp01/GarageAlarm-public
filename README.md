# GarageAlarm

![Completed GarageAlarm](images/GarageAlarm.jpg)

A battery-powered, Wi-Fi garage-door monitor built using the **Seeed Studio XIAO ESP32-C3**, PlatformIO, and Pushover.

The device mounts directly to the garage door, requires no connection to the garage-door opener, and sends smartphone notifications when the door has been left open.

## Features

- Door open/closed detection using a mechanical tilt switch
- ESP32-C3 deep sleep to conserve battery
- Automatic wake when the garage door opens
- Configurable open-door alert delay
- Pushover smartphone notifications
- Optional notification when the garage door closes after an alert
- Local Wi-Fi configuration and status webpage
- Dedicated setup button to wake the device without opening the door
- Battery voltage and estimated charge percentage monitoring
- Nonvolatile storage of settings using Preferences/NVS
- JSON status endpoint at `/status`
- Webpage button to return the device to deep sleep

**Measured deep-sleep current: approximately 64 µA.**

## Hardware

- Seeed Studio XIAO ESP32-C3 with external Wi-Fi antenna
- 3.7 V, 1000 mAh LiPo battery
- Mechanical tilt switch
- Momentary setup pushbutton
- Two 220 kΩ resistors
- One 0.1 µF capacitor
- 3D-printed enclosure

## Wiring

| Component | Connection |
|---|---|
| Tilt switch | D2 to GND |
| Setup button | D1 to GND |
| Battery voltage divider | BAT+ → 220 kΩ → A0 → 220 kΩ → GND |
| Filter capacitor | 0.1 µF between A0 and GND |

Both the tilt switch and setup button use the ESP32's internal pull-up resistors.

**Important:** The tilt switch must be oriented so that:

- Door CLOSED = D2 HIGH
- Door OPEN = D2 LOW

The LOW signal wakes the ESP32-C3 from deep sleep.

## Software Requirements

- Visual Studio Code
- PlatformIO extension
- Seeed Studio XIAO ESP32-C3 board support
- Arduino framework
- Wi-Fi network
- Pushover account and smartphone app

## Installation

1. Download or clone this repository.
2. Open the project folder in Visual Studio Code with PlatformIO installed.
3. Copy `include/secrets.example.h` to `include/secrets.h`.
4. Edit `secrets.h` to enter your Wi-Fi and Pushover credentials.
5. Build and upload the firmware to the XIAO ESP32-C3.
6. Open the device's local configuration webpage to adjust settings.

The actual `secrets.h` file is excluded from Git to prevent accidental publication of credentials.

## Operation

### Garage door closed

The ESP32-C3 remains in deep sleep, minimizing battery consumption.

### Garage door opens

The tilt switch pulls D2 LOW, waking the ESP32. The device connects to Wi-Fi and begins monitoring the door.

### Door remains open

If the door remains open beyond the configured delay, a Pushover notification is sent.

### Door closes

An optional closure notification is sent if an open-door alert was previously generated. After confirming that the door remains closed for three seconds, the ESP32 returns to deep sleep.

### Setup mode

Press the D1 setup button to wake the ESP32 and access its configuration webpage.

The webpage includes a **Sleep Now** control to return the device to deep sleep when configuration is complete.

## Battery Monitoring

Battery voltage is measured through a high-resistance divider connected to A0.

Two 220 kΩ resistors minimize continuous current consumption. A 0.1 µF capacitor helps stabilize the ADC measurement.

The completed device measured approximately **64 µA during deep sleep**, including the voltage-divider current.

Actual battery life depends on door activity, Wi-Fi connection time, battery condition, and temperature.

## Upload Troubleshooting

If PlatformIO reports a serial write timeout, manually enter bootloader mode:

1. Hold BOOT on the XIAO ESP32-C3.
2. Briefly press and release RESET.
3. Continue holding BOOT for approximately two seconds.
4. Release BOOT.
5. Upload the firmware using PlatformIO.

## Security

Never publish actual Wi-Fi passwords or Pushover credentials.

The `include/secrets.h` file is excluded through `.gitignore`. The included `secrets.example.h` contains placeholder values.

## Project Status

The completed device has been assembled, installed on a garage door, and tested using battery power.

Verified functionality includes door-triggered wakeup, setup-button wakeup, Pushover notifications, battery monitoring, and automatic return to deep sleep.