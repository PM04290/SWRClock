/*
  Designed for ESP32
  Partition scheme : specific, see below

  ----------------------------------------------------------------------------------------------------------------------
> need to create dedicated partition scheme (copy/paste default.csv in partition directory and rename to mypartition.csv)
  edit this new file and put data below
  
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x170000,
app1,     app,  ota_1,   0x180000,0x170000,
spiffs,   data, spiffs,  0x2F0000,0x100000,
coredump, data, coredump,0x3F0000,0x10000,

> modify board.txt to add new menu (esp????? is the board you want to modify)

esp32?????.menu.PartitionScheme.mypartition=My partition (App/OTA 1.5MB SPIFFS 900K)
esp32?????.menu.PartitionScheme.mypartition.build.partitions=mypartition
esp32?????.menu.PartitionScheme.mypartition.upload.maximum_size=1507328

example : https://iotespresso.com/how-to-set-partitions-in-esp32/
  ----------------------------------------------------------------------------------------------------------------------

*/
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

#include "esp_heap_caps.h"

#define WITH_METEO

//#define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
#define DEBUG(x) Serial.print(x)
#define DEBUGln(x) Serial.println(x)
#define DEBUGf(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG(x)
#define DEBUGln(x)
#define DEBUGf(...)
#endif

#define LGFX_USE_V1
#include "src/fonts-verdana-regular.h"
#include <LovyanGFX.hpp>
#include "config.h"

LGFX lcd;

hw_timer_t* secTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool topSecond = false;
bool needDrawing = true;

StaticJsonDocument<JSON_MAX_SIZE> configFile;
JsonVariant ntpConfig;
JsonVariant tzConfig;
JsonArray stationArray;
JsonObject weatherConfig;

uint8_t tmpIcon[MAX_ICON_SIZE];

int hours = 0;
int minutes = 0;
int seconds = 0;

byte lumLevel0 = 20;
byte lumLevel1 = 100;
byte lumSens  = 50;
byte lumOut   = 0;
byte clkR = 64;
byte clkG = 64;
byte clkB = 255;

int rain1 = 0;
int rain2 = 0;
int rain3 = 0;

#include "Xcontroller.hpp"
Xcontroller control;

#include "network.hpp"
#include "web.hpp"
#include "sound.hpp"

#ifdef WITH_METEO
#include "meteo.hpp"
#endif

void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps, const char *function_name)
{
#ifdef DEBUG_SERIAL
  printf("!!! %s failed to allocate %d bytes with 0x%X capabilities. \n", function_name, requested_size, caps);
  printf("heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  lcd.drawRect(0, 0, lcd.width(), lcd.height(), COLOR_RED);
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  topSecond = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void listDir(const char * dirname)
{
#ifdef DEBUG_SERIAL
  Serial.printf("Listing directory: %s\n", dirname);
  File root = SPIFFS.open(dirname);
  if (!root.isDirectory()) {
    Serial.println(F("No Dir"));
  }
  File file = root.openNextFile();
  while (file) {
    Serial.print(F("  FILE: "));
    Serial.print(file.name());
    Serial.print(F("\tSIZE: "));
    Serial.println(file.size());
    file = root.openNextFile();
  }
#endif
  DEBUGf("SPIFFS used %d / total %d\n", SPIFFS.usedBytes(), SPIFFS.totalBytes());
}

void onClockState(stateevent_t state)
{
  if (state == stateevent_t::clockAlarm)
  {
    wakeupVolume = 0;
    audio.setVolume(0);
    soundStart();
  }
  if (state == stateevent_t::saveAlarm)
  {
    byte h, m;
    bool active;
    control.getClock()->getAlarm(&h, &m, &active);
    EEPROM.write(EEPROM_ALARM_HR, h);
    EEPROM.write(EEPROM_ALARM_MN, m);
    EEPROM.write(EEPROM_ALARM_ON, active);
    EEPROM.commit();
    control.drawPopup("Alarm time stored");
  }
}

void onRadio(bool running)
{
  if (running && sound_TimeToStop)
  {
    control.getMenu(INDEX_BTN2)->setIcon(NULL);
    control.getMenu(INDEX_BTN2)->setText((String("#%") + String(100 * sound_TimeToStop / sound_SleepTimeTotal)).c_str());
  } else
  {
    control.getMenu(INDEX_BTN2)->setText(NULL);
    control.getMenu(INDEX_BTN2)->setIcon("/bmp/sleep.bmp");
  }
  control.getMenu(INDEX_BTN4)->setIcon(running ? "/bmp/pause.bmp" : "/bmp/play.bmp");
}

void onClockEditing(editingevent_t event)
{
  switch (event)
  {
    case editingevent_t::Start:
      control.getMenu(INDEX_BTN1)->setIcon("/bmp/down.bmp");
      control.getMenu(INDEX_BTN2)->setText(NULL);
      control.getMenu(INDEX_BTN2)->setIcon("/bmp/up.bmp");
      control.getMenu(INDEX_BTN4)->setIcon("/bmp/down.bmp");
      control.getMenu(INDEX_BTN5)->setIcon("/bmp/up.bmp");
      break;
    case editingevent_t::Stop:
      control.getClock()->saveAlarmTime();
    // continue below... (no break)
    case editingevent_t::Timeout:
      control.getMenu(INDEX_BTN1)->setIcon("/bmp/volume.bmp");
      if (sound_TimeToStop > 1)
      {
        control.getMenu(INDEX_BTN2)->setIcon(NULL);
        control.getMenu(INDEX_BTN2)->setText((String("#%") + String(soundPercentRemaining())).c_str());
        control.getMenu(INDEX_BTN4)->setIcon("/bmp/pause.bmp");
      } else {
        control.getMenu(INDEX_BTN2)->setText(NULL);
        control.getMenu(INDEX_BTN2)->setIcon("/bmp/sleep.bmp");
        control.getMenu(INDEX_BTN4)->setIcon("/bmp/play.bmp");
      }
      control.getMenu(INDEX_BTN5)->setIcon("/bmp/playlist.bmp");
      break;
  }
}

void onVolumeTouch(uint32_t idx, bool longpress, uint16_t tapcount)
{
  if (idx == 0)
  {
    if (soundVolume > 1) soundVolume--;
    audio.setVolume(soundVolume);
    control.drawPopup("Volume " + String(soundVolume) + "/21");
    EEPROM.write(EEPROM_VOLUME, soundVolume);
    EEPROM.commit();
    return;
  }
  if (idx == 1)
  {
    if (soundVolume < 21) soundVolume++;
    audio.setVolume(soundVolume);
    control.drawPopup("Volume " + String(soundVolume) + "/21");
    EEPROM.write(EEPROM_VOLUME, soundVolume);
    EEPROM.commit();
    return;
  }
}

void onListTouch(uint32_t idx, bool longpress, uint16_t tapcount)
{
  if (idx == 0)
  {
    soundStart(true);
  }
  if (idx == 1)
  {
    EEPROM.write(EEPROM_STATION, stationIndex);
    EEPROM.commit();
    control.drawPopup("Sation stored");
  }
}

void onItemTouch(uint32_t idx, bool longpress, uint16_t tapcount)
{
  static bool sel = false;
  bool clockEditing = control.getClock()->isEditing();
  switch (idx)
  {
    case INDEX_CLOCK:
      if (longpress)
      {
        if (clockEditing)
        {
          control.getClock()->stopEditing();
        } else {
          control.getClock()->startEditing();
        }
      } else {
        if (control.getClock()->getAlarmTriggered())
        {
          soundStop();
        }
        if (tapcount == 4)
        {
          bool active = !control.getClock()->getAlarmActive();
          control.getClock()->setAlarmActive(active);
          EEPROM.write(EEPROM_ALARM_ON, active);
          EEPROM.commit();
          if (active) {
            control.drawPopup("Alarm ON");
          } else
          {
            control.drawPopup("Alarm OFF");
          }
        }
      }
      break;
    case INDEX_BTN1:
      if (clockEditing)
      {
        control.getClock()->extendEditing();
        control.getClock()->moveAlarm(-1, 0);
      } else
      {
        bool opened = control.getMenu(INDEX_BTN1)->isSubmenuOpened();
        control.closeAllSubmenu();
        if (!opened)
        {
          control.openSubmenu(INDEX_BTN1);
        }
      }
      break;
    case INDEX_BTN2:
      if (clockEditing)
      {
        control.getClock()->extendEditing();
        control.getClock()->moveAlarm(1, 0);
      } else
      {
        control.closeAllSubmenu();
        soundStartDuration();
        control.drawPopup("Switch off in " + soundTimeRemaining());
      }
      break;
    case INDEX_BTN3:
      control.closeAllSubmenu();
      getMeteo();
      break;
    case INDEX_BTN4:
      if (clockEditing)
      {
        control.getClock()->extendEditing();
        control.getClock()->moveAlarm(0, -1);
      } else
      {
        control.closeAllSubmenu();
        if (audio.isRunning())
        {
          soundStop();
        } else
        {
          soundStart();
        }
      }
      break;
    case INDEX_BTN5:
      if (clockEditing)
      {
        control.getClock()->extendEditing();
        control.getClock()->moveAlarm(0, 1);
      } else
      {
        bool opened = control.getMenu(INDEX_BTN5)->isSubmenuOpened();
        control.closeAllSubmenu();
        if (!opened)
        {
          control.openSubmenu(INDEX_BTN5);
        }
      }
      break;
  }
}

void setup()
{
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  while (Serial.availableForWrite() == false)
  {
    delay(50);
  }
  printf("heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
  DEBUGln("\nDebug start");
  byte almH = 8;
  byte almM = 0;
  bool almOn = false;
  if (!EEPROM.begin(EEPROM_MAX_SIZE))
  {
    DEBUGln(F("failed to initialise EEPROM"));
  } else {
    // uncomment to initialisze EEPROM with code variable, juste one run with that
    //EEPROM.writeString(EEPROM_TEXT_OFFSET, Wifi_ssid);
    //EEPROM.writeString(EEPROM_TEXT_OFFSET + (EEPROM_TEXT_SIZE * 1), Wifi_pass);
    //EEPROM.commit();

    almH = EEPROM.read(EEPROM_ALARM_HR) % 24;
    almM = EEPROM.read(EEPROM_ALARM_MN) % 60;
    almOn = EEPROM.read(EEPROM_ALARM_ON) == 1 ? true : false;
    stationIndex = EEPROM.read(EEPROM_STATION);
    soundVolume = 1 + EEPROM.read(EEPROM_VOLUME) % 21;
    lumLevel0 = EEPROM.read(EEPROM_LUM_LVL0) % 128;
    lumLevel1 = EEPROM.read(EEPROM_LUM_LVL1) % 256;
    if (lumLevel1 < 128) lumLevel1 = 128;
    lumSens = EEPROM.read(EEPROM_LUM_SENS) % 101;
    lumOut = EEPROM.read(EEPROM_LUM_OUT) % 10;
    clkR = EEPROM.read(EEPROM_CLK_R) % 255;
    clkG = EEPROM.read(EEPROM_CLK_G) % 255;
    clkB = EEPROM.read(EEPROM_CLK_B) % 255;

    strcpy(Wifi_ssid, EEPROM.readString(EEPROM_TEXT_OFFSET).c_str());
    strcpy(Wifi_pass, EEPROM.readString(EEPROM_TEXT_OFFSET + EEPROM_TEXT_SIZE * 1).c_str());

    DEBUGln(Wifi_ssid);
    DEBUGln(Wifi_pass);
    DEBUGf("H:%d M:%d !%d St:%d V:%d L0:%d L1:%d Ls:%d Lo:%d R:%d G:%d B:%d\n", almH, almM, almOn, stationIndex, soundVolume, lumLevel0, lumLevel1, lumSens, lumOut, clkR, clkG, clkB);
  }

  if (!SPIFFS.begin())
  {
    DEBUGln(F("SPIFFS Mount failed"));
  } else
  {
    DEBUGln(F("SPIFFS Mount succesfull"));
    listDir("/");
  }

  // Setup the display.
  lcd.init();
  lcd.setRotation(1);
  lcd.setFont(&font_XS);
  size_t nb = readFile("/i/logo-64.png");
  if (nb)
  {
    int y = lcd.getCursorY();
    LGFX_Sprite sp(&lcd);
    sp.createSprite(64, 64);
    sp.drawPng(tmpIcon, nb, 0, 0);
    sp.pushSprite(lcd.width() / 2 - 32, y);
    sp.deleteSprite();
    lcd.setCursor(0, y + 64);
  }
  lcd.println("Smart WebRadio Clock by M&L");
  lcd.print("Version ");  lcd.println(VERSION);

  configWifi();

  LoadConfig();

  configWeb();

  // Setup controller
  DEBUGln(F("Setup controls"));
  control.begin(false); // No seconds displayed
  control.drawItems();
  // configure clock
  control.getClock()->setAlarm(almH, almM, almOn);
  control.getClock()->setCallbackTouch(onItemTouch);
  control.getClock()->setCallbackEditing(onClockEditing);
  control.getClock()->setCallbackState(onClockState);
  control.getClock()->setLongPressAuto(true); // long press auto action, to Set clock alarm
  control.getClock()->allowBlink(true);
  control.getClock()->setRainy(rain1, rain2, rain3);
  // configure menu buttons
  control.getMenu(INDEX_BTN1)->setIcon("/bmp/volume.bmp");
  control.getMenu(INDEX_BTN1)->setCallbackTouch(onItemTouch);
  control.getMenu(INDEX_BTN1)->addSubmenu(onVolumeTouch, "/bmp/volumedown.bmp");
  control.getMenu(INDEX_BTN1)->addSubmenu(onVolumeTouch, "/bmp/volumeup.bmp");

  control.getMenu(INDEX_BTN2)->setIcon("/bmp/sleep.bmp");
  control.getMenu(INDEX_BTN2)->setCallbackTouch(onItemTouch);

  control.getMenu(INDEX_BTN3)->setCallbackTouch(onItemTouch);

  control.getMenu(INDEX_BTN4)->setIcon("/bmp/play.bmp");
  control.getMenu(INDEX_BTN4)->setCallbackTouch(onItemTouch);

  control.getMenu(INDEX_BTN5)->setIcon("/bmp/playlist.bmp");
  control.getMenu(INDEX_BTN5)->setCallbackTouch(onItemTouch);
  control.getMenu(INDEX_BTN5)->addSubmenu(onListTouch, "/bmp/forward.bmp");
  control.getMenu(INDEX_BTN5)->addSubmenu(onListTouch, "/bmp/pin.bmp");

  DEBUGln(F("Get NTP"));
  if (getNTP(hours, minutes, seconds))
  {
    //
  }

  // define time ISR
  secTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(secTimer, &onTimer, true);
  timerAlarmWrite(secTimer, 1000000, true);
  timerAlarmEnable(secTimer);
  //
  DEBUGln(F("Init sound"));
  soundInit(onRadio);
#ifdef WITH_METEO
  DEBUGln("Get meteo");
  if (getMeteo())
  {
    control.getWeather()->setWeather(temperature, precipitation, weatherCode, getBmpByCode(weatherCode));
  }
  printf("heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
  DEBUGln(F("Init done"));
  DEBUGf("heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void loop()
{
  bool needNTP = false;
  bool needMeteo = false;
  bool blink = false;
  if (topSecond)
  {
    portENTER_CRITICAL(&timerMux);
    topSecond = false;
    portEXIT_CRITICAL(&timerMux);
    blink = true;
    //
    seconds++;
    if (seconds >= 60)
    {
      minutes++;
      seconds = 0;
      needDrawing = true;
      needNTP = wifiok && (minutes == 1); // scan NTP time every xx:01:00 (fix start problem)
#ifdef WITH_METEO
      needMeteo = (minutes % 20) == 8;
#endif
    }
    if (minutes >= 60)
    {
      minutes = 0;
      hours++;
    }
    if (hours >= 24)
    {
      hours = 0;
    }
    if (needNTP)
    {
      DEBUGf("old %02d:%02d:%02d\n", hours, minutes, seconds);
      if (!getNTP(hours, minutes, seconds))
      {
        control.drawPopup("Erreur mise à l'heure");
      }
      DEBUGf("new %02d:%02d:%02d\n", hours, minutes, seconds);
      needNTP = false;
    }
#ifdef WITH_METEO
    if (needMeteo)
    {
      if (getMeteo())
      {
        control.getWeather()->setWeather(temperature, precipitation, weatherCode, getBmpByCode(weatherCode));
      } else {
        control.drawPopup("Meteo error");
      }
      needMeteo = false;
    }
#endif
    if (needDrawing)
    {
      control.getClock()->setTime(hours, minutes, seconds, 60);
      printf("heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      needDrawing = false;
    }
    if (sound_TimeToStop > 1 && !control.getClock()->isEditing())
    {
      control.getMenu(INDEX_BTN2)->setText((String("#%") + String(soundPercentRemaining())).c_str());
    }
  }
  control.loop(blink);
  soundLoop(blink);
}

void LoadConfig()
{
  DEBUGln(F("Loading config file"));
  File file = SPIFFS.open("/config.json");
  if (!file || file.isDirectory())
  {
    DEBUGln(F("* failed to open config file"));
    return;
  }
  DeserializationError error = deserializeJson(configFile, file);
  file.close();
  if (error)
  {
    DEBUGln(F("* failed to deserialize config file"));
    return;
  }
  ntpConfig = configFile["ntp"];
  tzConfig = configFile["tz"];
  weatherConfig = configFile["weather"];
  if (weatherConfig.containsKey("rainy1") && weatherConfig.containsKey("rainy2") && weatherConfig.containsKey("rainy3"))
  {
    rain1 = weatherConfig["rainy1"].as<int>();
    rain2 = weatherConfig["rainy2"].as<int>();
    rain3 = weatherConfig["rainy3"].as<int>();
  }
  configTzTime(tzConfig ? tzConfig.as<const char*>() : defaultTZ, ntpConfig ? ntpConfig.as<const char*>() : defaultNTP);
  stationArray = configFile["stations"];
  if (stationIndex >= stationArray.size())
  {
    stationIndex = 0;
  }
  DEBUGf("%d stations found\n", stationArray.size());
  for (JsonVariant station : stationArray)
  {
    DEBUGln(station.as<const char*>());
  }
  DEBUGln(F("Config ready"));
}
