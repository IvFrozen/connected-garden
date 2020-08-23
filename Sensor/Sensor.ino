/***************SENSOR.INO****************
 *      This is the system controller code   *
 *********************************************/
#include <ESP8266WiFi.h>  //to enable wifi connection
#include <PubSubClient.h> //to enable MQTT communication

#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h> //to enable coding/decoding JSON documents
#include "Adafruit_seesaw.h" //soil humidity sensor
#include "Adafruit_VEML7700.h" //light intensity sensor

#include "config.h" //application configuration lives here
#include "credentials.h" //network and API credentials live here

char mac[18]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!

Adafruit_seesaw soilSensor;       //soil sensor
Adafruit_VEML7700 veml = Adafruit_VEML7700(); //light sensor
WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client
 
void setup() {
  // put your setup code here, to run once:
  setup_Serial();
  //setup sensors
  setup_Soil();
  setup_Light();
  //setup wifi
  setup_wifi(); //set up wifi connection
  //setup mqtt communication
  mqtt.setServer(mqtt_server, 1883);
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.println(mac);
    Serial.println(sensor_mqtt_user);
    Serial.println(mqtt_password);
    if (mqtt.connect(mac, sensor_mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("MQTT connected");
    } else {           
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
   Serial.println("Reporting sensor data");
   //TODO could have been done as a single json message sith 3 parameters
   //Left it this way for sensor configurability (what if I decide to disable light sensor?)
   reportHumidity();  //send soil humidity readings
   reportTemperature(); //send soil temperature readings
   reportLight(); //send sunlight intensity readings
   mqtt.disconnect(); 
   Serial.println("Falling asleep");
   //enter deep sleep when the message is sent
   ESP.deepSleep(sensorRefreshInterval);
  //timer = millis(); //reset timer
}

void loop() {
}

void setup_Serial() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));
}

void setup_Soil() {
  if (!soilSensor.begin(0x36)) {
    Serial.println("ERROR! Soil sensor not found");
    while(1);
  } else {
    Serial.print("Soil sensor is on");
  }
}

void setup_Light() {
  if (!veml.begin()) {
    Serial.println("Error! Lux sensor not found");
    while (1);
  }
  Serial.println("Lux sensor is on");
 
  veml.setGain(VEML7700_GAIN_1);
  veml.setIntegrationTime(VEML7700_IT_800MS);
  veml.setLowThreshold(10000);
  veml.setHighThreshold(20000);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress() returns a byte array 6 bytes representing the MAC address
  String temp = WiFi.macAddress();
  temp.toCharArray(mac, 18); //use full mac address, so that it is truly unique
}

void reportHumidity() {
  uint16_t cap = soilSensor.touchRead(0); //read soil humidity
  int hum = map(cap, 0,1023,0,100); //translate sensor readings to 0..100% scale
  StaticJsonDocument<256> outputDoc; //pack sensor reading into JSON message
  outputDoc["sensorID"]=mac;
  outputDoc["humidity"]=hum;
  char buffer[256];
  serializeJson(outputDoc, buffer); //create json string
  Serial.print("Humidity ");
  Serial.println(humFeed);
  Serial.println(buffer);
  mqtt.publish(humFeed, buffer); //post json string to corresponding feed
}

void reportTemperature() {
  float temp = soilSensor.getTemp(); //read soil temperature
  StaticJsonDocument<256> outputDoc;//pack sensor reading into JSON message
  outputDoc["sensorID"]=mac;
  outputDoc["temperature"]=temp;
  char buffer[256];
  serializeJson(outputDoc, buffer);//create json string
  Serial.print("Temperature ");
  Serial.println(tempFeed);
  Serial.println(buffer);
  mqtt.publish(tempFeed, buffer);//post json string to corresponding feed
}

void reportLight() {
  float lux = veml.readLux(); //read soil temperature
  StaticJsonDocument<256> outputDoc;//pack sensor reading into JSON message
  outputDoc["sensorID"]=mac;
  outputDoc["lumenosity"]=lux;
  char buffer[256];
  serializeJson(outputDoc, buffer);//create json string
  Serial.print("Lumenosity ");
  Serial.println(lightFeed);
  Serial.println(buffer);
  mqtt.publish(lightFeed, buffer);//post json string to corresponding feed
}
