#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RTClib.h>
#include "DHTesp.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h> // Убедитесь, что библиотека WiFiManager установлена через менеджер библиотек Arduino IDE.
#include <Wire.h>

// Relay pins
#define RELAY_IN1 D7 // Window open
#define RELAY_IN2 D8 // Window close
#define RELAY_IN6 D3 // Warm
#define RELAY_IN4 D4 // Irrigation
#define RELAY_IN3 D0 //  Replace analog sensor

#define RELAY_IN5 3 // Light

// Flow meter
const byte Interrupt_Pin = 1;
volatile uint16_t count_imp;
float count_imp_all;
uint16_t requiredMilliliters = 200;

bool is_irrigation_on = false;
bool is_light_on = false;
bool is_warm_on = false;
bool is_window_on = false;
bool WarmManualControl = false;
bool LightManualControl = false;
bool WindowManualControl = false;

int lightThreshold = 200; // Default threshold value
int lightOffThreshold = 100;
float warmThreshold = 20; // Default threshold value
float warmOffThreshold = 10;
float windowThreshold = 30; // Default threshold value
float windowOffThreshold = 25;

const unsigned long windowOpenDuration = 30000; // Window open duration (in milliseconds)
const unsigned long windowCloseDuration = 30000; // Window close duration (in milliseconds)

unsigned long windowStartTime = 0;
bool windowOpening = false;
bool windowClosing = false;

int ground_hum = 0;

RTC_DS3231 rtc;
ESP8266WebServer server(80);

const int oneWireBus = D6;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

DHTesp dht;

int irrigationStartTimeHour1 = 8;    // Час первой активации полива
int irrigationStartTimeMinute1 = 0;  // Минута первой активации полива
int irrigationStartTimeHour2 = 16;   // Час второй активации полива
int irrigationStartTimeMinute2 = 0;  // Минута второй активации полива

void saveConfigCallback() {
  Serial.println("Save config callback!");
}

void ICACHE_RAM_ATTR getFlow()
{
  count_imp++;
  if (count_imp_all >= requiredMilliliters * 2)
  {
    Serial.println("Полив закончился!!!");
    digitalWrite(RELAY_IN4, HIGH);
    count_imp_all = 0;
    count_imp = 0;
    is_irrigation_on = false;
  }
}

void handleRoot()
{
  String html = "<html><body>";
  html += "<h1>Irrigation Control</h1>";
  html += "<p>Enter the required milliliters:</p>";
  html += "<form id='setForm' method='POST'>";
  html += "<input type='number' name='milliliters' min='1' value='" + String(requiredMilliliters*5) + "'><br>";
  html += "<input type='button' value='Set' onclick='setMilliliters()'>";
  html += "</form>";
  html += "<p>Enter irrigation start times:</p>";
  html += "<form id='setTimeForm' method='POST'>";
  html += "1st Activation - Hour: <input type='number' name='startHour1' min='0' max='23' value='" + String(irrigationStartTimeHour1) + "'><br>";
  html += "1st Activation - Minute: <input type='number' name='startMinute1' min='0' max='59' value='" + String(irrigationStartTimeMinute1) + "'><br>";
  html += "2nd Activation - Hour: <input type='number' name='startHour2' min='0' max='23' value='" + String(irrigationStartTimeHour2) + "'><br>";
  html += "2nd Activation - Minute: <input type='number' name='startMinute2' min='0' max='59' value='" + String(irrigationStartTimeMinute2) + "'><br>";
  html += "<input type='button' value='Set Times' onclick='setIrrigationTimes()'>";
  html += "</form>";

  html += "<p>Status: ";
  if (is_irrigation_on) {
    html += "<div style = 'color:green'>Irrigation is running</div>";
  } else {
    html += "<div style = 'color:red'>Irrigation is not running</div>";
  }
  html += "</p>";

  html += "<button type='button' onclick='startPoliv()'>Start Irrigation</button>";
  html += "<button type='button' onclick='stopPoliv()'>Stop Irrigation</button>";
  DateTime now = rtc.now();
  html += "<p>Light Sensor Value: " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "</p>";
  html += "<hr>";

  html += "<h1>Light Control</h1>";

  html += "<p>Include when the value is less...(On)</p>";
  html += "<form id='setThresholdForm' method='POST'>";
  html += "<input type='number' name='threshold' value='" + String(lightThreshold) + "'><br>";
  html += "<input type='button' value='Set Threshold' onclick='setThreshold()'>";
  html += "</form>";

  html += "<p>Turn off when the value is greater...(Off):</p>";
  html += "<form id='setOffThresholdForm' method='POST'>";
  html += "<input type='number' name='offThreshold' value='" + String(lightOffThreshold) + "'><br>";
  html += "<input type='button' value='Set Off Threshold' onclick='setOffThreshold()'>";
  html += "</form>";

  html += "<p>Light Status: ";
  if (is_light_on) {
    html += "<div style = 'color:green'>On</div>";
  } else {
    html += "<div style = 'color:red'>Off</div>";
  }
  html += "</p>";

  int lightSensorValue = analogRead(A0);
  html += "<p>Light Sensor Value: " + String(lightSensorValue) + "</p>";

  html += "<button type='button' onclick='startLight()'>Turn On Light</button>";
  html += "<button type='button' onclick='stopLight()'>Turn Off Light</button>";
  html += "<button type='button' onclick='resetManualControl()'>Reset Manual Control</button>" + String(LightManualControl);
  html += "<hr>";


  html += "<h1>Warm Control</h1>";

  html += "<p>Include when the value is less...(On)</p>";
  html += "<form id='setThresholdFormWarm' method='POST'>";
  html += "<input type='number' name='thresholdWarm' value='" + String(warmThreshold) + "'><br>";
  html += "<input type='button' value='Set Threshold Warm' onclick='setThresholdWarm()'>";
  html += "</form>";

  html += "<p>Turn off when the value is greater...(Off):</p>";
  html += "<form id='setOffThresholdFormWarm' method='POST'>";
  html += "<input type='number' name='offThresholdWarm' value='" + String(warmOffThreshold) + "'>";
  html += "<input type='button' value='Set Off Threshold Warm' onclick='setOffThresholdWarm()'>";
  html += "</form>";

  html += "<p>Warm Status: ";
  if (is_warm_on) {
    html += "<div style = 'color:green'>On</div>";
  } else {
    html += "<div style = 'color:red'>Off</div>";
  }
  html += "</p>";

  sensors.requestTemperatures();
  float warmSensorValue = sensors.getTempCByIndex(0);
  html += "<p>Temp Sensor Value: " + String(warmSensorValue) + "</p>";

  html += "<button type='button' onclick='startWarm()'>Turn On Warm</button>";
  html += "<button type='button' onclick='stopWarm()'>Turn Off Warm</button>";
  html += "<button type='button' onclick='resetManualControlWarm()'>Reset Manual Control Warm</button>"+String(WarmManualControl); 
  html += "<hr>";

  html += "<h1>Window Control</h1>";
  html += "<p>Include when the value is less...(open)</p>";
  html += "<form id='setThresholdFormWindow' method='POST'>";
  html += "<input type='number' name='thresholdWindow' value='" + String(windowThreshold) + "'><br>";
  html += "<input type='button' value='Set Threshold Window' onclick='setThresholdWindow()'>";
  html += "</form>";

  html += "<p>Turn off when the value is greater...(close):</p>";
  html += "<form id='setOffThresholdFormWindow' method='POST'>";
  html += "<input type='number' name='offThresholdWindow' value='" + String(windowOffThreshold) + "'>";
  html += "<input type='button' value='Set Off Threshold Window' onclick='setOffThresholdWindow()'>";
  html += "</form>";
  html += "<p>Status: ";
  if (is_window_on) {
    html += "<div style = 'color:green'>Window open</div>";
  } else {
    html += "<div style = 'color:red'>Window close</div>";
  }
  html += "</p>";

  html += "<button type='button' onclick='openWindow()'>Open Window</button>";
  html += "<button type='button' onclick='closeWindow()'>Close Window</button>";
  html += "<button type='button' onclick='resetManualControlWindow()'>Reset Manual Control Window</button>"+String(WindowManualControl);
  float humidity = dht.getHumidity();
  float dht_temperature = dht.getTemperature();
  html += "<p>Temp Sensor Value: " + String(warmSensorValue) + "</p>";
  html += "<p>Humidity Sensor Value: " + String(humidity) + "</p>";
  
  html += "<hr>";
  html += "<p>Enter current time:</p>";
  html += "<form id='setRTCTimeForm' method='POST'>";
  html += "Hour: <input type='number' name='currentHour' min='0' max='23'><br>";
  html += "Minute: <input type='number' name='currentMinute' min='0' max='59'><br>";
  html += "<input type='button' value='Set Time' onclick='setCurrentTime()'>";
  html += "</form>";
  html += "<hr>";

  html += "<h1>Indications</h1>";

  html += "<p>Ground Humidity Sensor Value: " + String(ground_hum) + "</p>";
  html += "<p>Temperature in the house: " + String(warmSensorValue) + "</p>";
  html += "<p>Outdoor temperature: " + String(dht_temperature) + "</p>";
  html += "<p>Humidity Sensor Value: " + String(humidity) + "</p>";
  html += "<p>Light Sensor Value: " + String(lightSensorValue) + "</p>";
  html += "<hr>";
  
  html += "<button type='button' onclick='reconnect()'>Reconnect WIFI</button>";
  html += "<script>";
  html += "function startPoliv() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/start-poliv', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function stopPoliv() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/stop-poliv', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function setMilliliters() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set', true);";
  html += "  xhr.send(formData);";
  html += "}";

  html += "function startLight() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/start-light', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function stopLight() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/stop-light', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function resetManualControl() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/reset-manual-control', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function setThreshold() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setThresholdForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-threshold', true);";
  html += "  xhr.send(formData);";
  html += "}";
  
  html += "function setOffThreshold() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setOffThresholdForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-off-threshold', true);";
  html += "  xhr.send(formData);";
  html += "}";

  html += "function startWarm() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/start-warm', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function stopWarm() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/stop-warm', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function resetManualControlWarm() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/reset-manual-control-warm', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function setThresholdWarm() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setThresholdFormWarm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-threshold-warm', true);";
  html += "  xhr.send(formData);";
  html += "}";
  
  html += "function setOffThresholdWarm() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setOffThresholdFormWarm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-off-threshold-warm', true);";
  html += "  xhr.send(formData);";
  html += "}";

  html += "function openWindow() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/open-window', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function closeWindow() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/close-window', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function resetManualControlWindow() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/reset-manual-control-window', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function setThresholdWindow() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setThresholdFormWindow');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-threshold-window', true);";
  html += "  xhr.send(formData);";
  html += "}";
  
  html += "function setOffThresholdWindow() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setOffThresholdFormWindow');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-off-threshold-window', true);";
  html += "  xhr.send(formData);";
  html += "}";

  html += "function reconnect() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/reconnect', true);";
  html += "  xhr.send();";
  html += "  location.reload();";
  html += "}";

  html += "function setIrrigationTimes() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setTimeForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-irrigation-times', true);";
  html += "  xhr.send(formData);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState === XMLHttpRequest.DONE) {";
  html += "      if (xhr.status === 200) {";
  html += "        location.reload();";
  html += "      } else {";
  html += "        console.log('Error setting irrigation times.');";
  html += "      }";
  html += "    }";
  html += "  };";
  html += "}";

  html += "function setCurrentTime() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setRTCTimeForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set-current-time', true);";
  html += "  xhr.send(formData);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState === XMLHttpRequest.DONE) {";
  html += "      if (xhr.status === 200) {";
  html += "        location.reload();";
  html += "      } else {";
  html += "        console.log('Error setting current time.');";
  html += "      }";
  html += "    }";
  html += "  };";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  
  server.send(200, "text/html", html);
}

void handleSet()
{
  if (server.hasArg("milliliters"))
  {
    requiredMilliliters = server.arg("milliliters").toInt() / 5;
    count_imp_all = 0;
    count_imp = 0;
  }
  server.send(200, "text/html", "");
}

void handleSetThreshold()
{
  if (server.hasArg("threshold"))
  {
    int newThreshold = server.arg("threshold").toInt();
    lightThreshold = newThreshold;
  }
  server.send(200, "text/html", "");
}

void handleSetOffThreshold()
{
  if (server.hasArg("offThreshold"))
  {
    int newOffThreshold = server.arg("offThreshold").toInt();
    lightOffThreshold = newOffThreshold;
  }
  server.send(200, "text/html", "");
}

void handleSetThresholdWarm()
{
  if (server.hasArg("thresholdWarm"))
  {
    int newThreshold = server.arg("thresholdWarm").toInt();
    warmThreshold = newThreshold;
  }
  server.send(200, "text/html", "");
}

void handleSetOffThresholdWarm()
{
  if (server.hasArg("offThresholdWarm"))
  {
    int newOffThreshold = server.arg("offThresholdWarm").toInt();
    warmOffThreshold = newOffThreshold;
  }
  server.send(200, "text/html", "");
}

void handleSetThresholdWindow()
{
  if (server.hasArg("thresholdWindow"))
  {
    int newThreshold = server.arg("thresholdWindow").toInt();
    windowThreshold = newThreshold;
  }
  server.send(200, "text/html", "");
}

void handleSetOffThresholdWindow()
{
  if (server.hasArg("offThresholdWindow"))
  {
    int newOffThreshold = server.arg("offThresholdWindow").toInt();
    windowOffThreshold = newOffThreshold;
  }
  server.send(200, "text/html", "");
}

void start_light()
{
  digitalWrite(RELAY_IN5, LOW);
  is_light_on = true;
}

void stop_light()
{
  digitalWrite(RELAY_IN5, HIGH);
  is_light_on = false;
}

void open_window() {
  if (!windowOpening && !windowStartTime && !windowClosing) {
    digitalWrite(RELAY_IN1, LOW); // Start opening the window
    windowStartTime = millis();
    windowOpening = true;
    is_window_on = true;
  }
}

void close_window() {
  if (!windowClosing && windowStartTime && !windowOpening) {
    digitalWrite(RELAY_IN2, LOW); // Start closing the window
    windowStartTime = millis();
    windowClosing = true;
    is_window_on = false;
  }
}

void open_window_button() {
  if (!windowOpening && !windowStartTime && !windowClosing) {
    digitalWrite(RELAY_IN1, LOW); // Start opening the window
    windowStartTime = millis();
    windowOpening = true;
    is_window_on = true;
    WindowManualControl = true;
  }
}

void close_window_button() {
  if (!windowClosing && windowStartTime && !windowOpening) {
    digitalWrite(RELAY_IN2, LOW); // Start closing the window
    windowStartTime = millis();
    windowClosing = true;
    is_window_on = false;
    WindowManualControl = true;
  }
}

void updateWindowStatus() {
  if (windowOpening && millis() - windowStartTime >= windowOpenDuration) {
    digitalWrite(RELAY_IN1, HIGH); // Stop opening the window
    windowOpening = false;
    windowStartTime = true; // Update the window state
  }

  if (windowClosing && millis() - windowStartTime >= windowCloseDuration) {
    digitalWrite(RELAY_IN2, HIGH); // Stop closing the window
    windowClosing = false;
    windowStartTime = false; // Update the window state
  }
}

void start_light_button()
{
  digitalWrite(RELAY_IN5, LOW);
  LightManualControl = true;
  is_light_on = true;
}

void stop_light_button()
{
  digitalWrite(RELAY_IN5, HIGH);
  LightManualControl = true;
  is_light_on = false;
}

void start_warm()
{
  digitalWrite(RELAY_IN3, LOW);
  is_warm_on = true;
}

void stop_warm()
{
  digitalWrite(RELAY_IN3, HIGH);
  is_warm_on = false;
}

void start_warm_button()
{
  digitalWrite(RELAY_IN3, LOW);
  WarmManualControl = true;
  is_warm_on = true;
}

void stop_warm_button()
{
  digitalWrite(RELAY_IN3, HIGH);
  WarmManualControl = true;
  is_warm_on = false;
}

void handleResetManualControlWarm()
{
  WarmManualControl = false; // Disable manual control, revert to automatic mode
  server.send(200, "text/html", "");
}

void handleResetManualControlWindow()
{
  WindowManualControl = false; // Disable manual control, revert to automatic mode
  server.send(200, "text/html", "");
}

void handleResetManualControl()
{
  LightManualControl = false; // Disable manual control, revert to automatic mode
  server.send(200, "text/html", "");
}

void start_poliv()
{
  digitalWrite(RELAY_IN4, LOW);
  is_irrigation_on = true;
}

void stop_poliv()
{
  digitalWrite(RELAY_IN4, HIGH);
  is_irrigation_on = false;
}

void handleSetIrrigationTimes() {

  irrigationStartTimeHour1 = server.arg("startHour1").toInt();
  irrigationStartTimeMinute1 = server.arg("startMinute1").toInt();
  irrigationStartTimeHour2 = server.arg("startHour2").toInt();
  irrigationStartTimeMinute2 = server.arg("startMinute2").toInt();
  
  server.send(200, "text/html", "");
}

void handleSetCurrentTime() {
 
  int newHour = server.arg("currentHour").toInt();
  int newMinute = server.arg("currentMinute").toInt();
  DateTime now = rtc.now();
  now = DateTime(now.year(), now.month(), now.day(), newHour, newMinute, now.second());
  rtc.adjust(now);
  
  server.send(200, "text/html", "");
}

void setup()
{
  
  Serial.begin(115200);
  pinMode(Interrupt_Pin, INPUT);
  pinMode(RELAY_IN1, OUTPUT);
  pinMode(RELAY_IN2, OUTPUT);
  pinMode(RELAY_IN3, OUTPUT);
  pinMode(RELAY_IN4, OUTPUT);
  pinMode(RELAY_IN5, OUTPUT);
  pinMode(RELAY_IN6, OUTPUT);
  

  digitalWrite(RELAY_IN1, HIGH);
  digitalWrite(RELAY_IN2, HIGH);
  digitalWrite(RELAY_IN3, HIGH);
  digitalWrite(RELAY_IN4, HIGH);
  digitalWrite(RELAY_IN5, HIGH);
  digitalWrite(RELAY_IN6, HIGH);

  dht.setup(D5, DHTesp::DHT22);
  Wire.begin();
  
  rtc.begin();
  
  // Настройка времени: год-месяц-день час:минута:секунда
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  attachInterrupt(digitalPinToInterrupt(Interrupt_Pin), getFlow, FALLING);

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Попробуем подключиться к последней известной Wi-Fi сети
  if (!wifiManager.autoConnect("Config ESP8266")) {
    Serial.println("Failed to connect. You can try to connect manually using 'Config ESP8266' AP.");
  }
  else{
    reconnectToWiFi();
  }
  digitalWrite(RELAY_IN6, LOW);
  delay(100);
  ground_hum = analogRead(A0);
  digitalWrite(RELAY_IN6, HIGH);
  delay(100);
  // Если подключение установлено или было введено вручную, продолжим выполнение кода
  Serial.println("Connected to Wi-Fi!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Password: ");
  Serial.println(WiFi.psk());

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/start-poliv", start_poliv);
  server.on("/stop-poliv", stop_poliv);
  server.on("/start-light", start_light_button);
  server.on("/stop-light", stop_light_button);
  server.on("/reset-manual-control", handleResetManualControl);
  server.on("/set-threshold", handleSetThreshold);
  server.on("/set-off-threshold", handleSetOffThreshold);
  server.on("/open-window", open_window_button);
  server.on("/close-window", close_window_button);
  server.on("/start-warm", start_warm_button);
  server.on("/stop-warm", stop_warm_button);
  server.on("/reset-manual-control-warm", handleResetManualControlWarm);
  server.on("/set-threshold-warm", handleSetThresholdWarm);
  server.on("/set-off-threshold-warm", handleSetOffThresholdWarm);
  server.on("/reset-manual-control-window", handleResetManualControlWindow);
  server.on("/set-threshold-window", handleSetThresholdWindow);
  server.on("/set-off-threshold-window", handleSetOffThresholdWindow);
  server.on("/reconnect", clearWifiCredentials);
  server.on("/set-irrigation-times", handleSetIrrigationTimes);
  server.on("/set-current-time", handleSetCurrentTime);

  server.begin();
}

void loop()
{
  DateTime now = rtc.now();
  server.handleClient();
  count_imp_all = count_imp_all + count_imp;
  count_imp = 0;

  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();

  // Активация полива в установленные времена
  if ((currentHour == irrigationStartTimeHour1 && currentMinute == irrigationStartTimeMinute1 && currentSecond == 0) ||
      (currentHour == irrigationStartTimeHour2 && currentMinute == irrigationStartTimeMinute2 && currentSecond == 0)) {
    start_poliv();
  }

  sensors.requestTemperatures();
  float humidity = dht.getHumidity();
  float temperature = sensors.getTempCByIndex(0);

  if (!LightManualControl){
    if (analogRead(A0) < lightThreshold)
    {
      start_light();
    }
    else if (analogRead(A0) > lightOffThreshold)
    {
      stop_light();
    }
  }

  if (!WarmManualControl){
    if (sensors.getTempCByIndex(0) < warmThreshold)
    {
      start_warm();
    }
    else if (sensors.getTempCByIndex(0) > warmOffThreshold)
    {
      stop_warm();
    }
  }

  if (!WindowManualControl){
    if(temperature > windowThreshold){
      open_window();
    }
    else if(temperature < windowOffThreshold || humidity > 98){
      close_window();
    }
  }

  if ((currentHour == 0 && currentMinute == 0 && currentSecond == 0) || 
  (currentHour == 8 && currentMinute == 0 && currentSecond == 0) || 
  (currentHour == 16 && currentMinute == 0 && currentSecond == 0)){ // считывание влажности почвы 3 раза в сутки
    digitalWrite(RELAY_IN6, LOW);
    delay(100);
    ground_hum = analogRead(A0);
    digitalWrite(RELAY_IN6, HIGH);
    delay(100);
  }

  updateWindowStatus();
}

void clearWifiCredentials() {
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  Serial.println("Wi-Fi credentials cleared.");
  ESP.restart(); // Restart ESP8266 after clearing credentials
}

void reconnectToWiFi() {
  Serial.println("Attempting to reconnect to Wi-Fi...");

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Включаем режим точки доступа, чтобы быть доступным для новых настроек
  wifiManager.setAPCallback([](WiFiManager* mgr) {
    Serial.println("Entered config mode. You can now connect to 'Config ESP8266' to enter new Wi-Fi settings.");
  });

  // Попробуем подключиться к Wi-Fi сети
  if (wifiManager.autoConnect("Config ESP8266")) {
    Serial.println("Reconnected to Wi-Fi!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("Password: ");
    Serial.println(WiFi.psk());

    // Выключаем режим точки доступа после успешного подключения
    wifiManager.setAPCallback(nullptr);
  } else {
    Serial.println("Failed to reconnect. You can try again.");
  }
}
