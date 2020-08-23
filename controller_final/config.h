/***************config.h********************
 *         System configuration parameters     *
 ***********************************************/
#define humFeed "ivkhrul/soil/humidity"    // the name of the feed to publish to
#define tempFeed "ivkhrul/soil/temperature" // the name of the feed to publish to
#define lightFeed "ivkhrul/soil/lux" // the name of the feed to publish to
#define irr_controller_url "http://192.168.4.64/program.htm"
#define mqtt_server "mqtt.flespi.io" // the MQTT server address

#define SECONDS 1e3 
#define MINUTES 60*SECONDS
#define HOURS 60*MINUTES
const String location="Sammamish";
const float timeScale=0.1; //used to speed up the system operation. set to 1 for normal realtime
const float sensorRefreshInterval = 10*MINUTES*1000*timeScale; //refresh sensor readings every 10 minutes
const int nScreens =3; //number of different data views in main operation mode
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -7;     // Central European Time
const int wateringHours[2]={6, 23}; //water only around 6am or 11pm

typedef struct {
  String name;
  float soilHumidityMin;//water the plan if the humidity falls below this value
  float soilHumidityMax; //the soil is too moist above this point. Water unilt this level is reached
  float sunMin; //cumulitive minimum for sun exposure
  float sunMax; //cumulitive maximum for sun exposure
  float soilTemperatureMin; //soil is too cold below this point
  float soilTemperatureMax; //soil is too hot below this point, attempt cooling it off by watering
} Plant;
