/* Vehicle Crash Logger with MPU6050 + Neo-6M GPS + LCD + Webserver
   Pins used:
   - I2C (MPU6050 + 16x2 I2C LCD): SDA = D2, SCL = D1  -> Wire.begin(D2, D1)
   - GPS: SoftwareSerial ss(D6, D5)  (RX = D6, TX = D5)
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>

#define BUZZER_PIN D8

//
// ---- USER CONFIG ----
const char* ssid     = "Airtel_123";
const char* password = "12356790";
// ----------------------

LiquidCrystal_I2C lcd(0x27, 16, 2);
TinyGPSPlus gps;
SoftwareSerial ss(D6, D5);   // GPS: RX=D6, TX=D5

WiFiServer server(80);

// ----- Telegram credentials -----
//String BOT_TOKEN = "your token";
//String CHAT_ID = "your id";

String BOT_TOKEN = "add your token";//update
String CHAT_ID = "add your id"; //update

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOT_TOKEN, clientTCP);

// MPU6050
const uint8_t MPU_ADDR = 0x68;
int16_t rawAx, rawAy, rawAz;
float AccX, AccY, AccZ;

// Keep last valid GPS fix
String lastLat = "0.000000";
String lastLon = "0.000000";
String lastTime = "--:--:--";
String lastDate = "--/--/----";

// Crash logging (circular buffer)
const int MAX_LOGS = 20; // As per RAM Availability 
struct CrashLog {
  int id;
  String position;
  String latitude;
  String longitude;
  String date;
  String time;
};
CrashLog logs[MAX_LOGS];
int logsCount = 0;     // how many logs stored (<= MAX_LOGS)
int logsNext = 0;      // index to insert next log (0..MAX_LOGS-1)

// Crash detection/re-arm control
unsigned long lastCrashMillis = 0;
const unsigned long CRASH_DEBOUNCE_MS = 3000; // block re-logging for this duration
bool crashArmed = true; // allow logging when true

// Helper --------------------------------------------------------------------
String getPositionFromAcc(float ax, float ay, float az) {
  // Calculate pitch and roll in degrees
  float pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / PI;
  float roll  = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / PI;

  // Orientation detection using tilt angles
  if (pitch > 40)  return "Left";         // leaning forward
  if (pitch < -40) return "Right";          // leaning backward
  if (roll > 40)   return "Back";         // tilting right
  if (roll < -40)  return "Front";          // tilting left

  // Flat/down position
  if (az < 0.3)    return "Down";
  return "Normal";
}

void saveCrash(String pos) {
  // store next entry in circular buffer
  int idx = logsNext;
  logs[idx].id = (logsCount < MAX_LOGS) ? (logsCount + 1) : ( (logsNext==0) ? MAX_LOGS : logsNext );
  // we prefer sequential ids in output (1..n) but when buffer wraps ids will reflect order of filling
  // simpler: store incremental serial number using a separate counter:
  static int serialNo = 0;
  serialNo++;
  logs[idx].id = serialNo;

  logs[idx].position = pos;
  logs[idx].latitude = lastLat;
  logs[idx].longitude = lastLon;
  logs[idx].date = lastDate;
  logs[idx].time = lastTime;

  logsNext = (logsNext + 1) % MAX_LOGS;
  if (logsCount < MAX_LOGS) logsCount++;

  // print to serial
  Serial.println(F("=== Crash Recorded ==="));
  Serial.print(F("S.No: ")); Serial.println(logs[idx].id);
  Serial.print(F("Position: ")); Serial.println(pos);
  Serial.print(F("Lat: ")); Serial.println(lastLat);
  Serial.print(F("Lon: ")); Serial.println(lastLon);
  Serial.print(F("Date: ")); Serial.println(lastDate);
  Serial.print(F("Time: ")); Serial.println(lastTime);
  Serial.println(F("======================"));
}

void serveClient(WiFiClient &client) {
  // read request (simple)
  String req = client.readStringUntil('\r');
  // skip rest of request headers
  while (client.available()) client.readStringUntil('\n');

  // Build HTML (small, memory conscious)
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'>"; // auto-refresh every 5s
  html += "<title>Crash Logs</title>";
  html += "<style>body{font-family:Arial,Helvetica,sans-serif;margin:10px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #444;padding:6px;text-align:center}th{background:#eee}</style>";
  html += "</head><body>";
  html += "<h2 style='text-align:center'>Vehicle Crash Log</h2>";
  html += "<p>Last GPS: " + lastLat + ", " + lastLon + " | Time: " + lastTime + " | Date: " + lastDate + "</p>";
  html += "<table><tr><th>S.No</th><th>Position</th><th>Latitude</th><th>Longitude</th><th>Date</th><th>Time</th><th>Map</th></tr>";

  // print logs in chronological order (oldest -> newest)
  // If buffer hasn't wrapped, logs are 0..logsCount-1
  // If wrapped, logsNext points to next insertion index; oldest is logsNext, newest is logsNext-1
  int printed = 0;
  if (logsCount > 0) {
    int start;
    if (logsCount < MAX_LOGS) {
      start = 0;
    } else {
      start = logsNext; // oldest item
    }
    for (int i = 0; i < logsCount; ++i) {
      int idx = (start + i) % MAX_LOGS;
      html += "<tr>";
      html += "<td>" + String(logs[idx].id) + "</td>";
      html += "<td>" + logs[idx].position + "</td>";
      html += "<td>" + logs[idx].latitude + "</td>";
      html += "<td>" + logs[idx].longitude + "</td>";
      html += "<td>" + logs[idx].date + "</td>";
      html += "<td>" + logs[idx].time + "</td>";
      html += "<td><a href='https://maps.google.com/?q=" + logs[idx].latitude + "," + logs[idx].longitude + "' target='_blank'>Open</a></td>";
      html += "</tr>";
      printed++;
      // safe guard
      if (printed > MAX_LOGS) break;
    }
  }

  html += "</table><p style='font-size:12px;color:#666'>Auto-refresh every 5s</p>";
  html += "</body></html>";

  client.print(html);
  client.stop();
}

// ----------- SEND TELEGRAM ALERT -----------
void sendTelegramCrash(String direction) {

  String msg = "🚨 *CRASH DETECTED* 🚨\n\n";
  msg += "📍 Position: *" + direction + "*\n";
  msg += "🌐 Latitude: " + lastLat + "\n";
  msg += "🌐 Longitude: " + lastLon + "\n";
  msg += "📅 Date: " + lastDate + "\n";
  msg += "⏰ Time: " + lastTime + "\n\n";
  msg += "🔗 Google Maps:\n";
  msg += "https://maps.google.com/?q=" + lastLat + "," + lastLon;

  bot.sendMessage(CHAT_ID, msg, "Markdown");
}

// Setup ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);

  // I2C on your pins
  Wire.begin(D2, D1);

  // Buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Crash Logger Boot");
  delay(800);

  // GPS Serial
  ss.begin(9600);

  // Wake MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // WiFi connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.print("WiFi connecting");
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    lcd.print(".");
    // after long timeout we still continue (GPS + logging will work offline)
    if (millis() - wifiStart > 20000) break;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(F("Connected. IP: "));
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.print("IP:");
    lcd.setCursor(0,1);
    lcd.print(WiFi.localIP());
    server.begin();

    clientTCP.setInsecure();  // IMPORTANT for Telegram SSL
    delay(2000);
  } else {
    Serial.println();
    Serial.println(F("WiFi not connected (continue without web)"));
    lcd.clear();
    lcd.print("WiFi failed");
    delay(1000);
    lcd.clear();
  }



  delay(1000);
}

// Loop ----------------------------------------------------------------------
void loop() {
  // ---- read GPS bytes and update last valid fix ----
  while (ss.available() > 0) {
    char c = ss.read();
    gps.encode(c);
  }
  if (gps.location.isValid()) {
    lastLat = String(gps.location.lat(), 6);
    lastLon = String(gps.location.lng(), 6);
  }
  if (gps.time.isValid()) {
    // UTC time from GPS, convert to hh:mm:ss string
    int h = gps.time.hour();
    int m = gps.time.minute();
    int s = gps.time.second();
    // Convert to local if needed (example IST +5:30). Comment out if you want UTC.
    m += 30;
    if (m >= 60) { m -= 60; h++; }
    h += 5;
    if (h >= 24) h -= 24;
    lastTime = (h < 10 ? "0" + String(h) : String(h)) + ":" +
               (m < 10 ? "0" + String(m) : String(m)) + ":" +
               (s < 10 ? "0" + String(s) : String(s));
  }
  if (gps.date.isValid()) {
    int d = gps.date.day();
    int mo = gps.date.month();
    int yr = gps.date.year();
    lastDate = (d < 10 ? "0" + String(d) : String(d)) + "/" +
               (mo < 10 ? "0" + String(mo) : String(mo)) + "/" +
               String(yr);
  }

  // ---- read MPU6050 accelerations ----
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  rawAx = Wire.read() << 8 | Wire.read();
  rawAy = Wire.read() << 8 | Wire.read();
  rawAz = Wire.read() << 8 | Wire.read();

  AccX = rawAx / 16384.0;
  AccY = rawAy / 16384.0;
  AccZ = rawAz / 16384.0;

  // ---- update LCD with last-known coordinates ----
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Lat:");
  lcd.print(lastLat.substring(0,6));
  lcd.setCursor(0,1);
  lcd.print("Lon:");
  lcd.print(lastLon.substring(0,6));

  // ---- position & crash detection ----
  String pos = getPositionFromAcc(AccX, AccY, AccZ);

  unsigned long now = millis();

  if (pos != "Normal") {
    // crash candidate - check debounce & arming
    if (crashArmed && (now - lastCrashMillis > CRASH_DEBOUNCE_MS)) {
      // register crash
      lcd.clear();
      lcd.print("CRASH!");
      lcd.setCursor(0,1);
      lcd.print(pos);
      saveCrash(pos);

      lastCrashMillis = now;
      crashArmed = false; // wait until orientation returns to Normal to re-arm
      //delay(1000);         // small pause for stability
      tone(BUZZER_PIN, 2000); //pin, frequency, duration (0 to 2k freq.) 1600
      delay(2000);
      noTone(BUZZER_PIN);
      sendTelegramCrash(pos);
    }
  } else {
    // normal orientation: re-arm (after debounce)
    if (!crashArmed && (now - lastCrashMillis > 500)) {
      crashArmed = true;
    }
  }

  // ---- handle incoming web clients (if WiFi connected) ----
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client = server.available();
    if (client) {
      // wait for HTTP request
      unsigned long tstart = millis();
      while (client.available() == 0 && millis() - tstart < 2000) {
        delay(1);
      }
      if (client.available()) serveClient(client);
      else client.stop();
    }
  }

  delay(200); // main loop pacing
}
