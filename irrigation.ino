#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <microDS3231.h>
// relle
#define RELAY_IN D7 

MicroDS3231 rtc;
ESP8266WebServer server(80);

const char* ssid = "Beeline_5E79";
const char* password = "92515952";

const byte Interrupt_Pin PROGMEM = D8; // rashodomer
volatile uint16_t count_imp;
float count_imp_all;
uint16_t requiredMilliliters = 0;
bool isPolivRunning = false;

//-----------------------------------------------
ICACHE_RAM_ATTR void getFlow()
{
  count_imp++;
  if (count_imp_all >= requiredMilliliters * 2) // Assuming 450 pulses per liter, requiredMilliliters are in milliliters, so we need 2 * requiredMilliliters pulses to pass the required amount.
  {
    Serial.println("Полив закончился!!!");
    digitalWrite(RELAY_IN, LOW);
    count_imp_all = 0;    // Reset the total accumulated milliliters
    count_imp = 0;
    isPolivRunning = false; // Set the irrigation status to false
  }
}
//-----------------------------------------------

void handleRoot()
{
  String html = "<html><body>";
  html += "<h1>Flow Meter Control</h1>";
  html += "<p>Enter the required milliliters:</p>";
  html += "<form id='setForm' method='POST'>";
  html += "<input type='number' name='milliliters' min='1'><br>";
  html += "<input type='button' value='Set' onclick='setMilliliters()'>";
  html += "</form>";

  // Add the button to start the poliv (irrigation) manually
  html += "<button type='button' onclick='startPoliv()'>Start Poliv</button>";
  html += "<button type='button' onclick='stopPoliv()'>Stop Poliv</button>";

  html += "<p>Status: ";
  if (isPolivRunning) {
    html += "Irrigation is running";
  } else {
    html += "Irrigation is not running";
  }
  html += "</p>";
  // JavaScript function to submit the form and start poliv
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

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSet()
{
  if (server.hasArg("milliliters"))
  {
    requiredMilliliters = server.arg("milliliters").toInt() / 5;
    count_imp_all = 0;    // Reset the total accumulated milliliters
    count_imp = 0;        // Reset the current round's milliliters
  }
  server.send(200, "text/html", ""); // Empty response since we are not redirecting
}

void start_poliv(){
    Serial.println("Начался полив!");
    digitalWrite(RELAY_IN, HIGH);
    isPolivRunning = true;
}

void stop_poliv() {
  Serial.println("Полив остановлен!");
  digitalWrite(RELAY_IN, LOW);
  isPolivRunning = false;
}


void setup()
{
  Serial.begin(115200);
  server.begin();

  server.on("/start-poliv", start_poliv);
  server.on("/stop-poliv", stop_poliv);

  pinMode(Interrupt_Pin, INPUT);
  pinMode(RELAY_IN, OUTPUT);

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

  server.begin();
}

void loop()
{
  server.handleClient();
  count_imp_all = count_imp_all + count_imp;
  count_imp = 0;
  if (rtc.getSeconds() == 1){
    start_poliv();
  }
}
