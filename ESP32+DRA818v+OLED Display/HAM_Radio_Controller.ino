#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <vector>

#define SERIAL_TIMEOUT 500
#define SERIAL_DELAY 200

#define DRA_RX_PIN 16
#define DRA_TX_PIN 17

const int led = 2;
const int pttPin = 18;
const int powerPin = 23;  // تغییر به GPIO23
const int powerHLPin = 13;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oled_ok = false;
bool moduleReady = false;

struct Channel {
  int id;
  String name;
  String frequency;
  String squelch;
  String txCtcss;
  String rxCtcss;
  String channelSpacing;
};

std::vector<Channel> channels;
int nextChannelId = 1;

const char* ap_ssid = "HAM_NODE";
const char* ap_password = "";

String saved_ssid = "";
String saved_password = "";
bool wifi_connected = false;
unsigned long wifi_connect_start_time = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;

String frequency = "145.6000";
String squelch = "1";
String volume = "4";
String channelSpacing = "0";
String txCtcss = "0000";
String rxCtcss = "0000";

WebServer server(80);
HardwareSerial draSerial(2);

Preferences preferences;

bool pttState = false;
bool powerState = true;
String txPower = "1";

bool sendATCommand(String command, String expectedResponse);
void updateOLED();
void displayIP(String ip);
void connectToWiFi();
void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();
void deleteWiFiCredentials();
void startAccessPoint();
void handleWiFiConfig();
void handleWiFiConfigGet();
void handleRoot();
void handleSubmit();
void handlePTT();
void handlePower();
void handlePowerHL();
void handleVolume();
void handleSquelch();
void handleNotFound();
void testCommand();
void loadChannels();
void saveChannels();
void handleSaveChannel();
void handleLoadChannel();
void handleDeleteChannel();
void handleChannelsPage();
String getChannelsHTML();

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(pttPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  pinMode(powerHLPin, OUTPUT);
    
  digitalWrite(led, LOW);
  digitalWrite(pttPin, HIGH);
  digitalWrite(powerPin, HIGH);
  digitalWrite(powerHLPin, HIGH);
    
  Serial.begin(115200);
  draSerial.begin(9600, SERIAL_8N1, DRA_RX_PIN, DRA_TX_PIN);

  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oled_ok = false;
  } else {
    oled_ok = true;
    display.clearDisplay();
    display.display();
    delay(500);
  }
  
  loadWiFiCredentials();
  loadChannels();
  
  if (saved_ssid != "") {
    connectToWiFi();
    if (!wifi_connected) {
      startAccessPoint();
    }
  } else {
    startAccessPoint();
  }

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  Serial.println("Sending Handshake Command to DRA818V...");
  if (sendATCommand("AT+DMOCONNECT", "+DMOCONNECT:0")) {
      moduleReady = true;
      String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;
      sendATCommand(groupCommand, "+DMOSETGROUP:0");
      sendATCommand("AT+DMOSETVOLUME=" + volume, "+DMOSETVOLUME:0");
  } else {
      moduleReady = false;
  }
  updateOLED();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/ptt", HTTP_GET, handlePTT);
  server.on("/power", HTTP_GET, handlePower);
  server.on("/powerHL", HTTP_GET, handlePowerHL);
  server.on("/volume", HTTP_POST, handleVolume);
  server.on("/squelch", HTTP_POST, handleSquelch);
  server.on("/wifi-config", HTTP_GET, handleWiFiConfigGet);
  server.on("/wifi-config", HTTP_POST, handleWiFiConfig);
  server.on("/channels", HTTP_GET, handleChannelsPage);
  server.on("/save-channel", HTTP_POST, handleSaveChannel);
  server.on("/load-channel", HTTP_POST, handleLoadChannel);
  server.on("/delete-channel", HTTP_POST, handleDeleteChannel);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void startAccessPoint() {
  IPAddress ip(192,168,1,35);
  IPAddress subnet(255,255,255,0);
    
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.softAPConfig(ip, ip, subnet);
  displayIP(WiFi.softAPIP().toString());
  wifi_connected = false;
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
  
  wifi_connect_start_time = millis();
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED && millis() - wifi_connect_start_time < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
    digitalWrite(led, !digitalRead(led));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    digitalWrite(led, HIGH);
    displayIP(WiFi.localIP().toString());
  } else {
    wifi_connected = false;
    digitalWrite(led, LOW);
  }
}

void saveWiFiCredentials(String ssid, String password) {
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  saved_ssid = ssid;
  saved_password = password;
}

void loadWiFiCredentials() {
  preferences.begin("wifi-config", true);
  saved_ssid = preferences.getString("ssid", "");
  saved_password = preferences.getString("password", "");
  preferences.end();
}

void deleteWiFiCredentials() {
  preferences.begin("wifi-config", false);
  preferences.remove("ssid");
  preferences.remove("password");
  preferences.end();
  saved_ssid = "";
  saved_password = "";
}

void loadChannels() {
  preferences.begin("channels", true);
  int channelCount = preferences.getInt("count", 0);
  nextChannelId = preferences.getInt("nextId", 1);
  channels.clear();
  
  for (int i = 0; i < channelCount; i++) {
    Channel ch;
    ch.id = preferences.getInt(String("ch" + String(i) + "_id").c_str(), 0);
    ch.name = preferences.getString(String("ch" + String(i) + "_name").c_str(), "");
    ch.frequency = preferences.getString(String("ch" + String(i) + "_freq").c_str(), "");
    ch.squelch = preferences.getString(String("ch" + String(i) + "_sql").c_str(), "1");
    ch.txCtcss = preferences.getString(String("ch" + String(i) + "_txct").c_str(), "0000");
    ch.rxCtcss = preferences.getString(String("ch" + String(i) + "_rxct").c_str(), "0000");
    ch.channelSpacing = preferences.getString(String("ch" + String(i) + "_spacing").c_str(), "0");
    
    if (ch.id != 0 && ch.name != "" && ch.frequency != "") {
      channels.push_back(ch);
    }
  }
  preferences.end();
}

void saveChannels() {
  preferences.begin("channels", false);
  preferences.putInt("count", channels.size());
  preferences.putInt("nextId", nextChannelId);
  
  for (int i = 0; i < channels.size(); i++) {
    preferences.putInt(String("ch" + String(i) + "_id").c_str(), channels[i].id);
    preferences.putString(String("ch" + String(i) + "_name").c_str(), channels[i].name);
    preferences.putString(String("ch" + String(i) + "_freq").c_str(), channels[i].frequency);
    preferences.putString(String("ch" + String(i) + "_sql").c_str(), channels[i].squelch);
    preferences.putString(String("ch" + String(i) + "_txct").c_str(), channels[i].txCtcss);
    preferences.putString(String("ch" + String(i) + "_rxct").c_str(), channels[i].rxCtcss);
    preferences.putString(String("ch" + String(i) + "_spacing").c_str(), channels[i].channelSpacing);
  }
  preferences.end();
}

void handleSaveChannel() {
  if (server.hasArg("channelName") && server.hasArg("frequency")) {
    String name = server.arg("channelName");
    String freq = server.arg("frequency");
    String sql = server.arg("squelch");
    String txct = server.arg("txCtcss");
    String rxct = server.arg("rxCtcss");
    String spacing = server.arg("channelSpacing");
    
    if (name.length() > 0 && freq.length() > 0) {
      for (const auto& ch : channels) {
        if (ch.name == name) {
          server.send(200, "text/html", "<div class='alert alert-danger'>Channel name already exists!</div>");
          return;
        }
      }
      
      Channel newChannel;
      newChannel.id = nextChannelId++;
      newChannel.name = name;
      newChannel.frequency = freq;
      newChannel.squelch = sql;
      newChannel.txCtcss = txct;
      newChannel.rxCtcss = rxct;
      newChannel.channelSpacing = spacing;
      
      channels.push_back(newChannel);
      saveChannels();
      
      String message = "<div class='alert alert-success'>Channel '" + name + "' saved!</div>";
      server.send(200, "text/html", message);
    } else {
      server.send(200, "text/html", "<div class='alert alert-danger'>Channel name and frequency are required!</div>");
    }
  }
}

void handleLoadChannel() {
  if (server.hasArg("channelId")) {
    int channelId = server.arg("channelId").toInt();
    
    for (const auto& ch : channels) {
      if (ch.id == channelId) {
        frequency = ch.frequency;
        squelch = ch.squelch;
        txCtcss = ch.txCtcss;
        rxCtcss = ch.rxCtcss;
        channelSpacing = ch.channelSpacing;
        
        if (moduleReady) {
          String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;
          sendATCommand(groupCommand, "+DMOSETGROUP:0");
          sendATCommand("AT+DMOSETVOLUME=" + volume, "+DMOSETVOLUME:0");
          updateOLED();
        }
        
        String message = "<div class='alert alert-success'>Channel '" + ch.name + "' loaded!</div>";
        server.send(200, "text/html", message);
        return;
      }
    }
    server.send(200, "text/html", "<div class='alert alert-danger'>Channel not found!</div>");
  }
}

void handleDeleteChannel() {
  if (server.hasArg("channelId")) {
    int channelId = server.arg("channelId").toInt();
    String deletedName = "";
    
    for (auto it = channels.begin(); it != channels.end(); ++it) {
      if (it->id == channelId) {
        deletedName = it->name;
        channels.erase(it);
        saveChannels();
        break;
      }
    }
    
    if (deletedName != "") {
      String message = "<div class='alert alert-warning'>Channel '" + deletedName + "' deleted!</div>";
      server.send(200, "text/html", message);
    } else {
      server.send(200, "text/html", "<div class='alert alert-danger'>Channel not found!</div>");
    }
  }
}

String getChannelsHTML() {
  String html = "";
  
  if (channels.empty()) {
    html = "<div class='alert alert-info'>No channels saved yet.</div>";
  } else {
    html = "<div class='table-responsive'><table class='table table-striped'><thead><tr><th>Name</th><th>Frequency</th><th>SQL</th><th>Spacing</th><th>Actions</th></tr></thead><tbody>";
    
    for (const auto& ch : channels) {
      html += "<tr>";
      html += "<td><strong>" + ch.name + "</strong></td>";
      html += "<td>" + ch.frequency + " MHz</td>";
      html += "<td>" + ch.squelch + "</td>";
      html += "<td>" + String(ch.channelSpacing == "0" ? "12.5K" : "25K") + "</td>";
      html += "<td>";
      html += "<button onclick='loadChannel(" + String(ch.id) + ")' class='btn btn-sm btn-success'>Load</button> ";
      html += "<button onclick='deleteChannel(" + String(ch.id) + ")' class='btn btn-sm btn-danger'>Delete</button>";
      html += "</td>";
      html += "</tr>";
    }
    html += "</tbody></table></div>";
  }
  return html;
}

void handleChannelsPage() {
  String page = "<!DOCTYPE HTML><html><head><title>Channel Manager</title>"
               "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
               "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'>"
               "<style>body { font-family: Arial, sans-serif; margin: 20px; background-color: #f8f9fa; } .container { max-width: 800px; background: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); } .channel-list { max-height: 400px; overflow-y: auto; }</style>"
               "</head><body>"
               "<div class='container'>"
               "<h2 class='text-center mb-4'>Channel Manager</h2>"
               "<div class='card'>"
               "<div class='card-header'>Saved Channels (" + String(channels.size()) + ")</div>"
               "<div class='card-body channel-list'>"
               "<div id='channelsList'>" + getChannelsHTML() + "</div>"
               "</div></div>"
               "<div class='text-center mt-3'>"
               "<a href='/' class='btn btn-secondary'>Back to Main</a>"
               "</div>"
               "</div>"
               "<script>"
               "function loadChannel(channelId) {"
               "  if (!confirm('Load this channel?')) return;"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/load-channel', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      document.getElementById('channelsList').innerHTML = xhr.responseText;"
               "      setTimeout(function() { window.location.href = '/'; }, 1000);"
               "    }"
               "  };"
               "  var params = 'channelId=' + channelId;"
               "  xhr.send(params);"
               "}"
               "function deleteChannel(channelId) {"
               "  if (!confirm('Are you sure you want to delete this channel?')) return;"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/delete-channel', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      document.getElementById('channelsList').innerHTML = xhr.responseText;"
               "      setTimeout(function() { location.reload(); }, 1000);"
               "    }"
               "  };"
               "  var params = 'channelId=' + channelId;"
               "  xhr.send(params);"
               "}"
               "</script></body></html>";
  server.send(200, "text/html", page);
}

void loop(void) {
  server.handleClient();
  if (!wifi_connected && saved_ssid != "" && millis() - wifi_connect_start_time > 30000) {
    connectToWiFi();
  }
  testCommand();
}

void handleWiFiConfigGet() {
  String page = "<!DOCTYPE HTML><html><head><title>WiFi Configuration</title>"
               "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
               "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'>"
               "<style>body { font-family: Arial, sans-serif; margin: 20px; background-color: #f8f9fa; } .container { max-width: 500px; background: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }</style>"
               "</head><body>"
               "<div class='container'>"
               "<h2 class='text-center mb-4'>WiFi Configuration</h2>"
               "<form method='POST' action='/wifi-config'>"
               "<div class='form-group'>"
               "<label for='ssid'>WiFi SSID</label>"
               "<input type='text' class='form-control' id='ssid' name='ssid' placeholder='Enter your WiFi name' value='" + saved_ssid + "'>"
               "</div>"
               "<div class='form-group'>"
               "<label for='password'>WiFi Password</label>"
               "<input type='password' class='form-control' id='password' name='password' placeholder='Enter your WiFi password' value='" + saved_password + "'>"
               "</div>"
               "<button type='submit' class='btn btn-primary btn-block'>Save & Connect</button>"
               "</form>";
  
  if (saved_ssid != "") {
    page += "<hr><form method='POST' action='/wifi-config'><input type='hidden' name='delete' value='true'><button type='submit' class='btn btn-danger btn-block'>Delete Saved WiFi</button></form>";
  }
  
  page += "<div class='text-center mt-3'><a href='/' class='btn btn-secondary'>Back to Main</a></div></div></body></html>";
  server.send(200, "text/html", page);
}

void handleWiFiConfig() {
  String message = "";
  
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    
    if (new_ssid.length() > 0) {
      saveWiFiCredentials(new_ssid, new_password);
      message = "<div class='alert alert-success'>WiFi credentials saved successfully!</div><div class='alert alert-info'>Connecting to: <strong>" + new_ssid + "</strong></div><p>Device will restart and attempt to connect to the new network.</p><p>If connection fails, access point will be available again.</p>";
      server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='5;url/'></head><body><div class='container mt-5'>" + message + "</div></body></html>");
      delay(2000);
      ESP.restart();
      return;
    }
  } else if (server.hasArg("delete")) {
    deleteWiFiCredentials();
    message = "<div class='alert alert-warning'>WiFi credentials deleted!</div><p>Device will restart in Access Point mode.</p>";
    server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='3;url/'></head><body><div class='container mt-5'>" + message + "</div></body></html>");
    delay(2000);
    ESP.restart();
    return;
  }
  handleWiFiConfigGet();
}

void handleVolume() {
  if (server.hasArg("value")) {
    volume = server.arg("value");
    if (moduleReady) {
      sendATCommand("AT+DMOSETVOLUME=" + volume, "+DMOSETVOLUME:0");
      updateOLED();
    }
    server.send(200, "text/plain", "Volume set to " + volume);
  }
}

void handleSquelch() {
  if (server.hasArg("value")) {
    squelch = server.arg("value");
    if (moduleReady) {
      String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;
      sendATCommand(groupCommand, "+DMOSETGROUP:0");
      updateOLED();
    }
    server.send(200, "text/plain", "Squelch set to " + squelch);
  }
}

bool sendATCommand(String command, String expectedResponse) {
  if (!moduleReady && command != "AT+DMOCONNECT") {
      return false;
  }
  
  draSerial.println(command);
  delay(SERIAL_DELAY);
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < SERIAL_TIMEOUT) {
    if (draSerial.available() > 0) {
      char c = draSerial.read();
      response += c;
      startTime = millis();
    }
    delay(1);
  }

  Serial.print("AT Command: ");
  Serial.println(command);
  Serial.print("Response: ");
  Serial.println(response);
  
  if (expectedResponse != "" && response.indexOf(expectedResponse) != -1) {
      return true;
  } else if (expectedResponse == "") {
      return true;
  } else {
      return false;
  }
}

void displayIP(String ip) {
  if (!oled_ok) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(35, 1);
  display.print("HAM NODE");
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  if (wifi_connected) {
    display.print("STA MODE ACTIVE");
  } else {
    display.print("AP MODE ACTIVE");
  }
  display.setCursor(10, 35);
  display.print("IP: ");
  display.print(ip);
  display.display();
  delay(2000);
}

void updateOLED() {
  if (!oled_ok) return;
  display.clearDisplay();
  
  // Header با طراحی جدید
  display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(40, 4);
  display.print("HAM RADIO");
  
  // فرکانس اصلی - بزرگ و خوانا
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print(frequency.substring(0, 3));
  display.setTextSize(1);
  display.setCursor(70, 25);
  display.print(frequency.substring(3));
  display.setTextSize(1);
  display.setCursor(105, 25);
  display.print("MHz");
  
  // خط جداکننده
  display.drawFastHLine(0, 40, 128, SSD1306_WHITE);
  
  // وضعیت‌ها - در دو ردیف
  display.setTextSize(1);
  
  // ردیف اول
  display.setCursor(0, 45);
  display.print("TX:");
  display.print(pttState ? "ON " : "OFF");
  
  display.setCursor(45, 45);
  display.print("PWR:");
  display.print(txPower == "1" ? "HI" : "LO");
  
  display.setCursor(85, 45);
  display.print("VOL:");
  display.print(volume);
  
  // ردیف دوم
  display.setCursor(0, 55);
  display.print("SQL:");
  display.print(squelch);
  
  display.setCursor(35, 55);
  display.print("MOD:");
  display.print(moduleReady ? "OK" : "ERR");
  
  display.setCursor(75, 55);
  display.print("NET:");
  display.print(wifi_connected ? "WiFi" : "AP");
  
  display.display();
}

void handlePTT() {
  String message = "";
  if (!moduleReady) {
    message = "ERROR: DRA818V is not ready. Cannot change PTT.";
  } else {
    pttState = !pttState;
    digitalWrite(pttPin, pttState ? LOW : HIGH);
    message = String(pttState ? "TX ACTIVE" : "STANDBY");
    updateOLED();
  }
  server.send(200, "text/plain", message);
}

void handlePower() {
  String message;
  if (!moduleReady) {
    message = "ERROR: DRA818V is not ready. Cannot change Power status.";
  } else {
    powerState = !powerState;
    digitalWrite(powerPin, powerState ? HIGH : LOW);
    message = String(powerState ? "MODULE ON" : "MODULE OFF");
    updateOLED();
  }
  server.send(200, "text/plain", message);
}

void handlePowerHL() {
  String message;
  if (!moduleReady) {
    message = "ERROR: DRA818V is not ready. Cannot change Power Level.";
  } else {
    if (txPower == "1") {
      txPower = "0";
      digitalWrite(powerHLPin, LOW);
    } else {
      txPower = "1";
      digitalWrite(powerHLPin, HIGH);
    }
    message = String(txPower == "1" ? "HIGH POWER" : "LOW POWER");
    updateOLED();
  }
  server.send(200, "text/plain", message);
}

void handleSubmit() {
  String message;
  if (!moduleReady) {
    message = "<div class='alert alert-danger'>Module Error! Cannot apply settings.</div>";
  } else {
    if (server.args() > 0 && server.hasArg("frequency")) {
      frequency = server.arg("frequency");
      if (server.hasArg("channelSpacing")) channelSpacing = server.arg("channelSpacing");
      if (server.hasArg("txCtcss")) txCtcss = server.arg("txCtcss");
      if (server.hasArg("rxCtcss")) rxCtcss = server.arg("rxCtcss");

      String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;

      if (sendATCommand(groupCommand, "+DMOSETGROUP:0")) {
        message += "<div class='alert alert-success'>Settings Applied Successfully!</div>";
        message += "<div class='alert alert-info'>";
        message += "Frequency: <strong>" + frequency + " MHz</strong><br>";
        message += "Channel Spacing: <strong>" + String(channelSpacing == "0" ? "12.5KHz (Narrow)" : "25KHz (Wide)") + "</strong><br>";
        message += "CTCSS: <strong>" + txCtcss + "/" + rxCtcss + "</strong>";
        message += "</div>";
        updateOLED();
      } else {
        message = "<div class='alert alert-danger'>DRA818V Command Failed during configuration.</div>";
      }
    }
  }
  server.send(200, "text/html", message);
}

void handleRoot() {
  digitalWrite(led, HIGH);
  
  String moduleStatus = moduleReady ? 
      "<span class='badge badge-success'><i class='fas fa-check-circle'></i> READY</span>" : 
      "<span class='badge badge-danger'><i class='fas fa-exclamation-triangle'></i> ERROR</span>";
  
  String wifiStatus = wifi_connected ? 
      "<span class='badge badge-success'><i class='fas fa-wifi'></i> " + saved_ssid + "</span>" :
      "<span class='badge badge-warning'><i class='fas fa-broadcast-tower'></i> ACCESS POINT</span>";
      
  String powerStatus = powerState ? 
      "<span class='badge badge-success'><i class='fas fa-power-off'></i> ON</span>" :
      "<span class='badge badge-danger'><i class='fas fa-power-off'></i> OFF</span>";
      
  String page = "<!DOCTYPE HTML><html><head><title>HAM Radio Controller</title>"
               "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
               "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'>"
               "<meta name='viewport' content='width=device-width, initial-scale=1'>"
               "<style>"
               "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }"
               ".container { max-width: 600px; background: rgba(255,255,255,0.95); padding: 30px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); backdrop-filter: blur(10px); }"
               ".header { background: linear-gradient(135deg, #2c3e50, #34495e); color: white; padding: 20px; border-radius: 10px; margin-bottom: 25px; }"
               ".btn { border-radius: 25px; font-weight: 600; transition: all 0.3s ease; margin: 5px; }"
               ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }"
               ".btn-primary { background: linear-gradient(135deg, #3498db, #2980b9); border: none; }"
               ".btn-success { background: linear-gradient(135deg, #27ae60, #229954); border: none; }"
               ".btn-danger { background: linear-gradient(135deg, #e74c3c, #c0392b); border: none; }"
               ".btn-warning { background: linear-gradient(135deg, #f39c12, #e67e22); border: none; }"
               ".btn-info { background: linear-gradient(135deg, #17a2b8, #138496); border: none; }"
               ".btn-secondary { background: linear-gradient(135deg, #6c757d, #5a6268); border: none; }"
               ".card { border: none; border-radius: 15px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); margin-bottom: 20px; }"
               ".card-header { background: linear-gradient(135deg, #34495e, #2c3e50); color: white; border-radius: 15px 15px 0 0 !important; font-weight: 600; }"
               ".form-control { border-radius: 10px; border: 2px solid #e9ecef; transition: all 0.3s ease; }"
               ".form-control:focus { border-color: #3498db; box-shadow: 0 0 0 0.2rem rgba(52, 152, 219, 0.25); }"
               ".input-group-text { background: linear-gradient(135deg, #34495e, #2c3e50); color: white; border: none; border-radius: 10px; }"
               ".slider-container { background: #f8f9fa; padding: 15px; border-radius: 10px; margin: 10px 0; }"
               ".status-badge { font-size: 0.9em; padding: 8px 15px; border-radius: 20px; }"
               ".control-panel { background: linear-gradient(135deg, #ecf0f1, #bdc3c7); padding: 20px; border-radius: 15px; margin: 20px 0; }"
               ".frequency-display { font-size: 2.5em; font-weight: bold; color: #2c3e50; text-align: center; margin: 15px 0; }"
               ".ptt-btn { font-size: 1.5em; padding: 15px 30px; border-radius: 50px; }"
               ".real-time { background: #e8f5e8; border-left: 4px solid #27ae60; }"
               "</style>"
               "</head><body>"
               "<div class='container'>"
               
               "<div class='header text-center'>"
               "<h1><i class='fas fa-broadcast-tower'></i> HAM RADIO CONTROLLER</h1>"
               "<div class='row mt-3'>"
               "<div class='col-4'><div class='status-badge badge-info'>Module: " + moduleStatus + "</div></div>"
               "<div class='col-4'><div class='status-badge badge-primary'>Network: " + wifiStatus + "</div></div>"
               "<div class='col-4'><div class='status-badge " + (powerState ? "badge-success" : "badge-danger") + "'>Power: " + powerStatus + "</div></div>"
               "</div>"
               "</div>"

               "<div class='text-center mb-4'>"
               "<a href='/wifi-config' class='btn btn-outline-primary'><i class='fas fa-wifi'></i> WiFi</a>"
               "<a href='/channels' class='btn btn-outline-info'><i class='fas fa-list'></i> Channels</a>"
               "</div>"

               "<div class='frequency-display'>" + frequency + " MHz</div>"

               "<div class='control-panel text-center'>"
               "<div class='row'>"
               "<div class='col-4'>"
               "<button onclick='togglePTT()' class='btn " + (pttState ? "btn-danger" : "btn-outline-danger") + " ptt-btn'><i class='fas fa-microphone'></i><br>" + (pttState ? "TX" : "PTT") + "</button>"
               "</div>"
               "<div class='col-4'>"
               "<button onclick='togglePowerHL()' class='btn " + (txPower == "1" ? "btn-warning" : "btn-outline-warning") + "'><i class='fas fa-bolt'></i><br>PWR: " + (txPower == "1" ? "1W" : "0.5W") + "</button>"
               "</div>"
               "<div class='col-4'>"
               "<button onclick='togglePower()' class='btn " + (powerState ? "btn-success" : "btn-outline-secondary") + "'><i class='fas fa-power-off'></i><br>" + (powerState ? "ON" : "OFF") + "</button>"
               "</div>"
               "</div>"
               "</div>"

               "<form onsubmit='return submitForm();'>"
               
               "<div class='card'>"
               "<div class='card-header'><i class='fas fa-tune'></i> Frequency Settings</div>"
               "<div class='card-body'>"
               
               "<div class='form-group'>"
               "<label for='frequency'><i class='fas fa-wave-square'></i> Frequency (MHz)</label>"
               "<div class='input-group'>"
               "<input type='text' class='form-control' id='frequency' placeholder='145.6000' value='" + frequency + "'>"
               "<div class='input-group-append'>"
               "<button type='button' class='btn btn-success' onclick='changeFrequency(-1)'><i class='fas fa-minus'></i></button>"
               "<button type='button' class='btn btn-success' onclick='changeFrequency(1)'><i class='fas fa-plus'></i></button>"
               "</div></div></div>"
               
               "<div class='form-group'>"
               "<label for='step'><i class='fas fa-arrows-alt-h'></i> Frequency Step</label>"
               "<select class='form-control' id='step'>"
               "<option value='0.0025'>2.5 KHz</option>"
               "<option value='0.005'>5.0 KHz</option>"
               "<option value='0.00625'>6.25 KHz</option>"
               "<option value='0.01'>10.0 KHz</option>"
               "<option value='0.0125' selected>12.5 KHz</option>"
               "<option value='0.02'>20.0 KHz</option>"
               "<option value='0.025'>25.0 KHz</option>"
               "</select>"
               "</div>"
               
               "<div class='form-group'>"
               "<label for='channelSpacing'><i class='fas fa-compress-arrows-alt'></i> Channel Spacing</label>"
               "<select class='form-control' id='channelSpacing'>"
               "<option value='0' " + (channelSpacing == "0" ? "selected" : "") + ">12.5 KHz (Narrow)</option>"
               "<option value='1' " + (channelSpacing == "1" ? "selected" : "") + ">25 KHz (Wide)</option>"
               "</select>"
               "</div>"
               "</div></div>"

               "<div class='card real-time'>"
               "<div class='card-header'><i class='fas fa-sliders-h'></i> Real-time Audio Settings</div>"
               "<div class='card-body'>"
               
               "<div class='slider-container'>"
               "<label for='squelch'><i class='fas fa-volume-mute'></i> Squelch Level: <span id='squelchOutput'>" + squelch + "</span></label>"
               "<input type='range' class='custom-range' id='squelch' min='0' max='8' value='" + squelch + "' oninput='updateSquelch(this.value)'>"
               "<div class='d-flex justify-content-between'>"
               "<small>0 (Open)</small>"
               "<small>8 (Tight)</small>"
               "</div>"
               "</div>"
               
               "<div class='slider-container'>"
               "<label for='volume'><i class='fas fa-volume-up'></i> Volume Level: <span id='volumeOutput'>" + volume + "</span></label>"
               "<input type='range' class='custom-range' id='volume' min='1' max='8' value='" + volume + "' oninput='updateVolume(this.value)'>"
               "<div class='d-flex justify-content-between'>"
               "<small>1 (Low)</small>"
               "<small>8 (High)</small>"
               "</div>"
               "</div>"
               "</div></div>"

               "<div class='card'>"
               "<div class='card-header'><i class='fas fa-code'></i> CTCSS Settings</div>"
               "<div class='card-body'>"
               "<div class='form-row'>"
               "<div class='form-group col-md-6'>"
               "<label for='txCtcss'><i class='fas fa-sign-out-alt'></i> TX CTCSS</label>"
               "<input type='text' class='form-control' id='txCtcss' placeholder='0000 for None' value='" + txCtcss + "'>"
               "</div>"
               "<div class='form-group col-md-6'>"
               "<label for='rxCtcss'><i class='fas fa-sign-in-alt'></i> RX CTCSS</label>"
               "<input type='text' class='form-control' id='rxCtcss' placeholder='0000 for None' value='" + rxCtcss + "'>"
               "</div></div>"
               "</div></div>"

               "<button type='submit' class='btn btn-primary btn-block btn-lg'><i class='fas fa-sync'></i> APPLY FREQUENCY SETTINGS</button>"
               "</form>"

               "<div class='card mt-4'>"
               "<div class='card-header'><i class='fas fa-save'></i> Save Channel</div>"
               "<div class='card-body'>"
               "<form onsubmit='return saveChannel();'>"
               "<div class='form-group'>"
               "<input type='text' class='form-control' id='channelName' placeholder='Enter channel name (e.g. Local Repeater)'>"
               "</div>"
               "<button type='submit' class='btn btn-success btn-block'><i class='fas fa-save'></i> SAVE CURRENT SETTINGS</button>"
               "</form>"
               "</div></div>"

               "<div id='response' class='mt-3'></div>"
               "</div>"

               "<script>"
               "function submitForm() {"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/submit', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      document.getElementById('response').innerHTML = xhr.responseText;"
               "      document.querySelector('.frequency-display').innerHTML = document.getElementById('frequency').value + ' MHz';"
               "    }"
               "  };"
               "  var frequency = document.getElementById('frequency').value;"
               "  var channelSpacing = document.getElementById('channelSpacing').value;"
               "  var txCtcss = document.getElementById('txCtcss').value;"
               "  var rxCtcss = document.getElementById('rxCtcss').value;"
               "  var params = 'frequency=' + frequency + '&channelSpacing=' + channelSpacing + '&txCtcss=' + txCtcss + '&rxCtcss=' + rxCtcss;"
               "  xhr.send(params);"
               "  return false;"
               "}"
               "function changeFrequency(direction) {"
               "  var freqInput = document.getElementById('frequency');"
               "  var frequency = parseFloat(freqInput.value);"
               "  var step = parseFloat(document.getElementById('step').value);"
               "  frequency += direction * step;"
               "  freqInput.value = frequency.toFixed(4);"
               "  updateFrequency(frequency.toFixed(4));"
               "}"
               "function updateFrequency(frequency) {"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/submit', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      document.getElementById('response').innerHTML = xhr.responseText;"
               "      document.querySelector('.frequency-display').innerHTML = frequency + ' MHz';"
               "    }"
               "  };"
               "  var channelSpacing = document.getElementById('channelSpacing').value;"
               "  var txCtcss = document.getElementById('txCtcss').value;"
               "  var rxCtcss = document.getElementById('rxCtcss').value;"
               "  var params = 'frequency=' + frequency + '&channelSpacing=' + channelSpacing + '&txCtcss=' + txCtcss + '&rxCtcss=' + rxCtcss;"
               "  xhr.send(params);"
               "}"
               "function updateVolume(value) {"
               "  document.getElementById('volumeOutput').innerHTML = value;"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/volume', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.send('value=' + value);"
               "}"
               "function updateSquelch(value) {"
               "  document.getElementById('squelchOutput').innerHTML = value;"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/squelch', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.send('value=' + value);"
               "}"
               "function saveChannel() {"
               "  var channelName = document.getElementById('channelName').value;"
               "  if (channelName.trim() === '') {"
               "    alert('Please enter a channel name');"
               "    return false;"
               "  }"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('POST', '/save-channel', true);"
               "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      document.getElementById('response').innerHTML = xhr.responseText;"
               "      document.getElementById('channelName').value = '';"
               "    }"
               "  };"
               "  var params = 'channelName=' + encodeURIComponent(channelName) + '&frequency=' + encodeURIComponent('" + frequency + "') + '&squelch=' + encodeURIComponent('" + squelch + "') + '&txCtcss=' + encodeURIComponent('" + txCtcss + "') + '&rxCtcss=' + encodeURIComponent('" + rxCtcss + "') + '&channelSpacing=' + encodeURIComponent('" + channelSpacing + "');"
               "  xhr.send(params);"
               "  return false;"
               "}"
               "function togglePTT() {"
               "  var btn = event.target.closest('button');"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('GET', '/ptt', true);"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      var isActive = xhr.responseText.includes('ACTIVE');"
               "      btn.className = isActive ? 'btn btn-danger ptt-btn' : 'btn btn-outline-danger ptt-btn';"
               "      btn.innerHTML = isActive ? '<i class=\"fas fa-microphone\"></i><br>TX' : '<i class=\"fas fa-microphone\"></i><br>PTT';"
               "    }"
               "  };"
               "  xhr.send();"
               "}"
               "function togglePower() {"
               "  var btn = event.target.closest('button');"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('GET', '/power', true);"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      var isOn = xhr.responseText.includes('ON');"
               "      btn.className = isOn ? 'btn btn-success' : 'btn btn-outline-secondary';"
               "      btn.innerHTML = isOn ? '<i class=\"fas fa-power-off\"></i><br>ON' : '<i class=\"fas fa-power-off\"></i><br>OFF';"
               "      location.reload();"
               "    }"
               "  };"
               "  xhr.send();"
               "}"
               "function togglePowerHL() {"
               "  var btn = event.target.closest('button');"
               "  var xhr = new XMLHttpRequest();"
               "  xhr.open('GET', '/powerHL', true);"
               "  xhr.onreadystatechange = function() {"
               "    if (xhr.readyState == 4 && xhr.status == 200) {"
               "      var isHigh = xhr.responseText.includes('HIGH');"
               "      btn.className = isHigh ? 'btn btn-warning' : 'btn btn-outline-warning';"
               "      btn.innerHTML = isHigh ? '<i class=\"fas fa-bolt\"></i><br>1W' : '<i class=\"fas fa-bolt\"></i><br>0.5W';"
               "    }"
               "  };"
               "  xhr.send();"
               "}"
               "</script></body></html>";
  server.send(200, "text/html", page);
  digitalWrite(led, LOW);
}

void handleNotFound() {
  digitalWrite(led, HIGH);
  String message = "404 Not Found\n\n";
  message += "URI: " + server.uri();
  server.send(404, "text/plain", message);
  digitalWrite(led, LOW);
}

void testCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    draSerial.println(command);
  }
  if (draSerial.available() > 0) {
    String response = draSerial.readString();
    Serial.println("Module Response: " + response);
  }
}