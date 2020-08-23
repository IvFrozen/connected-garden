/***************callback.INO********************
 * This is the system controller MQTT callback *
 ***********************************************/
 
void callback(char* topic, byte* payload, unsigned int length) {
   Serial.println("Received MQTT message");
   StaticJsonDocument<256> doc;

   DeserializationError err = deserializeJson(doc, payload, length); //first try to deserialize message payload

  if (err) { //well what was it?
    Serial.println("deserializeJson() failed, are you sure this message is JSON formatted?");
    Serial.print("Error code: ");
    Serial.println(err.c_str());
    return;
  }

 
  if (strcmp(topic, humFeed) == 0) { //process soil moisture message
    //process soil humidity data
    Serial.println("Processing soil humidity data");
    currentHum = doc["humidity"].as<float>();
    prevHumT=millis();
  }

  
   else if (strcmp(topic, tempFeed) == 0) { //process soil temperature message
    //process soil temperature
        Serial.println("Processing soil temperature data");
    currentTemp = doc["temperature"].as<float>();
    prevTempT=millis();
  }
  

  else if (strcmp(topic, lightFeed) == 0) { //process lumenosity data message
    //process light sensor data
    Serial.println("Processing light data");
    currentLum=doc["lumenosity"].as<float>();
    intLum+=currentLum*(millis()-prevLumT)/1000/3600; //calculate integral lumenosity (lumen*hours)
  }
}
