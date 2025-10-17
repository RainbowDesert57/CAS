#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Try the common I2C addresses (0x3C most common)
#define OLED_ADDR1 0x3C
#define OLED_ADDR2 0x3D

// Use the pins you gave (SDA, SCL)
#define I2C_SDA 26
#define I2C_SCL 27

// instantiate display pointer later after detection
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WebServer server(80);

// store the last text to display
String currentText = "Hello!\nConnect to\n192.168.4.1";

// HTML page served to clients
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 OLED Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body{font-family: Arial, Helvetica, sans-serif; padding:20px; background:#f2f2f2;}
    .card{background:#fff;padding:16px;border-radius:8px;max-width:600px;margin:auto;box-shadow:0 2px 6px rgba(0,0,0,0.15)}
    textarea{width:100%;height:120px;font-size:16px;padding:8px}
    button{padding:10px 16px;font-size:16px;margin-top:8px}
    small{color:#666}
  </style>
</head>
<body>
  <div class="card">
    <h2>ESP32 → OLED</h2>
    <p>Type text below and press <strong>Update</strong>. Text will appear on the OLED.</p>
    <textarea id="txt" placeholder="Type here..."></textarea>
    <br>
    <button onclick="sendText()">Update</button>
    <button onclick="getText()">Refresh from ESP32</button>
    <p><small>Connect to Wi-Fi: <strong>ESP32_DISPLAY</strong> → open <strong>http://192.168.4.1</strong></small></p>
    <hr>
    <h4>OLED Preview</h4>
    <pre id="preview" style="background:#000;color:#0f0;padding:10px;min-height:80px"></pre>
  </div>

<script>
async function sendText(){
  const txt = document.getElementById('txt').value;
  try {
    const res = await fetch('/set', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({text: txt})
    });
    const j = await res.json();
    document.getElementById('preview').textContent = j.text;
  } catch(e){
    alert('Error sending text: '+e);
  }
}

async function getText(){
  try {
    const res = await fetch('/get');
    const j = await res.json();
    document.getElementById('preview').textContent = j.text;
    document.getElementById('txt').value = j.text;
  } catch(e) {
    alert('Error fetching text: '+e);
  }
}

// Load current text on open
window.onload = getText;
</script>
</body>
</html>
)rawliteral";

// helper: wrap and print string on OLED (max ~21 chars per line at size 1)
void displayWrapped(const String &s) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const int maxCharsPerLine = 21; // approx for 128px width with default font
  int y = 0;
  int lineHeight = 8; // pixels
  int maxLines = SCREEN_HEIGHT / lineHeight; // 8 lines
  int printedLines = 0;

  String tmp = s;
  // replace CRLF combos and single CR
  tmp.replace("\r\n", "\n");
  tmp.replace('\r', '\n');

  while (tmp.length() > 0 && printedLines < maxLines) {
    int newlinePos = tmp.indexOf('\n');
    String line;
    if (newlinePos >= 0) {
      line = tmp.substring(0, newlinePos);
      tmp = tmp.substring(newlinePos + 1);
    } else {
      if (tmp.length() <= maxCharsPerLine) {
        line = tmp;
        tmp = "";
      } else {
        // cut at last space within maxCharsPerLine if possible
        int cut = -1;
        for (int i = maxCharsPerLine; i >= 0; --i) {
          if (i < (int)tmp.length() && tmp[i] == ' ') { cut = i; break; }
        }
        if (cut == -1) cut = maxCharsPerLine;
        line = tmp.substring(0, cut);
        // trim leading space on remainder
        tmp = tmp.substring(cut);
        while (tmp.startsWith(" ")) tmp = tmp.substring(1);
      }
    }
    display.setCursor(0, y);
    display.println(line);
    y += lineHeight;
    printedLines++;
  }
  display.display();
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleGetText() {
  String json = "{\"text\":";
  json += "\"" + currentText + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetText() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body missing");
    return;
  }
  String body = server.arg("plain");
  // Expecting JSON like {"text":"..."}
  // Very small parser to get text value
  int p = body.indexOf("\"text\"");
  String newText = "";
  if (p >= 0) {
    int colon = body.indexOf(':', p);
    if (colon >= 0) {
      int firstQuote = body.indexOf('"', colon);
      int secondQuote = body.indexOf('"', firstQuote + 1);
      if (firstQuote >= 0 && secondQuote > firstQuote) {
        newText = body.substring(firstQuote + 1, secondQuote);
      } else {
        // fallback: try entire substring after colon trimming spaces and braces
        newText = body.substring(colon + 1);
        newText.trim();
      }
    }
  } else {
    // fallback: if not JSON, treat whole body as text
    newText = body;
  }

  // replace simple escaped sequences from JS JSON.stringify (basic)
  newText.replace("\\n", "\n");
  newText.replace("\\r", "\r");

  currentText = newText;
  displayWrapped(currentText);

  // reply with json
  String resp = "{\"ok\":true,\"text\":\"" + currentText + "\"}";
  server.send(200, "application/json", resp);
}

void notFoundHandler(){
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize I2C with chosen pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("I2C SDA=%d SCL=%d\n", I2C_SDA, I2C_SCL);

  // Try to initialize display at 0x3C first, then 0x3D
  bool ok = false;
  Wire.beginTransmission(OLED_ADDR1);
  if (Wire.endTransmission() == 0) {
    Serial.println("OLED found at 0x3C");
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR1)) {
      Serial.println("Display init failed at 0x3C");
    } else ok = true;
  } else {
    Wire.beginTransmission(OLED_ADDR2);
    if (Wire.endTransmission() == 0) {
      Serial.println("OLED found at 0x3D");
      if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR2)) {
        Serial.println("Display init failed at 0x3D");
      } else ok = true;
    }
  }

  if (!ok) {
    // Try default init (some modules still work)
    Serial.println("Trying default OLED init at 0x3C");
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  // Start WiFi AP (open)
  const char *apSSID = "ESP32_DISPLAY";
  const char *apPassword = ""; // open

  WiFi.softAP(apSSID);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Start server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/get", HTTP_GET, handleGetText);
  server.on("/set", HTTP_POST, handleSetText);
  server.onNotFound(notFoundHandler);

  server.begin();
  Serial.println("HTTP server started");

  // show initial instructions text on OLED
  displayWrapped(currentText);
}

void loop() {
  server.handleClient();
  // nothing else needed; OLED updated on POST
}
