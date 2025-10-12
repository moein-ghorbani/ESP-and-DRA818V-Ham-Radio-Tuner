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
String channelSpacing = "0";
String txCtcss = "0000";
String rxCtcss = "0000";

String scanStartFreq = "144.0000";
String scanStopFreq = "146.0000";
String scanStep = "0.0125";

ESP8266WebServer server(80);
SoftwareSerial softSerial(5, 4);
const int led = 13;
const int pttPin = 2;
const int powerPin = 0;
const int powerHLPin = 16;

bool pttState = false;
bool powerState = true;
String txPower = "1";

void writeString(String string);
void sendATCommand(String command, String expectedResponse);
void handleRoot();
void handleSubmit();
void handlePTT();
void handlePower();
void handlePowerHL();
String scanFrequencyBand(float startFreq, float stopFreq, float step);
void handleScan(); 
void handleNotFound();
void testCommand();

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(pttPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
  pinMode(powerHLPin, OUTPUT);
  
  digitalWrite(led, 0);
  digitalWrite(pttPin, HIGH);
  digitalWrite(powerPin, HIGH);
  digitalWrite(powerHLPin, HIGH);
  
  Serial.begin(9600);
  softSerial.begin(9600);

  IPAddress ip(192,168,1,35);
  IPAddress subnet(255,255,255,0);
  
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(ip, ip, subnet);
  Serial.println("\nConnected to " + String(ssid));
  Serial.println("IP address: " + WiFi.softAPIP().toString());
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  Serial.println("Sending Handshake Command to DRA818V...");
  sendATCommand("AT+DMOCONNECT", "+DMOCONNECT:0"); 

  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.on("/ptt", handlePTT);
  server.on("/power", handlePower);
  server.on("/powerHL", handlePowerHL);
  server.on("/scan", handleScan);
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
    response += (char)softSerial.read();
  }
  Serial.print("AT Command: ");
  Serial.println(command);
  Serial.print("Response: ");
  Serial.println(response);
}

void handlePTT() {
  pttState = !pttState;
  digitalWrite(pttPin, pttState ? LOW : HIGH);
  server.send(200, "text/plain", String(pttState ? "Pressed (TX)" : "Released (RX)"));
}

void handlePower() {
  powerState = !powerState;
  digitalWrite(powerPin, powerState ? HIGH : LOW);
  server.send(200, "text/plain", String(powerState ? "On" : "Off"));
}

void handlePowerHL() {
  if (txPower == "1") {
    txPower = "0";
    digitalWrite(powerHLPin, LOW);
  } else {
    txPower = "1";
    digitalWrite(powerHLPin, HIGH);
  }
  
  server.send(200, "text/plain", String(txPower == "1" ? "1W (High)" : "0.5W (Low)"));
}

String scanFrequencyBand(float startFreq, float stopFreq, float step) {
  if (squelch == "0") {
      return "<span class='text-danger'>ERROR: Squelch must be &gt; 0 (Monitor Mode is not supported for scanning).</span>";
  }
  if (startFreq >= stopFreq) {
    return "<span class='text-danger'>ERROR: Start Frequency must be less than Stop Frequency.</span>";
  }
  if (startFreq < 134.0000 || stopFreq > 174.0000) {
      return "<span class='text-danger'>ERROR: Frequency out of supported range (134.0000 - 174.0000 MHz).</span>";
  }

  String foundFreq = "";
  for (float freq = startFreq; freq <= stopFreq; freq += step) {
    String freqStr = String(freq, 4);
    String command = "S+" + freqStr;
    softSerial.println(command);
    delay(50);

    String response = "";
    while (softSerial.available() > 0) {
      response += (char)softSerial.read();
    }

    Serial.print("Scanning: ");
    Serial.print(freqStr);
    Serial.print(", Response: ");
    Serial.println(response);

    if (response.indexOf("S=0") != -1) {
      foundFreq = freqStr;
      frequency = foundFreq;
      
      String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;
      sendATCommand(groupCommand, "+DMOSETGROUP:0");
      
      return "<span class='text-success'><i class='fas fa-microphone'></i> **Signal Found!** Frequency: <a href='#' onclick='document.getElementById(\"frequency\").value=\"" + foundFreq + "\";submitForm();'>" + foundFreq + " MHz</a></span>";
    }
    server.handleClient();
  }

  return "<span class='text-info'><i class='fas fa-times-circle'></i> Scan Complete. No signal found in the specified range.</span>";
}

void handleScan() {
  String message = "";
  if (server.args() > 0) {
    String startStr = server.arg("start");
    String stopStr = server.arg("stop");
    String stepStr = server.arg("step");

    float startFreq = startStr.toFloat();
    float stopFreq = stopStr.toFloat();
    float step = stepStr.toFloat();
    scanStartFreq = startStr;
    scanStopFreq = stopStr;
    scanStep = stepStr;
    
    message = scanFrequencyBand(startFreq, stopFreq, step);
  } else {
    message = "<span class='text-danger'>Error: Missing scan parameters.</span>";
  }
  server.send(200, "text/html", message);
}

void handleSubmit() {
  if (server.args() > 0 && server.hasArg("frequency")) {
    frequency = server.arg("frequency");
    squelch = server.arg("squelch");
    volume = server.arg("volume");
    if (server.hasArg("channelSpacing")) channelSpacing = server.arg("channelSpacing");
    if (server.hasArg("txCtcss")) txCtcss = server.arg("txCtcss");
    if (server.hasArg("rxCtcss")) rxCtcss = server.arg("rxCtcss");

    String groupCommand = "AT+DMOSETGROUP=" + channelSpacing + "," + frequency + "," + frequency + "," + txCtcss + "," + squelch + "," + rxCtcss;

    sendATCommand(groupCommand, "+DMOSETGROUP:0");
    sendATCommand("AT+DMOSETVOLUME=" + volume, "+DMOSETVOLUME:0");
  }
  String message;
  message += "<h5><i class='fas fa-check-circle'></i> Settings Applied Successfully!</h5>";
  message += "Frequency set to: **" + frequency + "**<br>";
  message += "Channel Spacing: **" + String(channelSpacing == "0" ? "12.5KHz (Narrow)" : "25KHz (Wide)") + "**<br>";
  message += "Squelch set to: **" + squelch + "**<br>";
  message += "Volume set to: **" + volume + "**<br>";
  message += "TX/RX CTCSS set to: **" + txCtcss + "/" + rxCtcss + "**<br>";
  server.send(200, "text/html", message);
}

void handleRoot() {
  digitalWrite(led, 1);
  String page = "<!DOCTYPE HTML><html><head><title>Ham Radio Tuner</title>"
                "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
                "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'>"
                "<style>"
                "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f8f9fa; }" 
                ".container { max-width: 550px; background: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }"
                ".btn { margin: 5px; min-width: 100px; }"
                ".status-label { font-weight: bold; margin-left: 10px; }"
                "</style>"
                "</head><body>"
                "<div class='container'>"
                " <h2 class='text-center mb-4'><i class='fas fa-broadcast-tower'></i> DRA818V Tuner</h2>"
                "<form onsubmit='return submitForm();'>"
                
                "<div class='form-group'>"
                "<label for='frequency'><i class='fas fa-wave-square'></i> Frequency (MHz)</label>"
                "<div class='input-group'>"
                "<input type='text' class='form-control' id='frequency' placeholder='e.g. 145.6000' value='" + frequency + "'>"
                "<div class='input-group-append'>"
                "<button type='button' class='btn btn-success' onclick='changeFrequency(-1)'><i class='fas fa-minus'></i></button>"
                "<button type='button' class='btn btn-success' onclick='changeFrequency(1)'><i class='fas fa-plus'></i></button>"
                "</div></div></div>"
                
                "<div class='form-group'>"
                "<label for='step'>Frequency Step (KHz)</label>"
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
                "<label for='channelSpacing'><i class='fas fa-arrows-alt-h'></i> Channel Spacing</label>"
                "<select class='form-control' id='channelSpacing'>"
                "<option value='0' " + (channelSpacing == "0" ? "selected" : "") + ">12.5 KHz (Narrow)</option>"
                "<option value='1' " + (channelSpacing == "1" ? "selected" : "") + ">25 KHz (Wide)</option>"
                "</select>"
                "</div>"

                "<div class='form-row'>"
                "<div class='form-group col-md-6'>"
                "<label for='squelch'><i class='fas fa-volume-mute'></i> Squelch Level (0-8)</label>"
                "<input type='number' class='form-control' id='squelch' min='0' max='8' placeholder='e.g. 1' value='" + squelch + "'>"
                "</div>"
                "<div class='form-group col-md-6'>"
                "<label for='volume'><i class='fas fa-volume-up'></i> Volume (1-8)</label>"
                "<input type='range' class='custom-range' id='volume' min='1' max='8' value='" + volume + "' oninput='document.getElementById(\"volumeOutput\").innerHTML = this.value;'>"
                "<span id='volumeOutput' class='status-label'>" + volume + "</span>"
                "</div></div>"
                
                "<div class='form-row'>"
                "<div class='form-group col-md-6'>"
                "<label for='txCtcss'><i class='fas fa-sign-out-alt'></i> TX CTCSS (e.g. 100.0 or 0000)</label>"
                "<input type='text' class='form-control' id='txCtcss' placeholder='0000 for None' value='" + txCtcss + "'>"
                "</div>"
                "<div class='form-group col-md-6'>"
                "<label for='rxCtcss'><i class='fas fa-sign-in-alt'></i> RX CTCSS (e.g. 100.0 or 0000)</label>"
                "<input type='text' class='form-control' id='rxCtcss' placeholder='0000 for None' value='" + rxCtcss + "'>"
                "</div></div>"

                "<button type='submit' class='btn btn-primary btn-block mb-3'><i class='fas fa-sync'></i> Apply Settings (Tune)</button>"
                "</form>"

                "<hr>"
                "<div class='card mt-4'>"
                "<div class='card-header'><i class='fas fa-search'></i> Frequency Scan</div>"
                "<div class='card-body'>"
                "<form onsubmit='return startScan();'>"
                "<div class='form-row'>"
                "<div class='form-group col-md-4'>"
                "<label for='scanStartFreq'>Start (MHz)</label>"
                "<input type='text' class='form-control' id='scanStartFreq' value='" + scanStartFreq + "'>"
                "</div>"
                "<div class='form-group col-md-4'>"
                "<label for='scanStopFreq'>Stop (MHz)</label>"
                "<input type='text' class='form-control' id='scanStopFreq' value='" + scanStopFreq + "'>"
                "</div>"
                "<div class='form-group col-md-4'>"
                "<label for='scanStep'>Step (KHz)</label>"
                "<select class='form-control' id='scanStep'>"
                "<option value='0.00625'>6.25</option>"
                "<option value='0.0125' " + (scanStep == "0.0125" ? "selected" : "") + ">12.5</option>"
                "<option value='0.025' " + (scanStep == "0.025" ? "selected" : "") + ">25.0</option>"
                "</select>"
                "</div>"
                "</div>"
                "<button type='submit' class='btn btn-warning btn-block'><i class='fas fa-eye'></i> Start Scan</button>"
                "</form>"
                "<div id='scanResponse' class='alert alert-light mt-3' role='alert'>Scan Result: Ready.</div>"
                "</div></div>"

                "<hr>"
                "<div class='d-flex justify-content-between flex-wrap'>"
                
                "<button onclick='togglePTT()' class='btn btn-danger mb-2'><i class='fas fa-microphone-alt'></i> PTT</button>"
                "<span id='pttStatus' class='status-label alert alert-secondary p-2'>" + String(pttState ? "Pressed (TX)" : "Released (RX)") + "</span>"

                "<button onclick='togglePowerHL()' class='btn btn-info mb-2'><i class='fas fa-bolt'></i> TX Power</button>"
                "<span id='powerHLStatus' class='status-label alert " + String(txPower == "1" ? "alert-success" : "alert-warning") + " p-2'>" + String(txPower == "1" ? "1W (High)" : "0.5W (Low)") + "</span>"

                "<button onclick='togglePower()' class='btn btn-dark mb-2'><i class='fas fa-power-off'></i> Module Power</button>"
                "<span id='powerStatus' class='status-label alert " + String(powerState ? "alert-success" : "alert-danger") + " p-2'>" + String(powerState ? "On" : "Off") + "</span>"
                
                "</div>"
                
                "<div id='response' class='alert alert-info mt-3' role='alert'></div>"
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
                "  var channelSpacing = document.getElementById('channelSpacing').value;"
                "  var txCtcss = document.getElementById('txCtcss').value;"
                "  var rxCtcss = document.getElementById('rxCtcss').value;"

                "  var params = 'frequency=' + frequency + '&squelch=' + squelch + '&volume=' + volume + " + 
                "               '&channelSpacing=' + channelSpacing + '&txCtcss=' + txCtcss + '&rxCtcss=' + rxCtcss;"
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
                "    }"
                "  };"
                "  var squelch = document.getElementById('squelch').value;"
                "  var volume = document.getElementById('volume').value;"
                "  var channelSpacing = document.getElementById('channelSpacing').value;"
                "  var txCtcss = document.getElementById('txCtcss').value;"
                "  var rxCtcss = document.getElementById('rxCtcss').value;"
                "  var params = 'frequency=' + frequency + '&squelch=' + squelch + '&volume=' + volume + " + 
                "               '&channelSpacing=' + channelSpacing + '&txCtcss=' + txCtcss + '&rxCtcss=' + rxCtcss;"
                "  xhr.send(params);"
                "}"
                "function togglePTT() {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('GET', '/ptt', true);"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('pttStatus').innerHTML = xhr.responseText;"
                "      document.getElementById('pttStatus').classList.toggle('alert-success', xhr.responseText.includes('Pressed'));"
                "      document.getElementById('pttStatus').classList.toggle('alert-secondary', xhr.responseText.includes('Released'));"
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
                "      document.getElementById('powerStatus').classList.toggle('alert-success', xhr.responseText.includes('On'));"
                "      document.getElementById('powerStatus').classList.toggle('alert-danger', xhr.responseText.includes('Off'));"
                "    }"
                "  };"
                "  xhr.send();"
                "}"
                "function togglePowerHL() {"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('GET', '/powerHL', true);"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('powerHLStatus').innerHTML = xhr.responseText;"
                "      document.getElementById('powerHLStatus').classList.toggle('alert-success', xhr.responseText.includes('1W'));"
                "      document.getElementById('powerHLStatus').classList.toggle('alert-warning', xhr.responseText.includes('0.5W'));"
                "    }"
                "  };"
                "  xhr.send();"
                "}"
                "function startScan() {"
                "  var currentSquelch = document.getElementById('squelch').value;"
                "  if (currentSquelch == '0') {"
                "    document.getElementById('scanResponse').innerHTML = '<span class=\"text-danger\"><i class=\"fas fa-exclamation-triangle\"></i> ERROR: Squelch must be &gt; 0 to scan.</span>';"
                "    return false;"
                "  }"
                "  document.getElementById('scanResponse').innerHTML = 'Scan Result: <i class=\"fas fa-spinner fa-spin\"></i> Scanning...';"
                "  var xhr = new XMLHttpRequest();"
                "  xhr.open('POST', '/scan', true);"
                "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
                "  xhr.onreadystatechange = function() {"
                "    if (xhr.readyState == 4 && xhr.status == 200) {"
                "      document.getElementById('scanResponse').innerHTML = 'Scan Result: ' + xhr.responseText;"
                "    }"
                "  };"
                "  var startFreq = document.getElementById('scanStartFreq').value;"
                "  var stopFreq = document.getElementById('scanStopFreq').value;"
                "  var step = document.getElementById('scanStep').value;"
                "  var params = 'start=' + startFreq + '&stop=' + stopFreq + '&step=' + step;"
                "  xhr.send(params);"
                "  return false;"
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

void testCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    softSerial.println(command);
  }
  if (softSerial.available() > 0) {
    String response = softSerial.readString();
    Serial.println("Module Response: " + response);
  }
}