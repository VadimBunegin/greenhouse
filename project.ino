#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <microDS3231.h>
#include "DHTesp.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Relay pins
#define RELAY_IN1 D1 // Window open
#define RELAY_IN2 D2 // Window close
#define RELAY_IN3 D3 // Light
#define RELAY_IN4 D4 // Irrigation

// Flow meter
const byte Interrupt_Pin = D8;
volatile uint16_t count_imp;
float count_imp_all;
uint16_t requiredMilliliters = 0;

bool isPolivRunning = false;
bool is_light_on = false;
bool manualControl = false;
int lightThreshold = 200; // Default threshold value
int lightOffThreshold = 100;


MicroDS3231 rtc;
ESP8266WebServer server(80);

const char* ssid = "Beeline_5E79";
const char* password = "92515952";

const int oneWireBus = D6;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

DHTesp dht;

void ICACHE_RAM_ATTR getFlow()
{
  count_imp++;
  if (count_imp_all >= requiredMilliliters * 2)
  {
    Serial.println("Полив закончился!!!");
    digitalWrite(RELAY_IN4, HIGH);
    count_imp_all = 0;
    count_imp = 0;
    isPolivRunning = false;
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

  html += "<p>Status: ";
  if (isPolivRunning) {
    html += "Irrigation is running";
  } else {
    html += "Irrigation is not running";
  }
  html += "</p>";

  html += "<button type='button' onclick='startPoliv()'>Start Irrigation</button>";
  html += "<button type='button' onclick='stopPoliv()'>Stop Irrigation</button>";

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
    html += "On";
  } else {
    html += "Off";
  }
  html += "</p>";

  html += "<button type='button' onclick='startLight()'>Turn On Light</button>";
  html += "<button type='button' onclick='stopLight()'>Turn Off Light</button>";
  html += "<button type='button' onclick='resetManualControl()'>Reset Manual Control</button>";
  html += "<hr>";

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

void start_light()
{
  digitalWrite(RELAY_IN3, LOW);
  is_light_on = true;
}

void stop_light()
{
  digitalWrite(RELAY_IN3, HIGH);
  is_light_on = false;
}

void start_light_button()
{
  digitalWrite(RELAY_IN3, LOW);
  manualControl = true;
  is_light_on = true;
}

void stop_light_button()
{
  digitalWrite(RELAY_IN3, HIGH);
  manualControl = true;
  is_light_on = false;
}

void handleResetManualControl()
{
  manualControl = false; // Disable manual control, revert to automatic mode
  Serial.print(manualControl);
  server.send(200, "text/html", "");
}

void start_poliv()
{
  Serial.println("Начался полив!");
  digitalWrite(RELAY_IN4, LOW);
  isPolivRunning = true;
}

void stop_poliv()
{
  Serial.println("Полив остановлен!");
  digitalWrite(RELAY_IN4, HIGH);
  isPolivRunning = false;
}

void setup()
{
  Serial.begin(115200);

  pinMode(Interrupt_Pin, INPUT);
  pinMode(RELAY_IN1, OUTPUT);
  pinMode(RELAY_IN2, OUTPUT);
  pinMode(RELAY_IN3, OUTPUT);
  pinMode(RELAY_IN4, OUTPUT);

  digitalWrite(RELAY_IN1, HIGH);
  digitalWrite(RELAY_IN2, HIGH);
  digitalWrite(RELAY_IN3, HIGH);
  digitalWrite(RELAY_IN4, HIGH);

  dht.setup(D5, DHTesp::DHT22);

  attachInterrupt(digitalPinToInterrupt(Interrupt_Pin), getFlow, FALLING);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/start-poliv", start_poliv);
  server.on("/stop-poliv", stop_poliv);
  server.on("/start-light", start_light_button);
  server.on("/stop-light", stop_light_button);
  server.on("/reset-manual-control", handleResetManualControl);
  server.on("/set-threshold", handleSetThreshold);
  server.on("/set-off-threshold", handleSetOffThreshold);

  server.begin();
}

void loop()
{
  server.handleClient();
  count_imp_all = count_imp_all + count_imp;
  count_imp = 0;

  if (rtc.getSeconds() == 1)
  {
    start_poliv();
  }

  float humidity = dht.getHumidity();
  float temperature = sensors.getTempCByIndex(0);

  if (!manualControl){
    if (analogRead(A0) < lightThreshold)
    {
      start_light();
    }
    else if (analogRead(A0) > lightOffThreshold)
    {
      stop_light();
    }
  }


}
