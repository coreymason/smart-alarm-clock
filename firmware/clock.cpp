#include "application.h"
#include "TimeAlarms.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_MCP9808.h"
#include <vector>
#undef now()

bool isDST(int dayOfMonth, int month, int dayOfWeek, String rule);
void refreshDisplayTime();
void addAlarms();
void markDelete(int hour, int minute);
int checkDelete(AlarmID_t ID);
void setAlarm(int hour, int minute, int second, bool isOnce, bool light, bool sound, int dayOfWeek); //ignore = -1, Sunday = 1, etc
void SoundAlarm();
void LightAlarm();
void lightFadeIn(int hour, int minute);

Adafruit_7segment display = Adafruit_7segment();
Adafruit_MCP9808 tempSensor = Adafruit_MCP9808();

const int ONE_DAY_MILLIS = 24 * 60 * 60 * 1000;
unsigned long lastSync = millis();
unsigned long lastBeep = millis();
int DSTJumpHour; //When DST takes effect
double temp;
bool alarm = false;

struct Preferences {
  int timeZone; //UTC time zone offset
  int hourFormat; //12 or 24
  int piezoHertz; //Piezo alarm frequency
  String DSTRule; //US, EU, or OFF
  std::vector<std::vector<int>> alarmTimes; // {hour, minute, second, isOnce, light, sound, dayOfWeek, del}
} preferences;

int piezoPin = A4;

void setup() {
  if(EEPROM.read(0) == 117) {
    EEPROM.get(1, preferences);
  } else {
    preferences = {-8, 12, 2000, "US"};
    EEPROM.put(1, preferences);
    EEPROM.put(0, 117);
  }

  if(preferences.DSTRule == "US") {
    DSTJumpHour = 2;
  } else if(preferences.DSTRule == "EU") {
    DSTJumpHour = 1 + preferences.timeZone;
  } else {
    DSTJumpHour = 0;
  }

  //Set the proper time zone according to DST status
  Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), preferences.DSTRule) ? preferences.timeZone + 1 : preferences.timeZone);

  //Setup the 7-segment display
  display.begin(0x70);
  display.setBrightness(15); //TODO: adjust brightness to optimize for veneer/time

  //Add alarms stored in eeprom
  addAlarms();

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
    Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), preferences.DSTRule) ? preferences.timeZone + 1 : preferences.timeZone);
  }

  refreshDisplayTime();
  temp = tempSensor.getTemperature();

  //Sound alarm check
  if(alarm) {
    if(millis() - lastBeep > 600) {
      lastBeep = millis();
      tone(piezoPin, preferences.piezoHertz, 300);
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

  if(preferences.hourFormat == 12) {
    hour = Time.hourFormat12(currentTime);
  } else {
    hour = Time.hour(currentTime);
  }

  digits = hour/10*1000 + hour%10*100 + minute/10*10 + minute%10;

  display.print(digits);
  display.drawColon(true);
  display.writeDisplay();
}

//Add alarms from eeprom
void addAlarms() {
  for(int i=0;i<preferences.alarmTimes.size();i++) {
    setAlarm(preferences.alarmTimes.at(i).at(0), preferences.alarmTimes.at(i).at(1), preferences.alarmTimes.at(i).at(2), preferences.alarmTimes.at(3),
     preferences.alarmTimes.at(i).at(4), preferences.alarmTimes.at(i).at(5), preferences.alarmTimes.at(i).at(6));
  }
}

//Mark an alarm for deletion
void markDelete(int day, int hour, int minute) {
  for(int i=0;i<preferences.alarmTimes.size();i++) {
    if(preferences.alarmTimes.at(i).at(6) == day && preferences.alarmTimes.at(i).at(0) == hour && preferences.alarmTimes.at(i).at(1) == minute) {
      preferences.alarmTimes.at(i).at(7) = true;
    }
  }
}

//Check and if marked, delete the alarm from eeprom and TimeAlarms
//Returns 0 if deleted, -1 if not found, or the vector index
int checkDelete(AlarmID_t ID) {
  unsigned long currentTime = Time.now();
  int hour = Time.hour(currentTime);
  int minute = Time.minute(currentTime);
  int alarmIndex = -1;

  for(int i=0;i<preferences.alarmTimes.size();i++) {
    if(preferences.alarmTimes.at(i).at(0) == hour && preferences.alarmTimes.at(i).at(1) == minute) {
      alarmIndex = i;
      if(preferences.alarmTimes.at(i).at(7) == true) {
        Alarm.free(ID);
        preferences.alarmTimes.erase(i);
        return 0;
      }
    }
  }
  return alarmIndex;
}

//Set an alarm according to the given parameters
void setAlarm(int hour, int minute, int second, bool isOnce, bool light, bool sound, int dayOfWeek) {
  unsigned long currentTime = Time.now();
  int currentDay = Time.weekday(currentTime);
  int currentHour = Time.hour(currentTime);
  int currentMinute = Time.minute(currentTime);
  int day2 = dayOfWeek;
  int hour2 = hour;
  int minute2 = minute;

  //set a LightAlarm to go off 30 minutes early if light is enabled
  if(light) {
    minute2 -= 30;
    if(minute2 < 0) {
      minute2 = 60 + minute2;
      hour2 -= 1;
      if(hour2 < 0) {
        hour2 = 23;
        day2 -= 1;
        if(day2 < 1) {
          day2 = 7;
        }
      }
    }

    bool enoughTime = true;
    currentMinute -= 30;
    if(currentMinute < 0) {
      currentMinute = 60 + currentMinute;
      currentHour -= 1;
      if(currentHour < 0) {
        currentHour = 23;
        currentDay -= 1;
        if(currentDay < 1) {
          currentDay = 7;
        }
      }
    }
    if(currentDay == day2 && currentHour >= hour2 && currentMinute >= minute2) {
      enoughTime = false;
    }

    if(!enoughTime) {

    }
    if(isOnce) {
      if(!enoughTime) {
        markDelete(dayOfWeek, hour, minute);
      } else if(dayOfWeek != -1) {
        Alarm.alarmOnce(day2, hour2, minute2, second, LightAlarm);
      } else {
        Alarm.alarmOnce(hour2, minute2, second, LightAlarm);
      }
    } else {
      if(dayOfWeek != -1) {
        Alarm.alarmRepeat(day2, hour2, minute2, second, LightAlarm);
      } else {
        Alarm.alarmRepeat(hour2, minute2, second, LightAlarm);
      }
    }
  }

  //set a SoundAlarm if sound is enabled or if both sound and light are disabled
  if(sound || (!light && !sound)) {
    if(isOnce) {
      if(dayOfWeek != -1) {
        Alarm.alarmOnce(dayOfWeek, hour, minute, second, SoundAlarm);
      } else {
        Alarm.alarmOnce(hour, minute, second, SoundAlarm);
      }
    } else {
      if(dayOfWeek != -1) {
        Alarm.alarmRepeat(dayOfWeek, hour, minute, second, SoundAlarm);
      } else {
        Alarm.alarmRepeat(hour, minute, second, SoundAlarm);
      }
    }
  }

  preferences.lightAlarmTimes.push_back(hour, minute, second, isOnce, light, sound, dayOfWeek, false);
}

void SoundAlarm() {
  int alarmIndex = checkDelete(Alarm.getTriggeredAlarmId());
  if(alarmIndex == 0) {
    return;
  }
  alarm = true;
}

void LightAlarm() {
  int alarmIndex = checkDelete(Alarm.getTriggeredAlarmId());
  if(alarmIndex == 0) {
    return;
  }
  lightFadeIn(preferences.alarmTimes.at(alarmIndex).at(0), preferences.alarmTimes.at(alarmIndex).at(1));
}

void lightFadeIn(int hour, int minute) {
  //start led fade in to given time
}
