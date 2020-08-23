/***************CONTROLLER.INO****************
 *      This is the system controller code   *
 *********************************************/

#include <ESP8266WiFi.h>  //to enable wifi connection
#include <ESP8266HTTPClient.h> //to enable HTTP client for API requests
#include <PubSubClient.h> //to enable MQTT communication
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <LinkedList.h>
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h> //to enable coding/decoding JSON documents

#include "config.h" //application configuration lives here
#include "credentials.h" //network and API credentials live here



//Internal state variables
unsigned int localPort = 8888;  // local port to listen for UDP packets
char mac[18]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
int operationMode; //0 - setup, 1 - operation
bool leftPressed, rightPressed, enterPressed; //pushbutton states
int cursorPosition; //indicates which menu option or screen is selected
int screenPosition; //indicates which operation screen is shown on oled display
LinkedList<Plant> *plants = new LinkedList<Plant>; //"database" of plants 
float currentHum,  currentTemp; //current value of soil moisture and temperature measurement
float currentLum, intLum; //current and integrated values of lumenosity
long prevLumT, prevHumT, prevTempT; //the timestamp of the previous lumenosity and soil moisture measurement
long checkWateringTimer; //timer set when we start watering process
bool isWateringPostponed; //set to true if we postpone watering due to forecast
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets


//hardware configuration
const int LEFT_BTN_PIN = 14;
const int RIGHT_BTN_PIN = 0;
const int ENTER_BTN_PIN = 2;
const int LED_RED_PIN = 15;
const int LED_GREEN_PIN = 13;
const int LED_BLUE_PIN = 12;
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

WiFiUDP Udp;
WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //define display object 

//function declarations
time_t getNtpTime(); //gets network time
void sendNTPpacket(IPAddress &address); //utility function to get time
void setupMode(); //hub operation is setup mode
void mainMode(); //hub operation in the main mode
void mqtt_reconnect(); //maintains connection to mqtt server
void setupSerial(); //sets up serial port operation
void setupWifi(); // sets up wifi connection
void setupMQTT(); //sets up MQTT server connection
void setupDisplay (); //initializes OLED display
void displayPlant (String name, bool first, bool last); //renders setup menu (plant name and arrows)
void populatePlantData(); //utility function used to populate the list of plants in leu of the database
Plant generatePlant(String name, float hmin, float hmax, float tmin, float tmax, float lmin, float lmax); //generates single plant record
void setupButtons(int left, int right, int enter); //sets up hardware for pin buttons
void monitorButtons(); //surveys button state and populates internal state variables
void callback(char* topic, byte* payload, unsigned int length); //mqtt feeds callback 

void setup() {
  //initialize internal states
  checkWateringTimer=0;
  operationMode = 0;
  cursorPosition=0;
  isWateringPostponed = false;

  //setup application components
  setupSerial();
  setupButtons(LEFT_BTN_PIN, RIGHT_BTN_PIN, ENTER_BTN_PIN);
  setupDisplay();
  setupWifi();
  setupTime();
  setupMQTT();

  //populate plants database
  populatePlantData();  

  //only after that show something on the screen
  displayPlant(plants->get(cursorPosition).name,(cursorPosition==0),(plants->size()==cursorPosition+1));
}

void loop() {
  //maintain mqtt connection
  if (!mqtt.connected()) {
    mqtt_reconnect();
  }
  mqtt.loop();

  //proceed to operation function based on operation mode
  if (operationMode==0) {
    //we are in setup mode
    setupMode();
  } else {
    //we are in operation mode
    mainMode();
  }
  
}


/* ---------------SETUP FUNCTIONS--------------*/
void setupTime() {
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void setupMQTT() {
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);
  mqtt_reconnect();
  Serial.println("Listening to MQTT Feeds");
}

void setupSerial() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));
}

void setupWifi() {
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

void setupDisplay () {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();  //clear display buffer
    display.setTextSize(1);      // Set text size
    display.setTextColor(SSD1306_WHITE); // Draw white text
    Serial.println("Display is on");
}

void setupButtons(int left, int right, int enter) {
  pinMode(left, INPUT_PULLUP);
  pinMode(right, INPUT_PULLUP);
  pinMode(enter, INPUT_PULLUP);
}


/* ---------END OF SETUP FUNCTIONS------------*/



/* ---------MAIN LOOP FUNCTIONS --------------*/
void setupMode() { //this function implements setup mode
  //user can select plants using arrow and "enter" buttons
  monitorButtons(); //check if any buttons were clicked
  if (enterPressed) { //user pressed "enter"
    operationMode=1; //switch to operation mode
    screenPosition=0;
    delay(500);
  } else {
    //if left or right were pressed, update cursor position and display new plant
    if (leftPressed && cursorPosition>0) {
     cursorPosition-=1;
     delay(500);
     //leftPressed = false;
    }
    if (rightPressed && cursorPosition+1<plants->size()) {
     cursorPosition+=1;
     delay(500);
    } 
    displayPlant(plants->get(cursorPosition).name,(cursorPosition==0),(plants->size()==cursorPosition+1));
  }
}


void mainMode() { //this function implements main operation mode
  //user can switch between the screens showing different measured parameters using arrow keys
  //pressing "enter" will send the device back to setup mode
  monitorButtons(); //check if any buttons were pressed
  if (enterPressed) {
    Serial.println("Back to setup");
    operationMode=0;
  } else {
    if (leftPressed && screenPosition>0) {
     screenPosition-=1;
     //display next screen
     delay(500);
    }
    if (rightPressed && screenPosition<nScreens) {
     screenPosition+=1;
     delay(500);
     //display previous screen
    } 
    displayScreen(screenPosition);
    
    //now let's see if we need to water the plant
    int currenthour = hour();
    if (now()-checkWateringTimer>60*MINUTES) { //if we haven't watered the plant recently
      if ((currenthour==wateringHours[0])||(currenthour==wateringHours[1])) { //and we are in 1 of 2 watering windows
        Serial.println("It is watering time");
        checkWateringTimer = now(); //reset watering block, so that we don't water within the same hour again
        if (willItRain() && !isWateringPostponed) {
          //forecast says it is going to rain
          //AND we havent postponed the watering during last watering window
          Serial.println("But it is going to rain soon, so we are going to wait");
          isWateringPostponed=true;
        } else {
          //we are about to water plants
          //either there is no rain forecasted
          //or we have postponed the watering before and we can't wait for rain anymore
          isWateringPostponed=false;
          
          //but let's check soil moisture first
          if (currentHum>plants->get(cursorPosition).soilHumidityMin) {
            //too wet
            Serial.println("Soil is too wet for watering");
          } else {
            //dry enough
            Serial.println("Let the water flow");
            //water plant for 20 minutes
            //TODO - Ideally there should be a closed loop where watering time is determined based on sensor readings
            // But it requires multiple sensors installed on different depths, as single sensor quite quickly reads "100%"
            // Something to think about in future
            waterPlant(20);
          }
        }
      } else {
         //this is not the best time to water the plants
          checkWateringTimer=0;
      }
    }

    //let's check light integration
    if ((currenthour==0)&&(minute()<3)) {
      //reset light integrator at midnight
      intLum=0;
    }

    //TODO - I also wanted to implement soil cooling procedure with short watering in case the soil temperature is too high
    //While it is feasibe, I haven't found any information about whether it is beneficial and safe for plants.
    //This feature remains in "to be researched" status
  }
}

/* --------- END MAIN LOOP FUNCTIONS --------------*/



/* ------- Misc functions for getting information into and out of the system ----- */
void monitorButtons() { //reads mutton states
  leftPressed = (digitalRead(LEFT_BTN_PIN)==LOW);
  rightPressed = (digitalRead(RIGHT_BTN_PIN)==LOW);
  enterPressed = (digitalRead(ENTER_BTN_PIN)==LOW);
}

void mqtt_reconnect() { //maintains mqtt connection
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.println(mac);
    Serial.println(controller_mqtt_user);
    Serial.println(mqtt_password);
    if (mqtt.connect(mac, controller_mqtt_user, mqtt_password)) {
      Serial.println("MQTT connected");
       mqtt.subscribe(humFeed);
        mqtt.subscribe(tempFeed);
        mqtt.subscribe(lightFeed);
    } else {           
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool willItRain() { //implements API call to obrain weather forecast
  //analyses weather forecast to determine whether it is going to rain or not
  
  HTTPClient theClient;  //creating http client
  bool precip;
  Serial.print("Obtaining weather information for ");
  Serial.println(location); //location is hard-coded in config.h as a city name
  theClient.begin("http://api.weatherstack.com/current?access_key="+(weather_key+("&query="+location))); //form the url using city name and weather API key
  int httpCode = theClient.GET(); //send GET request
  
  if (httpCode==200) { //if success
    //parse weather data
    String payload = theClient.getString();
    StaticJsonDocument<2048> doc; //allocate 2K for json document
    DeserializationError error = deserializeJson(doc, payload); //deserialize response payload as json
    if (error) {
        Serial.print("deserializeJson() failed with error code ");
        Serial.println(error.c_str());
        Serial.println(payload);
        return(false); //if something goes wrong, assume it will not rain = don't rely on weather forecast
      }
     int pp =doc["current"]["precip"].as<int>(); //all we need is precipitation probability
     return (pp>80);  //if above 80%, assume that it will rain
      
  } else {
    //something went wrong
    Serial.println("Unable to obtain weather information for "+location);
    Serial.print("Response code: ");
    Serial.println(httpCode);
    String payload = theClient.getString();
    Serial.println(payload);
    return(false); //if something goes wrong, assume it will not rain = don't rely on weather forecast
  }
}

void waterPlant(int duration) { //connects to irrigation controller over HTTP and executes watering program
    String programTemplate="doProgram=1&z1durHr=0&z1durMin="+String(duration)+"&z2durHr=0&z2durMin=0&z3durHr=0&z3durMin=0&z4durHr=0&z4durMin=0&z5durHr=0&z5durMin=0&z6durHr=0&z6durMin=0&z7durHr=0&z7durMin=0&z8durHr=0&z8durMin=0&z9durHr=0&z9durMin=0&runNow=1&pgmNum=4";
    HTTPClient theClient;  //creating http client
    Serial.print("Water is on for ");
    Serial.print(duration);
    Serial.println(" minutes");  
    theClient.begin(irr_controller_url); //get the url of the watering program
    theClient.addHeader("Content-Type", "application/x-www-form-urlencoded"); //simulate form submit 
    int httpCode = theClient.POST(programTemplate); //send POST request
    theClient.end(); //close connection
    if (httpCode==200) {
      Serial.println("....Sound of flowing water.....");
    } else {
      //something didn't work this time. Let's assume the watering was postponed
      //the system will try watering the plant during the next watering window
      isWateringPostponed=false;
    }
}

/* ------- End of misc functions for getting information into and out of the system ----- */

/* ----- Functions to render data on the display ----- */

void displayPlant (String name, bool first, bool last) { //Renders setup mode
  const int dispLine = 10; //the number of characters that will fit on the display
  int nameLen=name.length();
  if (nameLen>dispLine-2) {
    name.remove(dispLine-2);
  }
  int bar = dispLine-2-name.length(); //spacing between arrows and the plant name
  String result = String(dispLine);
  if (first) {
    result=" ";
  } else {
    result="<";
  }
  for (int i=0;i<bar/2;i++) {
    result+=" ";
  }
  result+=name;
  int resLen=result.length();
  for (int i=resLen; i<dispLine-1;i++) {
    result+=" ";
  }
  if (last) {
    result+=" ";
  } else {
    result+=">";
  }
  display.clearDisplay();
  display.setCursor(0,4);
  display.setTextSize(2);
  display.println(result);
  Serial.println(result);
  display.display();
  
}

void displayScreen(int pos) { //Renders main mode

  //Print main screen content
  switch (pos) {
    case 0: //first screen will display soil humidity
    display.setTextSize(1);
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Soil humidity");
    display.println(currentHum);
    display.print(" (");
    display.print(plants->get(cursorPosition).soilHumidityMin); //show acceptable range
    display.print(" - ");
    display.print(plants->get(cursorPosition).soilHumidityMax);
    display.println(")");
    display.display();
    break;
    
    case 1: //second screen displays how much light the plant gets
    display.setTextSize(1);
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Light");
    display.println(intLum);
    display.print(" (");
    display.print(plants->get(cursorPosition).sunMin); //show acceptable range
    display.print(" - ");
    display.print(plants->get(cursorPosition).sunMax);
    display.println(")");
    display.display();
    break;
    
    case 2: //third screen displays soil temperature
    display.setTextSize(1);
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Soil temperature");
    display.println(currentTemp);
    display.print(" (");
    display.print(plants->get(cursorPosition).soilTemperatureMin); //show acceptable range
    display.print(" - ");
    display.print(plants->get(cursorPosition).soilTemperatureMax);
    display.println(")");
    break;
    
    default:
    break;
  }
  if (pos!=0) {
    //display left arrow if not the first screen
    display.print("<-  ");
  } else {
    display.print("      ");
  }
  display.print("Back");
  if (pos!=nScreens) {
    //display right arrow if not the last screen
    display.print("  ->");
  } else {
    display.print("      ");
  }
  display.display();
  
}
/* ----- End of functions to render data on the display ----- */


/* ----- Functions to generate plant data -----*/
void populatePlantData() {
  //populate plant database with some data. This is pretty inaccurate
  //TODO in real life the values must be properly adjusted
  plants->add(generatePlant("Tomatoes", 40, 60, 10, 40, 8*2000, 12*2000)); //assuming full sun at 2000 lumens
  plants->add(generatePlant("Cucumbers", 30, 50, 10, 40, 6*2000, 8*2000));
  plants->add(generatePlant("Cacti", 10, 60, 5, 60, 8*2000, 24*2000));
  Serial.println("Plant data populated");
}

Plant generatePlant(String name, float hmin, float hmax, float tmin, float tmax, float lmin, float lmax) {
  Serial.print("Generating data for plant:");
  Serial.println(name);
  Plant thePlant;
  thePlant.name = name;
  thePlant.soilHumidityMin = hmin;
  thePlant.soilHumidityMax = hmax;
  thePlant.soilTemperatureMin = tmin;
  thePlant.soilTemperatureMax = tmax;
  thePlant.sunMin = lmin;
  thePlant.sunMax = lmax;
  return(thePlant);
}
/* -----End of functions to generate plant data -----*/


/* ----- Functions to work with time -----*/
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/* ----- End of functions to work with time -----*/
