# Accident-detection-and-notification-system.
A compact accident detection system using ESP32, MPU6050, and NEO-6M/uBlox GPS. It monitors acceleration and tilt through the 3-axis gyroscope/accelerometer via I²C, detects sudden impacts, and instantly sends the vehicle’s live location to a Telegram Bot for emergency notification. Ideal for real-time, lightweight vehicle safety monitoring.

**Overview**

This project monitors accelerations, rotation and GPS location to determine probable vehicle crashes. When the system detects a collision-like event it:

captures high-frequency IMU data around the event,
reads the latest GPS coordinates,
composes a human-readable alert (including Google Maps link),
sends the alert to a Telegram Bot (chat ID/group).
Designed for motorcycles, cars, small delivery vehicles, or prototype research. The entire stack runs on an ESP32 and uses minimal external components.

**Key features**

Real-time crash detection (acceleration + orientation change)
GPS location in the notification (latitude, longitude, speed, timestamp)
Telegram push notifications with location link and summary data
Local logging (optional to microSD or Serial) for post-event analysis
Adjustable thresholds for sensitivity/tuning

**Hardware List**

NodeMCU(ESP8266)
ESP32
MPU6050 (3-axis accelerometer + 3-axis gyroscope; 
communicates over I²C) — Adafruit or MPU6050 breakout
NEO-6M GPS module (u-blox) — UART TTL (3.3V)
LCD display
Buzzer
Wires, connectors, small protoboard.

**Wiring / pin mapping (typical)**

Adjust pins to your board and code.
MPU6050 (I²C)
VCC -> 3.3V (or 5V if your breakout supports it)
GND -> GND
SDA -> ESP32 SDA (commonly GPIO21)
SCL -> ESP32 SCL (commonly GPIO22)
INT -> ESP32 interrupt pin (optional, e.g. GPIO4)
NEO-6M (UART)
VCC -> 3.3V (confirm module voltage)
GND -> GND
TX -> ESP32 RX (use a hardware Serial e.g. RX2 = GPIO16)
RX -> ESP32 TX (TX2 = GPIO17)
PPS (if present) -> for precise timestamping (optional)
ESP32
Use a dedicated hardware Serial for GPS (Serial1 or Serial2) to avoid conflicts with USB Serial.

**Software requirements & recommended libraries**

Software : Arduino IDE

Libraries:
Wire (built-in) for I²C
Adafruit_MPU6050 / Adafruit_Sensor
TinyGPSPlus or TinyGPS++ 
WiFi.h and HTTPSRedirect or WiFiClientSecure for secure Telegram HTTPS calls (or UniversalTelegramBot library)
ArduinoJson
Preferences (ESP32) 

**How the detection algorithm works**

Principle: crash events usually produce a short, large acceleration spike along with rapid rotation and often a stop in movement. Combining accelerometer + gyro + GPS (speed drop / heading change) drastically reduces false positives.

Core idea (multi-condition detection):

Monitor accelerometer magnitude (g-force).
Check for high jerk or deceleration (> threshold_g) for short window.
Confirm with rotation spike from gyroscope (sudden angular velocity).
If combined conditions hold, trigger event and send notification.
