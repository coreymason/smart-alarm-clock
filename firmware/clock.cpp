#include "application.h"
#include "TimeAlarms.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_MCP9808.h"
#undef now()

bool isDST(int dayOfMonth, int month, int dayOfWeek, String rule);
void refreshDisplayTime();
void setAlarm(int hour, int minute, int second, bool isOnce);
void setAlarm(int hour, int minute, int second, bool isOnce, int dayOfWeek); //Sunday = 1
void SoundAlarm();

Adafruit_7segment display = Adafruit_7segment();
Adafruit_MCP9808 tempSensor = Adafruit_MCP9808();


const int ONE_DAY_MILLIS = 24 * 60 * 60 * 1000;
unsigned long lastSync = millis();
unsigned long lastBeep = millis();
int timeZone = -8; //UTC time zone offset TODO: Must be set/recieved from web/cloud
String DSTRule = "US"; //US, EU, or OFF TODO: Must be set/recieved from web/cloud
int DSTJumpHour; //When DST takes effect
int hourFormat = 12; //12 or 24 TODO: Must be set/recieved from web/cloud
double temp;
bool alarm = false;

int piezoPin = A4;

void setup() {
  if(DSTRule == "US") {
    DSTJumpHour = 2;
  } else if(DSTRule == "EU") {
    DSTJumpHour = 1+timeZone;
  } else {
    DSTJumpHour = 0;
  }

  //Set the proper time zone according to DST status
  Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), DSTRule) ? timeZone+1 : timeZone);

  //Setup the 7-segment display
  display.begin(0x70);
  display.setBrightness(15); //TODO: adjust brightness to optimize for veneer/time

  //Setup the temperature sensor
  tempSensor.setResolution(MCP9808_SLOWEST);

  //Cloud variables
  Spark.variable("Temperature", &temp, DOUBLE);
}

void loop() {
  //Request time synchronization from the Spark Cloud every 24 hours
  if (millis() - lastSync > ONE_DAY_MILLIS) {
    Spark.syncTime();
    lastSync = millis();
  }

  //Update time zone at DSTJumpHour incase DST is now in effect
  if(Time.hour() == DSTJumpHour && Time.minute() == 0) {
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), DSTRule) ? timeZone+1 : timeZone);
  }

  refreshDisplayTime();
  temp = tempSensor.getTemperature();

  //Sound alarm check
  if(alarm) {
    if(millis() - lastBeep > 600) {
      lastBeep = millis();
      tone(piezoPin, 4000, 300);
    }
  }

  //Delay for checking alarms/timers
  Alarm.delay(1);
}

//Return true if DST is currently observed
bool isDST(int dayOfMonth, int month, int dayOfWeek, String rule) {
  if(rule == "US") {
    //Month quick check
    if (month < 3 || month > 11) {
      return false;
    } else if (month > 3 && month < 11) {
      return true;
    }
    //The US observes DST from the first Sunday of March
    //The US observes DST until the first Sunday of November
    int previousSunday = dayOfMonth - dayOfWeek + 1;
    if (month == 3) {
      return previousSunday >= 1;
    } else {
      return previousSunday <= 0;
    }
  } else if(rule == "EU") {
    //Month quick check
    if (month < 3 || month > 10) {
      return false;
    } else if (month > 3 && month < 10) {
      return true;
    }
    //The EU observes DST from the last Sunday of March
    //The EU observes DST until the last Sunday of November
    int previousSunday = dayOfMonth - dayOfWeek + 1;
    if(month == 3) {
      if(previousSunday+7>31) {
        return true;
      } else {
        return false;
      }
    } else {
      if(previousSunday+7>31) {
        return false;
      } else {
        return true;
      }
    }
  } else {
    return false;
  }
}

//display the current time on the 7-segment display
void refreshDisplayTime() {
  unsigned long currentTime = Time.now();
  int minute = Time.minute(currentTime);
  int hour;
  int digits;

  if(hourFormat == 12) {
    hour = Time.hourFormat12(currentTime);
  } else {
    hour = Time.hour(currentTime);
  }

  digits = hour/10*1000 + hour%10*100 + minute/10*10 + minute%10;

  display.print(digits);
  display.drawColon(true);
  display.writeDisplay();
}

void setAlarm(int hour, int minute, int second, bool isOnce) {
  if(isOnce) {
    Alarm.alarmOnce(hour, minute, second, SoundAlarm);
  } else {
    Alarm.alarmRepeat(hour, minute, second, SoundAlarm);
  }
}

void setAlarm(int hour, int minute, int second, bool isOnce, int dayOfWeek) {
  if(isOnce) {
    Alarm.alarmOnce(dayOfWeek, hour, minute, second, SoundAlarm);
  } else {
    Alarm.alarmRepeat(dayOfWeek, hour, minute, second, SoundAlarm);
  }
}

void SoundAlarm() {
  alarm = true;
}
