#include "application.h"

const int ONE_DAY_MILLIS = 24 * 60 * 60 * 1000;
unsigned long lastSync = millis();
int timeZone = -8; //UTC time zone TODO: Must be set/recieved from web/cloud

void setup() {
  //Set the proper time zone
  Time.zone(timeZone);
}

void loop() {
  //Request time synchronization from the Spark Cloud every 24 hours
  if (millis() - lastSync > ONE_DAY_MILLIS) {
    Spark.syncTime();
    lastSync = millis();
  }
}
