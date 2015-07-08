#include "application.h"

bool isDST(int dayOfMonth, int month, int dayOfWeek, String location);

const int ONE_DAY_MILLIS = 24 * 60 * 60 * 1000;
unsigned long lastSync = millis();
int timeZone = -8; //UTC time zone TODO: Must be set/recieved from web/cloud
String DSTRegion = "US"; //Location for checking DST - US (ignores exceptions), EU, and OFF TODO: Must be set/recieved from web/cloud, more DST support
int DSTJumpHour; //When DST takes effect

void setup() {
  //Set the DSTJumpHour according to the DSTRegion
  if(DSTRegion == "US") {
    DSTJumpHour = 2;
  } else if(DSTRegion == "EU") {
    DSTJumpHour = 1+timeZone;
  } else {
    DSTJumpHour = 0;
  }

  //Set the proper time zone
  Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), DSTRegion) ? timeZone+1 : timeZone);
}

void loop() {
  //Request time synchronization from the Spark Cloud every 24 hours
  if (millis() - lastSync > ONE_DAY_MILLIS) {
    Spark.syncTime();
    lastSync = millis();
  }

  //Update time zone at DSTJumpHour incase DST is now in effect
  if(Time.hour() == DSTJumpHour && Time.minute() == 0) {
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), DSTRegion) ? timeZone+1 : timeZone);
  }
}

//Return true if DST is currently observed
bool isDST(int dayOfMonth, int month, int dayOfWeek, String location) {
  if(location == "US") {
    //Month quick check
    if (month < 3 || month > 11)
      return false;
    else if (month > 3 && month < 11)
      return true;
    //The US observes DST from the first Sunday of March
    //The US observes DST until the first Sunday of November
    int previousSunday = dayOfMonth - dayOfWeek + 1;
    if (month == 3)
      return previousSunday >= 1;
    else
      return previousSunday <= 0;
  } else if(location == "EU") {
    //Month quick check
    if (month < 3 || month > 10)
      return false;
    else if (month > 3 && month < 10)
      return true;
    //The EU observes DST from the last Sunday of March
    //The EU observes DST until the last Sunday of November
    int previousSunday = dayOfMonth - dayOfWeek + 1;
    if(month == 3) {
      if(previousSunday+7>31)
        return true;
      else
        return false;
    } else {
      if(previousSunday+7>31)
        return false;
      else
        return true;
    }
  } else {
    return false;
  }
}
