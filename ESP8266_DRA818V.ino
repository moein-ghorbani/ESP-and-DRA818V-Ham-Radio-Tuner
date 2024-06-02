#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

const char* ssid = "HAM_NODE";
const char* password = "";
String frequency = "145.6000";
String squelch = "1";
String volume = "4";
ESP8266WebServer server(80);
SoftwareSerial softSerial(5, 4); // RX -> GPIO 5 (D1), TX -> GPIO 4 (D2)
const int led = 13;
const int pttPin = 2; // GPIO 2 (D4)
const int powerPin = 0; // GPIO 0 (D3)

bool pttState = false;
bool powerState = true;

void writeString(String string);
float mapFl(int x, float in_min, float in_max, float out_min, float out_max);
void sendATCommand(String command, String expectedResponse);

void handleRoot() {
  digitalWrite(led, 1);
  String page = "<!DOCTYPE HTML><html><head><title>Ham tuner</title>"
                "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
                "<style>body { font-family: Arial, sans-serif; margin: 20px; } .container { max-width: 500px; } .btn { margin-top: 10px; }</style>"
                "</head><body>"
                "<div class='container'>"
                "<h2>Ham Radio Tuner</h2>"
                "<form onsubmit='return submitForm();'>"
                "<div class='form-group'>"
                "<label for='frequency'>Frequency</label>"
                "<input type='text' class='form-control' id='frequency' placeholder='e.g. 145.6000' value='" + frequency + "'>"
                "<button type='button' class='btn btn-primary' onclick='changeFrequency(-1)'>-</button>"
                "<button type='button' class='btn btn-primary' onclick='changeFrequency(1)'>+</button>"
                "</div>"
                "<div class='form-group'>"
                "<label for='step'>Frequency Step</label>"
                "<select class='form-control' id='step'>"
                "<option value='0.0025'>2.5Khz</option>"
                "<option value='0.005'>5.0Khz</option>"
                "<option value='0.00625'>6.25Khz</option>"
                "<option value='0.01'>10.0Khz</option>"
                "<option value='0.0125'>12.5Khz</option>"
                "<option value='0.02'>20.0Khz</option>"
                "<option value='0.025'>25.0Khz</option>"
                "<option value='0.05'>50.0Khz</option>"
                "</select>"
                "</div>"
                "<div class='form-group'>"
                "<label for='squelch'>Squelch Level</label>"
                "<input type='text' class='form-control' id='squelch' placeholder='e.g. 1' value='" + squelch + "'>"
                "</div>"
                "<div class='form-group'>"
                "<label for='volume'>Volume</label>"
                "<input type='range' class='custom-range' id='volume' min='1' max='8' value='" + volume + "' oninput='document.getElementById(\"volumeOutput\").innerHTML = this.value;'>"
                "<span id='volumeOutput'>" + volume + "</span>"
                "</div>"
                "<button type='submit' class='btn btn-primary'>Tune</button>"
                "</form>"
                "<button onclick='togglePTT()' class='btn btn-secondary'>PTT</button> <span id='pttStatus'>" + String(pttState ? "Pressed" : "Released") + "</span><br>"
                "<button onclick='togglePower()' class='btn btn-secondary'>Power</button> <span id='powerStatus'>" + String(powerState ? "On" : "Off") + "</span><br>"
                "<div id='response'></div>"
                "</div>"
                "<script>"
                "function submitForm() {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('POST', '/submit', true);"
                "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('response').innerHTML = xhr.responseText;"
                "    }"
                "  };"
                "  var frequency = document.getElementById('frequency').value;"
                "  var squelch = document.getElementById('squelch').value;"
                "  var volume = document.getElementById('volume').value;"
                "  xhr.send('frequency=' + frequency + '&squelch=' + squelch + '&volume=' + volume);"
                "  return false;"
                "}"
                "function changeFrequency(direction) {"
                "  var frequency = parseFloat(document.getElementById('frequency').value);"
                "  var step = parseFloat(document.getElementById('step').value);"
                "  frequency += direction * step;"
                "  document.getElementById('frequency').value = frequency.toFixed(4);"
                "  updateFrequency(frequency.toFixed(4));"
                "}"
                "function updateFrequency(frequency) {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('POST', '/submit', true);"
                "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('response').innerHTML = xhr.responseText;"
                "    }"
                "  };"
                "  var squelch = document.getElementById('squelch').value;"
                "  var volume = document.getElementById('volume').value;"
                "  xhr.send('frequency=' + frequency + '&squelch=' + squelch + '&volume=' + volume);"
                "}"
                "function togglePTT() {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('GET', '/ptt', true);"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('pttStatus').innerHTML = xhr.responseText;"
                "    }"
                "  };"
                "  xhr.send();"
                "}"
                "function togglePower() {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('GET', '/power', true);"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('powerStatus').innerHTML = xhr.responseText;"
                "    }"
                "  };"
                "  xhr.send();"
                "}"
                "</script></body></html>";
  server.send(200, "text/html", page);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void handleSubmit() {
  String rx;
  if (server.args() > 0) {
    if (server.hasArg("frequency")) {
      frequency = server.arg("frequency");
      squelch = server.arg("squelch");
      volume = server.arg("volume");

      Serial.println("Sending to DRA818V: AT+DMOSETGROUP=1," + frequency + "," + frequency + ",0000," + squelch + ",0000");
      Serial.println("Sending to DRA818V: AT+DMOSETVOLUME=" + volume);

      sendATCommand("AT+DMOSETGROUP=1," + frequency + "," + frequency + ",0000," + squelch + ",0000", "+DMOSETGROUP:0");
      sendATCommand("AT+DMOSETVOLUME=" + volume, "+DMOSETVOLUME:0");

      delay(100);
      if (softSerial.available() > 0) {
        rx = softSerial.readString();
        Serial.println("Response from DRA818V: " + rx);
      } else {
        Serial.println("No response from DRA818V");
      }
    }
  }
  digitalWrite(led, 1);
  String message;
  message += "Frequency set to: " + frequency + "<br>";
  message += "Squelch level set to: " + squelch + "<br>";
  message += "Volume set to: " + volume + "<br>";
  digitalWrite(led, 0);
  server.send(200, "text/html", message);
}

void handlePTT() {
  pttState = !pttState;
  digitalWrite(pttPin, pttState ? LOW : HIGH);
  server.send(200, "text/plain", String(pttState ? "Pressed" : "Released"));
}

void handlePower() {
  powerState = !powerState;
  digitalWrite(powerPin, powerState ? HIGH : LOW);
  server.send(200, "text/plain", String(powerState ? "On" : "Off"));
}

void testCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    softSerial.println(command);
    delay(100);
    if (softSerial.available() > 0) {
      String response = softSerial.readString();
      Serial.println("Response from DRA818V: " + response);
    } else {
      Serial.println("No response from DRA818V");
    }
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(pttPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  digitalWrite(led, 0);
  digitalWrite(pttPin, HIGH);
  digitalWrite(powerPin, HIGH); // Initial power state is ON
  Serial.begin(9600);
  softSerial.begin(9600);

  IPAddress ip(192,168,1,35);
  IPAddress subnet(255,255,255,0);
  
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(ip, ip, subnet);
  Serial.println("");

  Serial.println("Connected to " + String(ssid));
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.on("/ptt", handlePTT);
  server.on("/power", handlePower);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  testCommand();
}

void writeString(String string) {
  for (int i = 0; i < string.length(); i++) {
    softSerial.write(string[i]);
  }
}

void sendATCommand(String command, String expectedResponse) {
  softSerial.println(command);
  delay(100);
  String response = "";
  while (softSerial.available() > 0) {
    response += char(softSerial.read());
  }
  if (response.indexOf(expectedResponse) != -1) {
    Serial.println("Command succeeded: " + command);
  } else {
    Serial.println("Command failed: " + command);
    Serial.println("Response: " + response);
  }
}

float mapFl(int x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - out_min) + out_min;
}
