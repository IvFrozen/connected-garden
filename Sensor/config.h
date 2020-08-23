#define humFeed "ivkhrul/soil/humidity"    // the name of the feed to publish to
#define tempFeed "ivkhrul/soil/temperature" // the name of the feed to publish to
#define lightFeed "ivkhrul/soil/lux" // the name of the feed to publish to
#define SECONDS 1e6
#define MINUTES 60*SECONDS
const String location="Sammamish";
const float timeScale=0.1; //used to speed up the system operation. set to 1 for normal realtime
const float sensorRefreshInterval = 10*MINUTES*timeScale; //refresh sensor readings every 10 minutes
