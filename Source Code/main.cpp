/*
 * ╔══════════════════════════════════════════╗
 * ║        XE CHỞ HÀNG — ESP32-S3           ║
 * ╠══════════════════════════════════════════╣
 * ║ MOTOR TRÁI  : IN1=15, IN2=7,  PWM=16   ║
 * ║ MOTOR PHẢI  : IN1=5,  IN2=6,  PWM=4    ║
 * ║ ENCODER TRÁI: A=10,  B=11               ║
 * ║ ENCODER PHẢI: A=8,   B=9                ║
 * ╚══════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESP32Encoder.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// ─────────────────────────────────────────────
// CẤU HÌNH WiFi
// ─────────────────────────────────────────────
#define WIFI_SSID "YOUR_ID_WIFI_HERE"
#define WIFI_PASS "YOUR_PASSWORD_WIFI_HERE"

// ─────────────────────────────────────────────
// GPIO
// ─────────────────────────────────────────────
#define L_IN1 15
#define L_IN2 7
#define L_PWM 16

#define R_IN1 5
#define R_IN2 6
#define R_PWM 4

#define L_ENC_A 10
#define L_ENC_B 11
#define R_ENC_A 8
#define R_ENC_B 9

// ─────────────────────────────────────────────
// PWM — 10-bit (0–1023), 800 Hz
// ─────────────────────────────────────────────
#define PWM_FREQ 800
#define PWM_RES 10
#define PWM_MAX 1023
#define PWM_MIN_L 420
#define PWM_MIN_R 420

// ─────────────────────────────────────────────
// CƠ HỌC
// ─────────────────────────────────────────────
#define WHEEL_DIAMETER 0.065f
#define WHEEL_CIRC (WHEEL_DIAMETER * PI)

float PPR_L = 11000.0f * 2;
float PPR_R = 13300.0f * 2;

// ─────────────────────────────────────────────
// GEAR
// ─────────────────────────────────────────────
const float GEAR_MAX_RPM[] = {0.0f, 15.0f, 22.0f, 30.0f, 37.0f};

// ─────────────────────────────────────────────
// PID
// ─────────────────────────────────────────────
#define PID_KP 3.0f
#define PID_KI 1.8f
#define PID_KD 0.05f
#define PID_INTERVAL_MS 100
#define PID_WINDUP 100.0f

// ─────────────────────────────────────────────
// TRẠNG THÁI XE
// ─────────────────────────────────────────────
struct CarState {
  int gear = 0;
  bool handbrake = true;
  float joyX = 0.0f;
  float joyY = 0.0f;
  float targetRpmL = 0.0f;
  float targetRpmR = 0.0f;
  float actualRpmL = 0.0f;
  float actualRpmR = 0.0f;
  float speedMS = 0.0f;
} car;

struct PIDState {
  float integral = 0;
  float lastErr = 0;
  void reset() {
    integral = 0;
    lastErr = 0;
  }
};
PIDState pidL, pidR;

// ─────────────────────────────────────────────
// OBJECTS
// ─────────────────────────────────────────────
ESP32Encoder encL, encR;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastPidMs = 0;
unsigned long lastWsMs = 0;

#define WS_INTERVAL 100

// ═════════════════════════════════════════════
// HTML NHÚNG THẲNG
// ═════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Xe Chở Hàng</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg1:#0d1b2a;--bg2:#1e3a5f;--bg3:#2a5f8d;
  --accent:#00d9ff;--accent2:#ff6b35;--accent3:#ffd60a;
  --success:#00ff88;--danger:#ff0055;--warning:#ffaa00;
  --text:#ffffff;--text-dim:#b0c4de;--r:14px;
}
html,body{width:100%;height:100%}
body{
  background:linear-gradient(135deg,#0d1b2a 0%,#1a2f4a 25%,#254a6f 50%,#2a5f8d 75%,#1e3a5f 100%);
  background-attachment:fixed;
  color:var(--text);font-family:'Segoe UI',sans-serif;overflow:hidden;
  display:flex;flex-direction:column;
}
.topbar{
  display:flex;align-items:center;justify-content:space-between;padding:14px 16px;
  background:linear-gradient(90deg,rgba(30,58,95,0.95),rgba(13,27,42,0.95));
  backdrop-filter:blur(20px);border-bottom:2px solid var(--accent);
  box-shadow:0 8px 32px rgba(0,217,255,0.2);flex-shrink:0;
}
.logo{display:flex;align-items:center;gap:10px}
.logo-icon{
  width:42px;height:42px;border-radius:10px;
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  display:flex;align-items:center;justify-content:center;font-size:22px;
  box-shadow:0 0 30px rgba(0,217,255,0.5);animation:pulse-icon 3s ease-in-out infinite;
}
@keyframes pulse-icon{0%,100%{transform:scale(1);box-shadow:0 0 30px rgba(0,217,255,0.5);}50%{transform:scale(1.1);box-shadow:0 0 40px rgba(255,107,53,0.6);}}
.logo-text{font-size:16px;font-weight:900;letter-spacing:.8px;background:linear-gradient(135deg,var(--accent),var(--accent2));-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-transform:uppercase;}
.logo-sub{font-size:10px;color:var(--accent3);letter-spacing:1.5px;text-transform:uppercase;font-weight:700}
.status-pill{
  display:flex;align-items:center;gap:8px;padding:8px 14px;
  border-radius:20px;border:2px solid var(--accent);
  background:rgba(0,217,255,0.1);backdrop-filter:blur(10px);
  font-size:11px;color:var(--accent);font-weight:700;
}
.dot{width:10px;height:10px;border-radius:50%;background:var(--text-dim);transition:all .3s;}
.dot.on{background:var(--success);box-shadow:0 0 15px var(--success);}
.dot.off{background:var(--danger);box-shadow:0 0 15px var(--danger);}
.main{
  flex:1;display:flex;flex-direction:column;gap:16px;padding:16px;overflow:hidden;
}
.container{display:grid;grid-template-columns:1fr 1fr;gap:16px;flex:1;min-height:0;overflow:hidden}
.left{display:flex;flex-direction:column;gap:16px;min-height:0;overflow-y:auto}
.right{display:flex;flex-direction:column;gap:16px;min-height:0;overflow-y:auto}
.card{
  background:linear-gradient(135deg,rgba(42,95,141,0.3),rgba(30,58,95,0.4));
  border:2px solid rgba(0,217,255,0.3);border-radius:var(--r);
  padding:14px;backdrop-filter:blur(20px);transition:all .3s;
  box-shadow:0 8px 32px rgba(0,217,255,0.1),inset 0 1px 0 rgba(0,217,255,0.2);overflow:hidden;
  position:relative;flex-shrink:0;
}
.card::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,rgba(0,217,255,0.05),transparent);pointer-events:none;}
.card:hover{border-color:rgba(0,217,255,0.6);box-shadow:0 12px 40px rgba(0,217,255,0.2);}
/* IP */
.ip-banner{display:flex;justify-content:space-between;align-items:center;font-size:12px;padding:10px 14px;background:linear-gradient(135deg,rgba(0,217,255,0.05),rgba(255,107,53,0.05)) !important;border:2px solid var(--accent) !important;}
.ip-label{color:var(--accent);font-weight:800;display:flex;align-items:center;gap:6px}
.ip-val{color:var(--accent3);font-weight:900;font-family:monospace;font-size:14px;background:rgba(0,0,0,0.4);padding:6px 12px;border-radius:6px;border:1px solid var(--accent3);}
/* METRICS - RPM Monitor */
.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;padding:0 !important;margin:0;}
.metric{
  background:linear-gradient(135deg,rgba(0,217,255,0.1),rgba(255,107,53,0.08)) !important;
  border:2px solid rgba(0,217,255,0.4) !important;padding:14px 10px !important;
  border-radius:var(--r) !important;text-align:center;cursor:default;position:relative;
}
.metric::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,rgba(0,217,255,0.05),transparent);pointer-events:none;}
.metric:hover{transform:translateY(-3px) !important;border-color:var(--accent) !important;box-shadow:0 8px 24px rgba(0,217,255,0.3) !important;}
.m-label{font-size:9px;color:var(--accent3);text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px;font-weight:800}
.m-val{font-size:24px;font-weight:900;background:linear-gradient(135deg,var(--accent),#00ffaa);-webkit-background-clip:text;-webkit-text-fill-color:transparent;}
.m-unit{font-size:9px;color:var(--accent2);margin-top:2px;font-weight:700}
/* SPEED */
.speed-card{
  background:linear-gradient(135deg,rgba(0,217,255,0.15),rgba(255,106,53,0.1)) !important;
  border:3px solid var(--accent) !important;box-shadow:0 0 40px rgba(0,217,255,0.3) !important;
}
.speed-label{display:flex;gap:6px;margin-bottom:10px;align-items:center;color:var(--accent3);font-weight:800;font-size:11px;text-transform:uppercase;}
.speed-top{display:flex;align-items:baseline;justify-content:center;gap:12px;margin-bottom:10px;}
.speed-big{font-size:64px;font-weight:900;background:linear-gradient(135deg,var(--accent),var(--accent2));-webkit-background-clip:text;-webkit-text-fill-color:transparent;animation:bounce-speed .6s ease-in-out infinite;}
@keyframes bounce-speed{0%,100%{transform:scaleY(1);}50%{transform:scaleY(1.08);}}
.speed-units{display:flex;flex-direction:column;gap:1px}
.speed-ms{font-size:12px;color:var(--text-dim);font-weight:700}
.speed-kmh{font-size:12px;color:var(--accent2);font-weight:900}
.prog-wrap{
  height:10px;background:rgba(0,0,0,0.5);border-radius:8px;overflow:hidden;margin:10px 0;
  border:2px solid rgba(0,217,255,0.3);box-shadow:inset 0 2px 8px rgba(0,0,0,0.6);
}
.prog-fill{
  height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));
  transition:width .2s ease-out;box-shadow:0 0 20px rgba(0,217,255,0.8);
}
.prog-labels{display:flex;justify-content:space-between;font-size:9px;color:var(--accent2);font-weight:700}
/* STATS */
.stats-card{
  background:linear-gradient(135deg,rgba(255,107,53,0.15),rgba(0,217,255,0.1)) !important;
  border:2px solid var(--accent2) !important;box-shadow:0 0 35px rgba(255,107,53,0.25) !important;
  display:grid;grid-template-columns:repeat(3,1fr);gap:10px;padding:12px !important;
}
.stat-item{text-align:center;padding:8px;border-right:2px solid rgba(0,217,255,0.2)}
.stat-item:last-child{border-right:none}
.stat-label{font-size:9px;color:var(--accent3);text-transform:uppercase;letter-spacing:.3px;font-weight:800;margin-bottom:4px}
.stat-val{font-size:18px;font-weight:900;background:linear-gradient(135deg,var(--accent2),#ff9f5a);-webkit-background-clip:text;-webkit-text-fill-color:transparent;}
.stat-unit{font-size:8px;color:var(--accent2);font-weight:700;margin-top:1px}
/* CONTROLS */
.controls{
  display:flex;flex-direction:column;gap:12px;
  background:linear-gradient(135deg,rgba(0,217,255,0.08),rgba(255,107,53,0.05)) !important;
  border:2px solid var(--accent) !important;
}
.section-lbl{
  font-size:11px;color:var(--accent3);text-transform:uppercase;letter-spacing:.7px;
  font-weight:900;margin-bottom:6px;text-shadow:0 2px 8px rgba(0,0,0,0.4);
}
.gear-row{display:flex;gap:7px}
.g-btn{
  flex:1;padding:11px 6px;font-size:13px;font-weight:800;border-radius:var(--r);
  border:2px solid rgba(0,217,255,0.4);background:rgba(30,58,95,0.5);color:var(--text-dim);
  cursor:pointer;transition:all .2s;-webkit-tap-highlight-color:transparent;position:relative;overflow:hidden;
  text-transform:uppercase;letter-spacing:.3px;
}
.g-btn::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,rgba(0,217,255,0.1),transparent);pointer-events:none;}
.g-btn:disabled{opacity:.2;cursor:not-allowed}
.g-btn.active{
  background:linear-gradient(135deg,var(--accent),var(--accent2));color:#000;
  border-color:var(--accent3);box-shadow:0 0 25px rgba(0,217,255,0.6);transform:scale(1.08);
  font-weight:900;text-shadow:0 1px 3px rgba(0,0,0,0.3);
}
.g-btn[data-gear="N"].active{background:linear-gradient(135deg,var(--accent3),#ffaa00);color:#000;}
.hb-btn{
  width:100%;padding:13px;font-size:13px;font-weight:800;border-radius:var(--r);
  cursor:pointer;border:2px solid;transition:all .2s;-webkit-tap-highlight-color:transparent;
  display:flex;align-items:center;justify-content:center;gap:6px;position:relative;overflow:hidden;
  text-transform:uppercase;letter-spacing:.4px;
}
.hb-btn::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,rgba(255,255,255,0.1),transparent);pointer-events:none;}
.hb-btn.on{background:linear-gradient(135deg,rgba(255,0,85,0.4),rgba(255,0,55,0.3));color:var(--danger);border-color:var(--danger);animation:pulse-danger 1.5s ease-in-out infinite;box-shadow:0 0 20px rgba(255,0,85,0.4);}
@keyframes pulse-danger{0%,100%{box-shadow:0 0 20px rgba(255,0,85,0.4);}50%{box-shadow:0 0 30px rgba(255,0,85,0.6);}}
.hb-btn.off{
  background:linear-gradient(135deg,var(--success),#00dd77);color:#000;border-color:var(--success);
  box-shadow:0 0 20px rgba(0,255,136,0.5);font-weight:900;
}
.hb-notice{
  font-size:10px;text-align:center;min-height:18px;margin-top:4px;font-weight:700;
  animation:slideIn .3s ease-out;text-transform:uppercase;letter-spacing:.3px;
}
@keyframes slideIn{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0);}}
.hb-notice.warn{color:var(--warning)}
.hb-notice.ok{color:var(--success)}
/* JOY + BARS */
.joy-ctrl{
  display:flex;gap:14px;background:linear-gradient(135deg,rgba(0,217,255,0.08),rgba(255,107,53,0.05)) !important;
  border:2px solid var(--accent) !important;
}
.joy-wrap{flex:0 0 160px;display:flex;flex-direction:column;align-items:center;gap:6px}
.joy-label{font-size:10px;color:var(--accent3);font-weight:800;text-transform:uppercase;margin-bottom:4px;}
.joy-zone{
  width:140px;height:140px;border-radius:50%;border:3px solid rgba(0,217,255,0.5);
  background:radial-gradient(circle at 35% 35%,rgba(0,217,255,0.15),rgba(0,0,0,0.5));
  position:relative;touch-action:none;cursor:grab;
  box-shadow:inset 0 0 30px rgba(0,217,255,0.2),0 0 35px rgba(0,217,255,0.3);transition:all .2s;
}
.joy-zone:active{cursor:grabbing;box-shadow:inset 0 0 40px rgba(0,217,255,0.3),0 0 45px rgba(0,217,255,0.4);}
.joy-zone.locked{opacity:.3;cursor:not-allowed;pointer-events:none}
.ch{position:absolute;background:rgba(0,217,255,0.25);border:1px solid rgba(0,217,255,0.4)}
.ch.h{top:50%;left:10%;width:80%;height:2px;transform:translateY(-50%);border-radius:1px;}
.ch.v{left:50%;top:10%;height:80%;width:2px;transform:translateX(-50%);border-radius:1px;}
.joy-ring{position:absolute;inset:14px;border-radius:50%;border:2px dashed var(--accent2);pointer-events:none}
.joy-thumb{
  width:50px;height:50px;border-radius:50%;background:linear-gradient(135deg,var(--accent),var(--accent2));
  position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);pointer-events:none;
  box-shadow:0 0 25px rgba(0,217,255,0.8),inset 0 2px 8px rgba(255,255,255,0.3);transition:all .1s;
}
.joy-thumb.idle{background:linear-gradient(135deg,rgba(100,100,130,0.6),rgba(80,80,110,0.6));box-shadow:0 0 12px rgba(150,150,180,0.3);}
.joy-hint{font-size:9px;color:var(--accent2);font-style:italic;font-weight:700}
.side-bars{flex:1;display:flex;flex-direction:column;gap:9px;min-width:100px;overflow-y:auto}
.bar-row{display:flex;flex-direction:column;gap:2px;flex-shrink:0}
.bar-lbl{
  font-size:9px;color:var(--accent3);display:flex;justify-content:space-between;
  text-transform:uppercase;letter-spacing:.3px;font-weight:800;
}
.bar-lbl span:last-child{color:var(--accent);font-size:10px;font-weight:900}
.bar-bg{
  height:6px;background:rgba(0,0,0,0.5);border-radius:6px;overflow:hidden;
  border:1px solid rgba(0,217,255,0.3);box-shadow:inset 0 2px 4px rgba(0,0,0,0.6);
}
.bar-fill{
  height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));
  transition:width .1s ease-out;box-shadow:0 0 12px rgba(0,217,255,0.6);border-radius:6px;
}
.bar-fill.neg{background:linear-gradient(90deg,#ff0055,var(--danger));box-shadow:0 0 12px rgba(255,0,85,0.6);}
.bar-fill.idle{background:rgba(100,100,130,0.4);box-shadow:none;}
.divider{height:2px;background:linear-gradient(90deg,transparent,rgba(0,217,255,0.3),transparent);margin:6px 0}
/* BOTTOM */
.bottom{display:flex;justify-content:center;gap:16px;flex-shrink:0;padding-top:10px;border-top:2px solid rgba(0,217,255,0.2);}
.estop-btn{
  width:60%;max-width:450px;padding:14px;font-size:13px;font-weight:900;border-radius:var(--r);
  cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px;
  background:linear-gradient(135deg,rgba(255,0,85,0.4),rgba(255,0,55,0.3));color:var(--danger);
  border:3px solid var(--danger);transition:all .2s;-webkit-tap-highlight-color:transparent;
  position:relative;overflow:hidden;box-shadow:0 0 30px rgba(255,0,85,0.5);
  animation:pulse-estop 2s ease-in-out infinite;text-transform:uppercase;letter-spacing:.5px;
}
@keyframes pulse-estop{0%,100%{box-shadow:0 0 30px rgba(255,0,85,0.5);}50%{box-shadow:0 0 40px rgba(255,0,85,0.7);}}
.estop-btn:active{transform:scale(.95);box-shadow:0 0 50px rgba(255,0,85,0.8);}
.estop-btn::before{content:'';position:absolute;inset:0;background:linear-gradient(135deg,rgba(255,255,255,0.15),transparent);pointer-events:none;}
</style>
</head>
<body>

<div class="topbar">
  <div class="logo">
    <div class="logo-icon">🚛</div>
    <div><div class="logo-text">Xe Chở Hàng</div><div class="logo-sub">⚡ Control</div></div>
  </div>
  <div class="status-pill">
    <div class="dot" id="dot"></div>
    <span id="conn-text">Connecting...</span>
  </div>
</div>

<div class="main">
  <!-- IP Banner -->
  <div class="card ip-banner">
    <span class="ip-label">🌐 IP Address</span>
    <span class="ip-val" id="ip-val">...</span>
  </div>

  <!-- Main Container -->
  <div class="container">
    <!-- LEFT COLUMN -->
    <div class="left">
      <div class="card speed-card">
        <div class="speed-label">📊 Real-time Speed</div>
        <div class="speed-top">
          <span class="speed-big" id="speed-val">0.00</span>
          <div class="speed-units">
            <span class="speed-ms">m/s</span>
            <span class="speed-kmh" id="speed-kmh">0.00</span>
          </div>
        </div>
        <div class="prog-wrap">
          <div class="prog-fill" id="speed-bar" style="width:0%"></div>
        </div>
        <div class="prog-labels">
          <span>0%</span>
          <span id="spd-max-lbl">N·0.00m/s</span>
          <span>100%</span>
        </div>
      </div>

      <div class="card controls">
        <div class="section-lbl">⚙️ Transmission</div>
        <div class="gear-row" id="gear-row">
          <button class="g-btn active" data-gear="N">N</button>
          <button class="g-btn" data-gear="1" disabled>①</button>
          <button class="g-btn" data-gear="2" disabled>②</button>
          <button class="g-btn" data-gear="3" disabled>③</button>
          <button class="g-btn" data-gear="4" disabled>④</button>
        </div>
        <button class="hb-btn on" id="hb-btn">🅿️ PARKING BRAKE</button>
        <div class="hb-notice warn" id="hb-notice">Release to Drive</div>
      </div>
    </div>

    <!-- RIGHT COLUMN -->
    <div class="right">
      <!-- RPM Monitor -->
      <div class="metrics card" style="margin:0">
        <div class="metric">
          <div class="m-label">Left Wheel</div>
          <div class="m-val" id="rpm-l">0</div>
          <div class="m-unit">RPM</div>
        </div>
        <div class="metric">
          <div class="m-label">Right Wheel</div>
          <div class="m-val" id="rpm-r">0</div>
          <div class="m-unit">RPM</div>
        </div>
        <div class="metric">
          <div class="m-label">Current Gear</div>
          <div class="m-val" id="gear-disp">N</div>
          <div class="m-unit">/4</div>
        </div>
        <div class="metric">
          <div class="m-label">Direction</div>
          <div class="m-val" id="dir-icon">⏸</div>
          <div class="m-unit">&nbsp;</div>
        </div>
      </div>

      <!-- Joystick & Output -->
      <div class="card joy-ctrl">
        <div class="joy-wrap">
          <div class="joy-label">🕹️ Control</div>
          <div class="joy-zone locked" id="joystick">
            <div class="ch h"></div>
            <div class="ch v"></div>
            <div class="joy-ring"></div>
            <div class="joy-thumb idle" id="thumb"></div>
          </div>
          <div class="joy-hint" id="joy-hint">Release brake</div>
        </div>
        <div class="side-bars">
          <div style="font-size:9px;color:var(--accent3);font-weight:800;text-transform:uppercase;margin-bottom:6px">⚡ Output</div>
          <div class="bar-row">
            <div class="bar-lbl"><span>L-RPM</span><span id="pct-l">0%</span></div>
            <div class="bar-bg"><div class="bar-fill idle" id="bar-l" style="width:0%"></div></div>
          </div>
          <div class="bar-row">
            <div class="bar-lbl"><span>R-RPM</span><span id="pct-r">0%</span></div>
            <div class="bar-bg"><div class="bar-fill idle" id="bar-r" style="width:0%"></div></div>
          </div>
          <div class="divider"></div>
          <div class="bar-row">
            <div class="bar-lbl"><span>L-PWM</span><span id="pwm-l">0</span></div>
            <div class="bar-bg"><div class="bar-fill idle" id="pwm-bar-l" style="width:0%"></div></div>
          </div>
          <div class="bar-row">
            <div class="bar-lbl"><span>R-PWM</span><span id="pwm-r">0</span></div>
            <div class="bar-bg"><div class="bar-fill idle" id="pwm-bar-r" style="width:0%"></div></div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- STATS -->
  <div class="card stats-card">
    <div class="stat-item">
      <div class="stat-label">📍 Distance</div>
      <div class="stat-val" id="dist-val">0.00</div>
      <div class="stat-unit">m</div>
    </div>
    <div class="stat-item">
      <div class="stat-label">⏱️ Duration</div>
      <div class="stat-val" id="time-val">00:00</div>
      <div class="stat-unit">sec</div>
    </div>
    <div class="stat-item">
      <div class="stat-label">📊 Avg Speed</div>
      <div class="stat-val" id="avg-spd">0.00</div>
      <div class="stat-unit">m/s</div>
    </div>
  </div>

  <!-- BOTTOM - Centered ESTOP -->
  <div class="bottom">
    <button class="estop-btn" id="estop-btn">⛔ EMERGENCY STOP</button>
  </div>

</div>

<script>
const WHEEL_CIRC=Math.PI*0.065, GEAR_RPM=[0,15,22,30,37], PWM_MAX_UI=1023;
let gear=0,handbrake=true,joyX=0,joyY=0,joyActive=false,ws;
let sessionStart=0,totalDist=0,lastSpd=0;

function connectWS(){
  ws=new WebSocket(`ws://${location.hostname}/ws`);
  ws.onopen=()=>{setConn(true);document.getElementById('ip-val').textContent=location.hostname;};
  ws.onclose=()=>{setConn(false);setTimeout(connectWS,2000)};
  ws.onerror=()=>ws.close();
  ws.onmessage=e=>{
    try{const d=JSON.parse(e.data);
      document.getElementById('rpm-l').textContent=Math.abs(d.rpmL||0);
      document.getElementById('rpm-r').textContent=Math.abs(d.rpmR||0);
      const spd=parseFloat(d.speed)||0;
      document.getElementById('speed-val').textContent=spd.toFixed(2);
      document.getElementById('speed-kmh').textContent=(spd*3.6).toFixed(2);
      const maxSpd=(GEAR_RPM[d.gear]||0)*WHEEL_CIRC/60;
      document.getElementById('speed-bar').style.width=(maxSpd>0?Math.min(spd/maxSpd*100,100):0)+'%';
      updateStats(spd);
    }catch(e){}
  };
}
function setConn(ok){
  document.getElementById('dot').className='dot '+(ok?'on':'off');
  document.getElementById('conn-text').textContent=ok?'Connected':'Disconnected';
}
function send(o){if(ws&&ws.readyState===WebSocket.OPEN)ws.send(JSON.stringify(o));}

function updateStats(spd){
  const isMoving=spd>0.1;
  if(isMoving&&!sessionStart){sessionStart=Date.now();totalDist=0;}
  if(!isMoving&&sessionStart){sessionStart=0;return;}
  if(sessionStart){
    const elapsed=Math.floor((Date.now()-sessionStart)/1000);
    document.getElementById('time-val').textContent=elapsed;
    totalDist+=(spd*0.1)/1000;
    document.getElementById('dist-val').textContent=totalDist.toFixed(2);
    if(elapsed>0)document.getElementById('avg-spd').textContent=(totalDist*1000/elapsed).toFixed(2);
  }
}

const joystick=document.getElementById('joystick'),thumb=document.getElementById('thumb');
const R=70,TMAX=R-24;
const canMove=()=>!handbrake&&gear>0;
function joyPos(e){const r=joystick.getBoundingClientRect(),s=e.touches?e.touches[0]:e;return{x:s.clientX-r.left-R,y:s.clientY-r.top-R};}
function moveThumb(x,y){
  const d=Math.hypot(x,y);if(d>TMAX){x=x/d*TMAX;y=y/d*TMAX;}
  joyX=x/TMAX;joyY=-y/TMAX;
  thumb.style.left=(R+x)+'px';thumb.style.top=(R+y)+'px';
  updateUI();send({cmd:'joy',x:+joyX.toFixed(3),y:+joyY.toFixed(3)});
}
function resetJoy(){joyX=joyY=0;thumb.style.left=R+'px';thumb.style.top=R+'px';updateUI();send({cmd:'joy',x:0,y:0});}
joystick.addEventListener('mousedown',e=>{if(!canMove())return;joyActive=true;moveThumb(...Object.values(joyPos(e)));});
joystick.addEventListener('touchstart',e=>{if(!canMove())return;joyActive=true;e.preventDefault();const p=joyPos(e);moveThumb(p.x,p.y);},{passive:false});
window.addEventListener('mousemove',e=>{if(joyActive)moveThumb(...Object.values(joyPos(e)));});
window.addEventListener('touchmove',e=>{if(joyActive){e.preventDefault();const p=joyPos(e);moveThumb(p.x,p.y);}},{passive:false});
window.addEventListener('mouseup',()=>{if(joyActive){joyActive=false;resetJoy();}});
window.addEventListener('touchend',()=>{if(joyActive){joyActive=false;resetJoy();}});

document.getElementById('gear-row').addEventListener('click',e=>{
  const btn=e.target.closest('.g-btn');if(!btn||btn.disabled)return;
  gear=btn.dataset.gear==='N'?0:parseInt(btn.dataset.gear);
  resetJoy();send({cmd:'gear',val:gear});refreshGear();
});
function refreshGear(){
  document.querySelectorAll('.g-btn').forEach(b=>{
    const g=b.dataset.gear==='N'?0:parseInt(b.dataset.gear);
    b.disabled=handbrake&&b.dataset.gear!=='N';
    b.classList.toggle('active',g===gear);
  });
  document.getElementById('gear-disp').textContent=gear===0?'N':gear;
  updateJoyStyle();updateSpeedLabel();
}
document.getElementById('hb-btn').addEventListener('click',()=>{
  handbrake=!handbrake;
  if(handbrake){gear=0;resetJoy();send({cmd:'brake',val:true});sessionStart=0;}
  else send({cmd:'brake',val:false});
  const btn=document.getElementById('hb-btn'),notice=document.getElementById('hb-notice');
  if(handbrake){btn.className='hb-btn on';btn.textContent='🅿️ PARKING BRAKE';notice.textContent='Release to Drive';}
  else{btn.className='hb-btn off';btn.textContent='🔓 FREE TO DRIVE';notice.textContent='Select Gear';}
  refreshGear();
});
document.getElementById('estop-btn').addEventListener('click',()=>{
  handbrake=true;gear=0;resetJoy();send({cmd:'estop'});sessionStart=0;
  document.getElementById('hb-btn').className='hb-btn on';
  document.getElementById('hb-btn').textContent='🅿️ PARKING BRAKE';
  document.getElementById('hb-notice').textContent='Release to Drive';
  refreshGear();
});
function updateUI(){
  const idle=!canMove();
  let lP=idle?0:Math.round((joyY-joyX*.5)*100),rP=idle?0:Math.round((joyY+joyX*.5)*100);
  lP=Math.max(-100,Math.min(100,lP));rP=Math.max(-100,Math.min(100,rP));
  const pwmL=Math.round(Math.abs(lP)/100*PWM_MAX_UI),pwmR=Math.round(Math.abs(rP)/100*PWM_MAX_UI);
  document.getElementById('pct-l').textContent=lP+'%';
  document.getElementById('pct-r').textContent=rP+'%';
  document.getElementById('pwm-l').textContent=pwmL;
  document.getElementById('pwm-r').textContent=pwmR;
  const cls=v=>'bar-fill'+(idle?' idle':v<0?' neg':'');
  ['l','r'].forEach((s,i)=>{const p=i===0?lP:rP;document.getElementById('bar-'+s).style.width=Math.abs(p)+'%';document.getElementById('bar-'+s).className=cls(p);});
  document.getElementById('pwm-bar-l').style.width=(pwmL/PWM_MAX_UI*100)+'%';
  document.getElementById('pwm-bar-r').style.width=(pwmR/PWM_MAX_UI*100)+'%';
  document.getElementById('pwm-bar-l').className=cls(lP);
  document.getElementById('pwm-bar-r').className=cls(rP);
  const dir=document.getElementById('dir-icon');
  if(idle)dir.textContent=handbrake?'🅿':'⏸';
  else if(joyY>.15)dir.textContent='⬆';
  else if(joyY<-.15)dir.textContent='⬇';
  else if(joyX>.15)dir.textContent='➡';
  else if(joyX<-.15)dir.textContent='⬅';
  else dir.textContent='⏸';
}
function updateJoyStyle(){
  const zone=document.getElementById('joystick'),hint=document.getElementById('joy-hint');
  if(handbrake||gear===0){zone.className='joy-zone locked';thumb.className='joy-thumb idle';hint.textContent=handbrake?'Brake ON':'Sel. Gear';}
  else{zone.className='joy-zone';thumb.className='joy-thumb';hint.textContent='Control';}
}
function updateSpeedLabel(){
  const maxRPM=GEAR_RPM[gear]||0,maxSpd=maxRPM*WHEEL_CIRC/60,lbl=gear===0?'N':gear;
  document.getElementById('spd-max-lbl').textContent=`${lbl}·${maxSpd.toFixed(2)}m`;
}
refreshGear();updateUI();connectWS();
setInterval(updateStats,100);
</script>
</body>
</html>
)HTMLEOF";

// ═════════════════════════════════════════════
// MOTOR
// ═════════════════════════════════════════════
void setDir(int L, int R) {
  if (L > 0) {
    digitalWrite(L_IN1, LOW);
    digitalWrite(L_IN2, HIGH);
  } else if (L < 0) {
    digitalWrite(L_IN1, HIGH);
    digitalWrite(L_IN2, LOW);
  } else {
    digitalWrite(L_IN1, LOW);
    digitalWrite(L_IN2, LOW);
  }
  if (R > 0) {
    digitalWrite(R_IN1, LOW);
    digitalWrite(R_IN2, HIGH);
  } else if (R < 0) {
    digitalWrite(R_IN1, HIGH);
    digitalWrite(R_IN2, LOW);
  } else {
    digitalWrite(R_IN1, LOW);
    digitalWrite(R_IN2, LOW);
  }
}

void applyMotor(int pwmL, int pwmR) {
  auto withMin = [](int v, int minV) -> int {
    if (v == 0)
      return 0;
    return (v > 0 ? 1 : -1) * max(abs(v), minV);
  };
  pwmL = withMin(constrain(pwmL, -PWM_MAX, PWM_MAX), PWM_MIN_L);
  pwmR = withMin(constrain(pwmR, -PWM_MAX, PWM_MAX), PWM_MIN_R);
  setDir(pwmL > 0 ? 1 : (pwmL < 0 ? -1 : 0),
         pwmR > 0 ? 1 : (pwmR < 0 ? -1 : 0));
  ledcWrite(0, abs(pwmL));
  ledcWrite(1, abs(pwmR));
}

void stopAll() {
  setDir(0, 0);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  pidL.reset();
  pidR.reset();
}

// ═════════════════════════════════════════════
// TARGET RPM & PID
// ═════════════════════════════════════════════
void updateTargetRPM() {
  if (car.handbrake || car.gear == 0) {
    car.targetRpmL = car.targetRpmR = 0;
    return;
  }
  float maxRPM = GEAR_MAX_RPM[car.gear];
  car.targetRpmL = constrain(car.joyY - car.joyX * 0.5f, -1.0f, 1.0f) * maxRPM;
  car.targetRpmR = constrain(car.joyY + car.joyX * 0.5f, -1.0f, 1.0f) * maxRPM;
}

int computePID(float sp, float act, PIDState &pid, float dt, float maxRPM) {
  if (fabsf(sp) < 0.5f) {
    pid.reset();
    return 0;
  }
  float err = sp - act;
  pid.integral = constrain(pid.integral + err * dt, -PID_WINDUP, PID_WINDUP);
  float deriv = (err - pid.lastErr) / dt;
  pid.lastErr = err;
  float out = PID_KP * err + PID_KI * pid.integral + PID_KD * deriv;
  if (maxRPM < 1.0f)
    return 0;
  return constrain((int)(out / maxRPM * PWM_MAX), -PWM_MAX, PWM_MAX);
}

void runPID() {
  unsigned long now = millis();
  if (now - lastPidMs < PID_INTERVAL_MS)
    return;
  float dt = (now - lastPidMs) / 1000.0f;
  lastPidMs = now;

  long dA = encL.getCount();
  encL.clearCount();
  long dB = encR.getCount();
  encR.clearCount();
  car.actualRpmL = (dA / PPR_L) / dt * 60.0f;
  car.actualRpmR = (dB / PPR_R) / dt * 60.0f;
  car.speedMS = (fabsf(car.actualRpmL) + fabsf(car.actualRpmR)) / 2.0f *
                WHEEL_CIRC / 60.0f;

  if (car.handbrake || car.gear == 0) {
    stopAll();
    return;
  }
  float maxRPM = GEAR_MAX_RPM[car.gear];
  applyMotor(computePID(car.targetRpmL, car.actualRpmL, pidL, dt, maxRPM),
             computePID(car.targetRpmR, car.actualRpmR, pidR, dt, maxRPM));
}

// ═════════════════════════════════════════════
// WEBSOCKET
// ═════════════════════════════════════════════
void handleWsMsg(uint8_t *data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len))
    return;
  const char *cmd = doc["cmd"];
  if (!cmd)
    return;

  if (!strcmp(cmd, "joy")) {
    car.joyX = constrain((float)doc["x"], -1.0f, 1.0f);
    car.joyY = constrain((float)doc["y"], -1.0f, 1.0f);
    updateTargetRPM();
  } else if (!strcmp(cmd, "gear")) {
    int g = doc["val"] | 0;
    if (!car.handbrake && g >= 0 && g <= 4) {
      car.gear = g;
      car.joyX = car.joyY = 0;
      updateTargetRPM();
    }
  } else if (!strcmp(cmd, "brake")) {
    car.handbrake = (bool)doc["val"];
    if (car.handbrake) {
      car.gear = 0;
      car.joyX = car.joyY = 0;
      stopAll();
    }
    updateTargetRPM();
  } else if (!strcmp(cmd, "estop")) {
    car.handbrake = true;
    car.gear = 0;
    car.joyX = car.joyY = 0;
    stopAll();
  }
}

void broadcastStatus() {
  if (!ws.count())
    return;
  JsonDocument doc;
  doc["rpmL"] = (int)car.actualRpmL;
  doc["rpmR"] = (int)car.actualRpmR;
  doc["speed"] = roundf(car.speedMS * 100.0f) / 100.0f;
  doc["gear"] = car.gear;
  doc["brake"] = car.handbrake;
  char buf[200];
  serializeJson(doc, buf);
  ws.textAll(buf);
}

void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] #%u disconnected → ESTOP\n", client->id());
    car.handbrake = true;
    car.gear = 0;
    stopAll();
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode == WS_TEXT)
      handleWsMsg(data, len);
  }
}

// ═════════════════════════════════════════════
// SETUP
// ═════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== XE CHỞ HÀNG BOOT ===");

  // Motor GPIO — kéo LOW ngay để tránh boot glitch
  pinMode(L_IN1, OUTPUT);
  digitalWrite(L_IN1, LOW);
  pinMode(L_IN2, OUTPUT);
  digitalWrite(L_IN2, LOW);
  pinMode(R_IN1, OUTPUT);
  digitalWrite(R_IN1, LOW);
  pinMode(R_IN2, OUTPUT);
  digitalWrite(R_IN2, LOW);
  pinMode(L_PWM, OUTPUT);
  digitalWrite(L_PWM, LOW);
  pinMode(R_PWM, OUTPUT);
  digitalWrite(R_PWM, LOW);

  // LEDC PWM
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcAttachPin(L_PWM, 0);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcAttachPin(R_PWM, 1);
  ledcWrite(0, 0);
  ledcWrite(1, 0);

  // Encoder
  pinMode(L_ENC_A, INPUT_PULLUP);
  pinMode(L_ENC_B, INPUT_PULLUP);
  pinMode(R_ENC_A, INPUT_PULLUP);
  pinMode(R_ENC_B, INPUT_PULLUP);
  encL.attachFullQuad(L_ENC_A, L_ENC_B);
  encR.attachFullQuad(R_ENC_A, R_ENC_B);

  // WiFi
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++tries > 30) { // timeout 15s → restart
      Serial.println("\n[WiFi] Timeout! Restarting...");
      ESP.restart();
    }
  }
  Serial.println();
  Serial.println("╔══════════════════════════════╗");
  Serial.println("║      WIFI KẾT NỐI THÀNH CÔNG ║");
  Serial.print("║  IP: http://");
  Serial.print(WiFi.localIP());
  Serial.println("  ║");
  Serial.println("╚══════════════════════════════╝");

  // WebSocket + HTTP
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", INDEX_HTML);
  });
  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("[HTTP] Server started — mở trình duyệt, nhập IP ở trên");

  lastPidMs = lastWsMs = millis();
}

// ═════════════════════════════════════════════
// LOOP
// ═════════════════════════════════════════════
void loop() {
  runPID();

  if (millis() - lastWsMs > WS_INTERVAL) {
    lastWsMs = millis();
    broadcastStatus();
  }

  ws.cleanupClients();
}
