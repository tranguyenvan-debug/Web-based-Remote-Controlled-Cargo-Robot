
# 🚛 Cargo Transport Robot — ESP32-S3

A remotely operated cargo transport robot controlled over Wi‑Fi. An ESP32-S3 acts as the central controller, driving two DC motors with encoders, closed‑loop PID speed control, a virtual gearbox (gears N–4), and a real‑time web dashboard (joystick, parking brake, emergency stop).

The current chassis reuses a woven plastic storage bin as the body. The two driven wheels (motor + encoder) are at the front, and a self‑aligning caster wheel is at the rear.

--- 

## ✨ Features

- **Wi‑Fi control**: the web UI is embedded directly in the firmware (no separate app needed), communicating over WebSocket.
- **Virtual joystick**: drag on screen to drive forward/backward/left/right (touch‑enabled).
- **4-speed virtual gearbox (N, 1, 2, 3, 4)**: each gear maps to a different maximum RPM, capping speed and improving safety.
- **Parking brake**: locks the joystick and gear selector; the brake must be released before the vehicle can move.
- **Emergency stop (E‑STOP)**: cuts the motors immediately, re‑engages the parking brake, and returns to neutral (N).
- **Auto-stop on disconnect**: if the WebSocket connection drops (Wi‑Fi loss, browser closed, etc.), the robot automatically engages the parking brake.
- **Closed-loop PID control**: encoders measure each wheel's actual speed and the firmware adjusts PWM output to match the target speed, keeping both wheels in sync even under uneven load.
- **Real-time telemetry**: per-wheel RPM, speed (m/s, km/h), distance traveled, elapsed time, and average speed.

<img width="2304" height="2014" alt="z8050125881942_c35e7c582f268d0a03684a4e6d5927d4" src="https://github.com/user-attachments/assets/2b734c23-aaa5-4ce8-963b-60d563521ba0" />

<img width="2268" height="2560" alt="z8050125884054_ccfe8743f822ca6ef8028e7d9aaf0053" src="https://github.com/user-attachments/assets/378f0a75-5a5b-4c28-a7fa-b923455ea8de" />

<img width="2236" height="2222" alt="z8050125879150_40bcfcad9c2eb0e02cbd6144ce5a6dce" src="https://github.com/user-attachments/assets/a36cf312-92eb-4c0c-baad-3698f2f6d01a" />

<img width="1920" height="1080" alt="z8050131123213_465bf49cdefad02708fe183bb3b57d06" src="https://github.com/user-attachments/assets/13e359ca-27be-4386-b2ea-625a28c09c7b" />

<img width="1918" height="940" alt="z8050141489710_e5d582f91fca249241eff071ff49bf8d" src="https://github.com/user-attachments/assets/e5b262ab-8935-45ca-9c6d-da21e4c8c2c4" />

---

## 🧰 Hardware

| Component | Notes |
|---|---|
| Microcontroller | ESP32-S3-DevKitC-1 |
| Motors | 2× geared DC motors with encoders (front, driven wheels) |
| Motor driver | 2× L298N H-bridge modules |
| Rear wheel | Self-aligning caster wheel |
| Power | 3× 18650 cells (series), 25A BMS module for battery protection |
| Step-down | Buck converter module (XL4051) supplying 5V to the ESP32 and logic |
| Chassis | Woven plastic storage bin as the body, with an internal compartment for wiring and boards |

The full schematic (power, motor drivers, controller, encoders) was designed in Altium Designer (`Schematic_Robot.SchDoc`).

---

### Pinout (GPIO)

```
LEFT MOTOR   : IN1=15, IN2=7,  PWM=16
RIGHT MOTOR  : IN1=5,  IN2=6,  PWM=4
LEFT ENCODER : A=10,   B=11
RIGHT ENCODER: A=8,    B=9
```

---

## ⚙️ Control Specifications

- **PWM**: 10-bit resolution (0–1023), 800 Hz frequency, minimum PWM of 420 (to overcome motor static friction).
- **Mechanics**: wheel diameter 0.065 m.
- **Encoder pulses per revolution (PPR)**: left wheel ~22000, right wheel ~26600 (already doubled from full-quadrature reading) — may need re-calibration against the actual encoders.
- **Virtual gearbox**: maximum speed per gear — Gear 1: 15 RPM, Gear 2: 22 RPM, Gear 3: 30 RPM, Gear 4: 37 RPM.
- **PID**: Kp = 3.0, Ki = 1.8, Kd = 0.05, 100 ms control loop interval, integral anti-windup limit of ±100.

---

## 🖥️ Web Dashboard

The UI is embedded directly in the firmware (stored in PROGMEM); the ESP32 serves the page itself when a device connects to its IP address. No installation required — just join the same Wi‑Fi network.

Main dashboard elements:
- Banner showing the robot's current IP address.
- Real-time speedometer (m/s, km/h) with a progress bar relative to the current gear's max speed.
- Left/right wheel RPM readout, current gear, and direction indicator.
- Gear selector (N, 1–4) and parking brake button.
- Virtual joystick with live percentage and PWM value readouts for each wheel.
- Trip stats: distance, duration, average speed.
- Emergency stop button.

---

## 📁 Project Structure

```
.
├── src/
│   └── main.cpp          # Main firmware (Wi-Fi, WebSocket, PID, embedded web UI)
├── platformio.ini        # PlatformIO configuration
└── README.md
```

---

## 🚀 Setup & Flashing

This project uses **PlatformIO** (VS Code + the PlatformIO extension is recommended).

1. Clone/copy the project to your machine.
2. Open the project folder with PlatformIO.
3. In `src/main.cpp`, update your Wi‑Fi credentials:

   ```cpp
   #define WIFI_SSID "Your_WiFi_Name"
   #define WIFI_PASS "Your_WiFi_Password"
   ```

4. Double-check the GPIO pinout if your wiring differs from the defaults.
5. Build and flash the firmware:

   ```bash
   pio run --target upload
   ```

6. Open the Serial Monitor (115200 baud) to check the Wi‑Fi connection log and the assigned IP address:

   ```bash
   pio device monitor
   ```

7. Note the IP address shown in the Serial Monitor, then open it in a browser on any phone/laptop connected to the same Wi‑Fi network to access the control dashboard.

### Dependencies (auto-installed by PlatformIO)

- `madhephaestus/ESP32Encoder`
- `bblanchon/ArduinoJson`
- `mathieucarbou/AsyncTCP`
- `mathieucarbou/ESPAsyncWebServer`

---

## 🕹️ Usage

1. Open the robot's IP address in a browser and wait for the status to show **Connected**.
2. Press **🔓 FREE TO DRIVE** to release the parking brake.
3. Select a gear (1–4) matching the desired speed.
4. Use the joystick to drive: pull up/down to go forward/backward, left/right to turn.
5. In an emergency, press **⛔ EMERGENCY STOP** — the robot stops immediately and returns to parking brake / neutral gear.

---

## 🐞 Known Issues

- Motor direction is inverted in some cases — check the IN1/IN2 wiring or whether the encoder orientation matches the actual direction of rotation.
- The right wheel jerks while driving — likely caused by inaccurate encoder PPR calibration or PID gains (Kp/Ki/Kd) not yet tuned for the real load; needs re-measurement of actual PPR and further PID tuning.

---

## 🔭 Next Steps

- Re-calibrate encoder PPR based on actual measurements for each wheel.
- Fine-tune the PID controller to reduce jerk/vibration during acceleration or sudden direction changes.
- Consider adding current/voltage sensing for the battery to monitor power status on the dashboard.
- Add authentication (password) to the control dashboard to prevent unauthorized access from other devices on the same Wi‑Fi network.

---

If this project helped you or inspired your own robot build, please consider giving it a star ⭐ on GitHub — it means a lot and helps others discover this project!

<div align="center">
   
Made with ❤️ by nguyenvantra-debug (Văn Trà)

Ho Chi Minh City University of Technology (HCMUT) | Bachelor of Engineering in Electronics and Telecommunications | 2026
