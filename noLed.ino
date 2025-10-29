#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_Alarm";
const char* password = "12345678";

WebServer server(80);

// Pin for the buzzer
#define BUZZER_PIN 15
bool alarmState = false;

String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Calamity Alarm</title>
<style>
body { font-family: sans-serif; text-align: center; margin-top: 40px; }
button {
  padding: 15px 25px; font-size: 18px; margin: 10px;
  border: none; border-radius: 8px; color: white;
}
.on { background-color: red; }
.off { background-color: green; }
</style>
</head>
<body>
<h2>ESP32 Calamity Alarm</h2>
<p>Status: <b id="status">OFF</b></p>
<button class="on" onclick="fetch('/alarm/on').then(r=>r.text()).then(t=>{document.getElementById('status').innerText='ON'})">Activate Alarm</button>
<button class="off" onclick="fetch('/alarm/off').then(r=>r.text()).then(t=>{document.getElementById('status').innerText='OFF'})">Stop Alarm</button>
</body>
</html>
)rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Create Wi-Fi hotspot
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Wi-Fi AP started. Connect to: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(IP);

  // Web server routes
  server.on("/", []() {
    server.send(200, "text/html", htmlPage());
  });

  server.on("/alarm/on", []() {
    digitalWrite(BUZZER_PIN, HIGH);
    alarmState = true;
    Serial.println("🚨 Alarm Activated!");
    server.send(200, "text/plain", "Alarm ON");
  });

  server.on("/alarm/off", []() {
    digitalWrite(BUZZER_PIN, LOW);
    alarmState = false;
    Serial.println("✅ Alarm Stopped");
    server.send(200, "text/plain", "Alarm OFF");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
