// Includes
#include <ESP8266WiFi.h>  // ESP board
#include <RCSwitch.h>     // 433 MHz radio
#include "PubSubClient.h" // MQTT
#include "WEMOS_SHT3X.h"  // Temperature and humidity
#include <ArduinoJson.h>  // JSON helper
#include <time.h>         // Time

// WiFi Settings
//const char* WIFI_SSID = "IreHOST";
//const char* WIFI_PWD = "ChciZazit";
//const char* WIFI_SSID = "iPhone IReSoft";
//const char* WIFI_PWD = "12345678";
const char* WIFI_SSID = "Pa&Pi";
const char* WIFI_PWD = "papousek";
const int WIFI_INIT_RETRY_DELAY = 1000;

// Time
const char* TIME_SRC_1 = "pool.ntp.org";
const char* TIME_SRC_2 = "time.nist.gov";
const int TIME_INIT_RETRY_DELAY = 2000;

// MQTT server
const char* MQTT_SERVER = "jkiothub.azure-devices.net";
const char* MQTT_CLIENT_ID = "jk01";
const char* MQTT_USERNAME = "jkiothub.azure-devices.net/jk01";
const char* MQTT_PASSWORD = "SharedAccessSignature sr=jkiothub.azure-devices.net%2Fdevices%2Fjk01&sig=l%2BToD8LnqJHbTX%2FS8rHBvNyiNcNgF7tmX9ekLO5KE2A%3D&se=1514922089";
const int MQTT_PORT = 8883;
const int mqtt_publish_period = 60000;
const int MQTT_INIT_RETRY_DELAY = 5000;
long lastMessageTime = 0;

// mqttClients
WiFiClientSecure epsWiFiClient;
PubSubClient mqttClient(epsWiFiClient);

// 433 MHz radio controller
RCSwitch rcClient = RCSwitch();
const int RC_DATA_BIT_LENGTH = 24;

// Temperature and humidity controller
SHT3X sht30Client(0x45);

const int AMBIENT_LIGHT_PIN = A0;
const int PIR_PIN = D3;
const int RC_PIN = 12; // GPIO 12 = D6

const int RC_PULSE_LENGTH = 308;

// MQTT incoming message handler
void MqttIncomingMessageHandler(char* topic, byte* payload, unsigned int length) {

  // Switch on led (incoming message processing indicator)
  digitalWrite(LED_BUILTIN, LOW);

  String cmd = String();
  for (int i = 0; i < length; i++) {
    cmd += (char)payload[i];
  }

  Serial.print("Incomming command is: ");
  Serial.println(cmd);
  
  // Command parsing
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(cmd);

  const char* action = root["action"];
  const char* actor = root["actor"];
  const char* parameters = root["parameters"];

  String actionString = String(action);
  String actorString = String(actor);

  // Lights are handled by 433 radio
  if (actorString.equals("lights"))
  {
    rcClient.send(String(parameters).toInt(), RC_DATA_BIT_LENGTH);
  }

  // Switch on led (incoming message processing indicator)
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Publish current state
  ReadDataAndPublish();
}

// Init IO
void initIO()
{
  pinMode(AMBIENT_LIGHT_PIN, INPUT);
  pinMode(PIR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
}

// Init Serial
void initSerial()
{
  Serial.begin(115200);
}

// Initializes current time
void initTime() {
  time_t epochTime;
  configTime(0, 0, TIME_SRC_1, TIME_SRC_2);
  while (true) {
    epochTime = time(NULL);
    if (epochTime == 0) {
      Serial.println("Fetching NTP epoch time failed! Waiting " + String(TIME_INIT_RETRY_DELAY) + " miliseconds to retry.");
      delay(TIME_INIT_RETRY_DELAY);
    } else {
      Serial.print("Fetched NTP epoch time is: ");
      Serial.println(epochTime);
      break;
    }
  }
}

// Initializes WiFi
void initWifi() {
}


// Recibbect for WiFi
void WiFiReconnect() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_INIT_RETRY_DELAY);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Init RC
void initRC()
{
  rcClient.enableTransmit(RC_PIN); // GPIO 12 = D6
  rcClient.setPulseLength(RC_PULSE_LENGTH);
}

// Init MQTT
void initMQTT()
{
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(MqttIncomingMessageHandler);
}

// MqttReconnect to MQTT
void MqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("connected");
      mqttClient.subscribe("devices/jk01/messages/devicebound/#", 1);
    } else {
      Serial.print("failed, rcClient=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in " + String(MQTT_INIT_RETRY_DELAY) + " seconds");
      delay(MQTT_INIT_RETRY_DELAY);
    }
  }
}

void ReadDataAndPublish()
{
  lastMessageTime = millis();
  Serial.print("Publishing message: ");

  sht30Client.get();
  //    Serial.print("Temperature in Celsius : ");
  //    Serial.println(sht30Client.cTemp);
  //    Serial.print("Temperature in Fahrenheit : ");
  //    Serial.println(sht30Client.fTemp);
  //    Serial.print("Relative Humidity : ");
  //    Serial.println(sht30Client.humidity);
  //    Serial.println();
  unsigned int light = analogRead(AMBIENT_LIGHT_PIN);
  //    Serial.println(light);

  if (light < 50)
    light = 0;
  else
    light = 100;

  bool motion = false;
  int pirValState = digitalRead(PIR_PIN);
  if (pirValState == LOW)
  {
    // motion
    motion = true;
  }
  else
  {
    motion = false;
    // no motion
  }
  Serial.println(motion);

  time_t epochTime = time(NULL);
  //Serial.println(".........");
  //Serial.println(epochTime);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = "jk01";
  root["humidity"] = sht30Client.humidity;
  root["temperature"] = sht30Client.cTemp;
  root["timestamp"] = epochTime;
  root["light"] = light;
  root["move"] = motion;
  //    root.printTo(Serial);

  String s;
  root.printTo(s);

  Serial.println(s);

  const char *cstr = s.c_str();
  mqttClient.publish("devices/jk01/messages/events/", cstr);
}

// Setup board
void setup() {
  // Init IO
  initIO();

  // Init serial
  initSerial();

  // Init Wifi
  initWifi();

  // Init time
  initTime();

  // Init RC
  initRC();

  // Init MQTT
  initMQTT();
}

// Main loop
void loop() {
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFiReconnect();
  }

  // Reconnect, if not already connected
  if (!mqttClient.connected()) {
    MqttReconnect();
  }

  // Check incoming messages
  mqttClient.loop();

  // Send current status, if it is time
  if (millis() - lastMessageTime > mqtt_publish_period) {
    ReadDataAndPublish();
  }
}

