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
bool isAutoMode = true; // Default to auto mode

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
  html += "<h1>Flow Meter Control</h1>";
  html += "<p>Enter the required milliliters:</p>";
  html += "<form id='setForm' method='POST'>";
  html += "<input type='number' name='milliliters' min='1'><br>";
  html += "<input type='button' value='Set' onclick='setMilliliters()'>";
  html += "</form>";

  html += "<p>Mode: ";
  if (isAutoMode) {
    html += "Auto";
  } else {
    html += "Manual";
  }
  html += "</p>";

  html += "<p>Status: ";
  if (isPolivRunning) {
    html += "Irrigation is running";
  } else {
    html += "Irrigation is not running";
  }
  html += "</p>";

  html += "<button type='button' onclick='toggleMode()'>Toggle Mode</button>";
  html += "<button type='button' onclick='startPoliv()'>Start Poliv</button>";
  html += "<button type='button' onclick='stopPoliv()'>Stop Poliv</button>";

  html += "<hr>";
  html += "<p>Light Status: ";
  if (is_light_on) {
    html += "On";
  } else {
    html += "Off";
  }

  html += "</p>";
  html += "<button type='button' onclick='toggleLight()'>Toggle Light</button>";
  html += "<script>";

  html += "function startPoliv() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/start-poliv', true);";
  html += "  xhr.send();";
  html += "}";

  html += "function stopPoliv() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/stop-poliv', true);";
  html += "  xhr.send();";
  html += "}";

  html += "function setMilliliters() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var form = document.getElementById('setForm');";
  html += "  var formData = new FormData(form);";
  html += "  xhr.open('POST', '/set', true);";
  html += "  xhr.send(formData);";
  html += "}";

  html += "function toggleLight() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/toggle-light', true);";
  html += "  xhr.send();";
  html += "}";

  html += "function toggleMode() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/toggle-mode', true);";
  html += "  xhr.send();";
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

void toggleLight()
{
  if (isAutoMode) return; // Ignore manual control if in auto mode
  if (is_light_on)
  {
    stop_light();
  }
  else
  {
    start_light();
  }
}

void toggleMode()
{
  isAutoMode = !isAutoMode;
  if (isAutoMode) {
    stop_light(); // Turn off the light when switching to auto mode
  }
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
  server.on("/toggle-light", toggleLight);
  server.on("/toggle-mode", toggleMode);

  server.begin();
}

void loop()
{
  server.handleClient();
  count_imp_all = count_imp_all + count_imp;
  count_imp = 0;

  if (rtc.getSeconds() == 1 && isAutoMode)
  {
    start_poliv();
  }

  float humidity = dht.getHumidity();
  float temperature = sensors.getTempCByIndex(0);

  if (isAutoMode)
  {
    if (analogRead(A0) > 200)
    {
      start_light();
    }
    else
    {
      stop_light();
    }
  }
}
