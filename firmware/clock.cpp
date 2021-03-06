#include "application.h"
#include "TimeAlarms.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_MCP9808.h"
#include "Adafruit_TPA2016.h"
#undef now()
#undef swap()
#include <vector>

enum DSTRule {DST_US, DST_EU, DST_OFF};

bool isDST(int dayOfMonth, int month, int dayOfWeek, DSTRule rule);
void refreshDisplayTime();
void updatePreferences();
void addAlarms();
void updateAlarmString();
void markDelete(int hour, int minute);
int checkDelete(AlarmID_t ID);
int deleteAlarm(String command);
int createAlarm(String command);
void setAlarm(int hour, int minute, int second, bool isOnce, bool light, bool sound, int dayOfWeek); //ignore = -1, Sunday = 1, etc
void SoundAlarm();
void LightAlarm();
void lightFadeIn(int hour, int minute);
void alarmOff();

Adafruit_7segment display = Adafruit_7segment();
Adafruit_MCP9808 tempSensor = Adafruit_MCP9808();
Adafruit_TPA2016 audioAmp = Adafruit_TPA2016();

const int ONE_DAY_MILLIS = 24 * 60 * 60 * 1000;
unsigned long lastSync = millis();
unsigned long lastBeep = millis();
unsigned long lightS;
unsigned long lightF;
int maxBrightness;
int DSTJumpHour; //When DST takes effect
int driverCurrent = 1000; //Driver current output (in mA)
double temp;
String alarmString;
String testString;
bool soundAlarm = false;
bool lightAlarm = false;
std::vector<std::vector<int>> vectorAlarmTimes;

struct Preferences {
  int version;
  int timeZone; //UTC time zone offset
  int hourFormat; //12 or 24
  int piezoHertz; //Piezo alarm frequency
  int LEDCurrent; //LED current rating (in mA)
  int LEDBrightness; //Maximum brightness (0 to 100%)
  DSTRule dstRule; //US, EU, or OFF
  int alarmTimes[32][8]; //{hour, minute, second, isOnce, light, sound, dayOfWeek, del}
};

Preferences preferences;

int piezoPin = A4;
int LEDPin = A7;

void setup() {
  //EEPROM.clear();
  testString = "zo345566";
  EEPROM.get(0, preferences);
  testString += "get";
  if(preferences.version != 0) {
    preferences.version = 0;
    preferences.timeZone = -8;
    preferences.hourFormat = 12;
    preferences.piezoHertz = 2000;
    preferences.LEDCurrent = 700;
    preferences.LEDBrightness = 100;
    preferences.dstRule = DST_US;
    for(int i=0;i<32;++i) {
      preferences.alarmTimes[i][0] = -1;
    }
    EEPROM.put(0, preferences);
    testString += "put";
  }

  //Converts preferences.alarmTimes to vectorAlarmTimes
  for(int i=0;i<32;++i) {
    if(preferences.alarmTimes[i][0] != -1) {
      std::vector<int> temp;
      temp.push_back(preferences.alarmTimes[i][0]);
      temp.push_back(preferences.alarmTimes[i][1]);
      temp.push_back(preferences.alarmTimes[i][2]);
      temp.push_back(preferences.alarmTimes[i][3]);
      temp.push_back(preferences.alarmTimes[i][4]);
      temp.push_back(preferences.alarmTimes[i][5]);
      temp.push_back(preferences.alarmTimes[i][6]);
      temp.push_back(preferences.alarmTimes[i][7]);
      vectorAlarmTimes.push_back(temp);
    }
  }

  testString += preferences.timeZone;
  testString += preferences.hourFormat;
  testString += preferences.piezoHertz;
  testString += preferences.LEDCurrent;
  testString += preferences.LEDBrightness;
  testString += preferences.dstRule;

  if(preferences.dstRule == DST_US) {
    DSTJumpHour = 2;
  } else if(preferences.dstRule == DST_EU) {
    DSTJumpHour = 1 + preferences.timeZone;
  } else {
    DSTJumpHour = 0;
  }

  //Set the proper time zone according to DST status
  Time.zone(isDST(Time.day(), Time.month(), Time.weekday(), preferences.dstRule) ? preferences.timeZone + 1 : preferences.timeZone);
  //testString += isDST(Time.day(), Time.month(), Time.weekday(), preferences.dstRule);

  //Setup the 7-segment display, temperature sensor, and audio amp
  display.begin(0x70);
  display.setBrightness(15); //TODO: adjust brightness to optimize for veneer/time
  tempSensor.begin();
  tempSensor.setResolution(MCP9808_SLOWEST);
  audioAmp.begin();

  //Pin setup
  pinMode(LEDPin, OUTPUT);

  //Add alarms stored in eeprom
  addAlarms();

  //Cloud functions and variables
  Particle.function("CreateAlarm", createAlarm);
  Particle.function("DeleteAlarm", deleteAlarm);
  Particle.variable("Temperature", temp);
  Particle.variable("alarmString", alarmString);
  Particle.variable("testString", testString);
}

void loop() {
  unsigned long currentTime = Time.now();

  //Request time synchronization from the Particle Cloud every 24 hours
  if (millis() - lastSync > ONE_DAY_MILLIS) {
    Particle.syncTime();
    lastSync = millis();
  }

  //Update time zone at DSTJumpHour incase DST is now in effect
  if(Time.hour(currentTime) == DSTJumpHour && Time.minute(currentTime) == 0) {
    Time.zone(isDST(Time.day(currentTime), Time.month(currentTime), Time.weekday(currentTime), preferences.dstRule) ? preferences.timeZone + 1 : preferences.timeZone);
  }

  //Update the 7-segment display
  refreshDisplayTime();

  //Record data from sensors every 5 minutes
  if(Time.minute(currentTime) % 5) {
    temp = tempSensor.getTemperature();
    //TODO: Send and/or record data
  }
  //Testing
  if(Time.minute(currentTime) % 1) {
    temp = tempSensor.getTemperature();
    //TODO: Send and/or record data
  }

  //Sound alarm check
  if(soundAlarm) {
    if(millis() - lastBeep > 600) {
      lastBeep = millis();
      tone(piezoPin, preferences.piezoHertz, 300);
    }
  }

  //Light alarm check
  if(lightAlarm) {
    analogWrite(LEDPin, min(maxBrightness, ((currentTime - lightS) / (lightF - lightS)) * maxBrightness));
  }

  //Delay for checking alarms/timers
  Alarm.delay(1);
}

//Return true if DST is currently observed
bool isDST(int dayOfMonth, int month, int dayOfWeek, DSTRule rule) {
  if(rule == DST_US) {
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
  } else if(rule == DST_EU) {
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

//Update the alarmTimes array in preferences with vectorAlarmTimes
void updatePreferences() {
  for(int i=0;i<32;++i) {
    for(int j=0;j<8;++j) {
      if(i < vectorAlarmTimes.size()) {
        preferences.alarmTimes[i][j] = vectorAlarmTimes.at(i).at(j);
      } else {
        if(j == 0) {
          preferences.alarmTimes[i][j] = -1;
        } else {
          preferences.alarmTimes[i][j] = 0;
        }
      }
    }
  }
}

//Add alarms from eeprom
void addAlarms() {
  for(int i=0;i<vectorAlarmTimes.size();++i) {
    setAlarm(vectorAlarmTimes.at(i).at(0), vectorAlarmTimes.at(i).at(1), vectorAlarmTimes.at(i).at(2),
      vectorAlarmTimes.at(i).at(3), vectorAlarmTimes.at(i).at(4), vectorAlarmTimes.at(i).at(5), vectorAlarmTimes.at(i).at(6));
  }
}

//Updates alarmString so alarms can be read via cloud
//hh.mm.ss.b.b.b.d (b = 1/0) (d = - or 1,2,3,4,5,6,7)
void updateAlarmString() {
  alarmString = "";
  for(int i=0;i<vectorAlarmTimes.size();++i) {
    if(vectorAlarmTimes.at(i).at(7) == true) {
      continue;
    }
    String temp = "";
    temp += vectorAlarmTimes.at(i).at(0);
    temp += ".";
    temp += vectorAlarmTimes.at(i).at(1);
    temp += ".";
    temp += vectorAlarmTimes.at(i).at(2);
    temp += ".";
    temp += vectorAlarmTimes.at(i).at(3);
    temp += ".";
    temp += vectorAlarmTimes.at(i).at(4);
    temp += ".";
    temp += vectorAlarmTimes.at(i).at(5);
    temp += ".";
    if(vectorAlarmTimes.at(i).at(6) == -1) {
      temp += "-";
    } else {
      temp += vectorAlarmTimes.at(i).at(6);
    }
    alarmString += temp;
  }
}

//Mark an alarm for deletion
void markDelete(int day, int hour, int minute) {
  for(int i=0;i<vectorAlarmTimes.size();++i) {
    if(vectorAlarmTimes.at(i).at(6) == day && vectorAlarmTimes.at(i).at(0) == hour && vectorAlarmTimes.at(i).at(1) == minute) {
      vectorAlarmTimes.at(i).at(7) = true;
      updatePreferences();
      updateAlarmString();
    }
  }
}

//Check if marked, if so delete the alarm from eeprom and alarmTimes
//Returns 0 if deleted, -1 if not found, or the array index
int checkDelete(AlarmID_t ID) {
  unsigned long currentTime = Time.now();
  int hour = Time.hour(currentTime);
  int minute = Time.minute(currentTime);
  int alarmIndex = -1;

  for(int i=0;i<vectorAlarmTimes.size();++i) {
    if(vectorAlarmTimes.at(i).at(0) == hour && vectorAlarmTimes.at(i).at(1) == minute) {
      alarmIndex = i;
      if(vectorAlarmTimes.at(i).at(7)== true) {
        Alarm.free(ID);
        vectorAlarmTimes.erase(vectorAlarmTimes.begin() + i);
        updatePreferences();
        updateAlarmString();
        return -2;
      }
    }
  }
  return alarmIndex;
}

//Deletes an alarm from the cloud
//hh.mm.dd
int deleteAlarm(String command) {
  if(command.length() != 7 || command.length() != 8) {
    return -1;
  }
  int hour = command.substring(0,2).toInt();
  int minute = command.substring(3,5).toInt();
  int day = command.substring(6).toInt();
  markDelete(day, hour, minute);
  return 1;
}

//Creates an alarm from the cloud
//hh.mm.ss.b.b.b.d (b = 1/0) (d = - or 1,2,3,4,5,6,7)
int createAlarm(String command) {
  if(command.length() != 16) {
    return -1;
  }
  int hour = command.substring(0,2).toInt();
  int minute = command.substring(3,5).toInt();
  int second = command.substring(6,8).toInt();
  int isOnce = command.substring(9,10).toInt();
  int light = command.substring(11,12).toInt();
  int sound = command.substring(13,14).toInt();
  int dayOfWeek;
  if(command.substring(15) == "-") {
    dayOfWeek = -1;
  } else {
    dayOfWeek = command.substring(15).toInt();
  }
  setAlarm(hour, minute, second, isOnce, light, sound, dayOfWeek);
  return 1;
}

//TODO: Make sure no larger than array size 32
//Set an alarm according to the given parameters
void setAlarm(int hour, int minute, int second, bool isOnce, bool light, bool sound, int dayOfWeek) {
  unsigned long currentTime = Time.now();
  int currentDay = Time.weekday(currentTime);
  int currentHour = Time.hour(currentTime);
  int currentMinute = Time.minute(currentTime);
  int day2 = dayOfWeek;
  int hour2 = hour;
  int minute2 = minute;

  //Checks if duplicate marked for deletion exists, if so unmark delete
  for(int i=0;i<vectorAlarmTimes.size();++i) {
    if(vectorAlarmTimes.at(i).at(0) == hour && vectorAlarmTimes.at(i).at(1) == minute) {
      if(vectorAlarmTimes.at(i).at(7) == false) {
        return;
      } else if(vectorAlarmTimes.at(i).at(3) == isOnce && vectorAlarmTimes.at(i).at(4) == light &&
          vectorAlarmTimes.at(i).at(5) == sound && vectorAlarmTimes.at(i).at(6) == dayOfWeek) {
        vectorAlarmTimes.at(i).at(7) = true;
        updateAlarmString();
        return;
      }
    }
  }

  //Get the time a LightAlarm would go off (30 minutes early if light is enabled)
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

    //Check if there is enough time and handle accordingly
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
      lightFadeIn(hour, minute);
    }
    if(isOnce) {
      if(enoughTime) {
        if(dayOfWeek != -1) {
          Alarm.alarmOnce(day2, hour2, minute2, second, LightAlarm);
        } else {
          Alarm.alarmOnce(hour2, minute2, second, LightAlarm);
        }
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

  std::vector<int> temp;
  temp.push_back(hour);
  temp.push_back(minute);
  temp.push_back(second);
  temp.push_back(isOnce);
  temp.push_back(light);
  temp.push_back(sound);
  temp.push_back(dayOfWeek);
  temp.push_back(false);
  vectorAlarmTimes.push_back(temp);
  updatePreferences();
  updateAlarmString();
}

void SoundAlarm() {
  //TODO: Check/Debug if this can even run - one time alarm may have already been cleared/deleted by now
  int alarmID = Alarm.getTriggeredAlarmId();
  int alarmIndex = checkDelete(alarmID);
  //do nothing if it was just deleted (in checkDelete())
  if(alarmIndex == -2) {
    return;
  }
  soundAlarm = true;
  //delete alarm from the array if it is a one time alarm
  if(vectorAlarmTimes.at(alarmIndex).at(3) == true) {
    vectorAlarmTimes.erase(vectorAlarmTimes.begin() + alarmIndex);
    updatePreferences();
    updateAlarmString();
  }
}

void LightAlarm() {
  //TODO: Check/Debug if this can even run - one time alarm may have already been cleared/deleted by now
  int alarmID = Alarm.getTriggeredAlarmId();
  int alarmIndex = checkDelete(alarmID);
  //do nothing if it was just deleted (in checkDelete())
  if(alarmIndex == -2) {
    return;
  }
  lightFadeIn(vectorAlarmTimes.at(alarmIndex).at(0), vectorAlarmTimes.at(alarmIndex).at(1));
  //delete alarm from the array if it is a one time alarm
  if(vectorAlarmTimes.at(alarmIndex).at(3) == true) {
    vectorAlarmTimes.erase(vectorAlarmTimes.begin() + alarmIndex);
    updatePreferences();
    updateAlarmString();
  }
}

//Start light fade in to a given time
void lightFadeIn(int hour, int minute) {
  lightS = Time.now();
  int difference = (hour*60 + minute) - (Time.hour(lightS)*60 + Time.minute(lightS));
  if(difference < 0) {
    difference += 24*60;
  }
  lightF = lightS + difference*60;
  maxBrightness = 4095*(preferences.LEDCurrent/driverCurrent)*(preferences.LEDBrightness/100);
  lightAlarm = true;
}

//Turn off all active alarms
void alarmOff() {
  soundAlarm = false;
  lightAlarm = false;
}
