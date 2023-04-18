#include <ESPAsyncWebServer.h>
#include <Update.h>

AsyncWebServer server(80);

void onIndexRequest(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  File f = SPIFFS.open("/index.html", "r");
  if (f)
  {
    String html;
    while (f.available())
    {
      html = f.readStringUntil('\n') + '\n';
      if (html.indexOf('%') > 0)
      {
        html.replace("%VERSION%", VERSION);

        html.replace("%LUMLVL0%", String(lumLevel0));
        html.replace("%LUMLVL1%", String(lumLevel1));
        html.replace("%LUMSENS%", String(lumSens));
        char hex[8] = {0};
        sprintf(hex, "#%02X%02X%02X", clkR, clkG, clkB);
        html.replace("%CLKRGB%", hex);
        html.replace("%SUNRGB0%", lumOut != 1 ? "checked" : "");
        html.replace("%SUNRGB1%", lumOut == 1 ? "checked" : "");

        html.replace("%WIFIMAC%", WiFi.macAddress());
        html.replace("%WIFISSID%", Wifi_ssid);
        html.replace("%WIFIPASS%", Wifi_pass);

        html.replace("%NTP%", ntpConfig ? ntpConfig.as<const char*>() : defaultNTP);
        html.replace("%TZ%", tzConfig ? tzConfig.as<const char*>() : defaultTZ);
        for (byte n = 0; n < 5; n++)
        {
          if (n < stationArray.size())
          {
            html.replace("%URL" + String(n + 1) + "%", stationArray[n].as<const char*>());
          } else {
            html.replace("%URL" + String(n + 1) + "%", "");
          }
        }
        if (weatherConfig)
        {
          html.replace("%LAT%", String(weatherConfig["lat"].as<float>()));
          html.replace("%LON%", String(weatherConfig["lon"].as<float>()));
          html.replace("%RAINY1%", String(weatherConfig["rainy1"].as<int>()));
          html.replace("%RAINY2%", String(weatherConfig["rainy2"].as<int>()));
          html.replace("%RAINY3%", String(weatherConfig["rainy3"].as<int>()));
        } else
        {
          html.replace("%LAT%", "33.145");
          html.replace("%LON%", "-31.455");
          html.replace("%RAINY1%", "0");
          html.replace("%RAINY2%", "0");
          html.replace("%RAINY3%", "0");
        }
      }
      response->print(html);
    }
    f.close();
  }
  request->send(response);
}

void onConfigRequest(AsyncWebServerRequest * request)
{
  if (request->hasParam("lumlvl0", true))
  {
    lumLevel0 = request->getParam("lumlvl0", true)->value().toInt();
    lumLevel1 = request->getParam("lumlvl1", true)->value().toInt();
    lumSens = request->getParam("lumsens", true)->value().toInt();
    lumOut = request->getParam("lumout", true)->value().toInt();
    long number = (long) strtol( request->getParam("clkrgb", true)->value().c_str() + 1, NULL, 16);
    clkR = number >> 16;
    clkG = number >> 8 & 0xFF;
    clkB = number & 0xFF;

    EEPROM.write(EEPROM_LUM_LVL0, lumLevel0);
    EEPROM.write(EEPROM_LUM_LVL1, lumLevel1);
    EEPROM.write(EEPROM_LUM_SENS, lumSens);
    EEPROM.write(EEPROM_LUM_OUT, lumOut);
    EEPROM.write(EEPROM_CLK_R, clkR);
    EEPROM.write(EEPROM_CLK_G, clkG);
    EEPROM.write(EEPROM_CLK_B, clkB);
    EEPROM.commit();
    control.getClock()->changeColor(SEGMENT_ON);
  }
  if (request->hasParam("wifissid", true))
  {
    strcpy(Wifi_ssid, request->getParam("wifissid", true)->value().c_str() );
    strcpy(Wifi_pass, request->getParam("wifipass", true)->value().c_str() );
    DEBUGln(Wifi_ssid);
    DEBUGln(Wifi_pass);
    EEPROM.writeString(EEPROM_TEXT_OFFSET, Wifi_ssid);
    EEPROM.writeString(EEPROM_TEXT_OFFSET + (EEPROM_TEXT_SIZE * 1), Wifi_pass);
    EEPROM.commit();
  }
  bool saveJson = false;
  if (request->hasParam("ntp", true))
  {
    ntpConfig.set(request->getParam("ntp", true)->value().c_str());
    tzConfig.set(request->getParam("tz", true)->value().c_str());
    stationArray[0].set(request->getParam("url1", true)->value().c_str());
    stationArray[1].set(request->getParam("url2", true)->value().c_str());
    stationArray[2].set(request->getParam("url3", true)->value().c_str());
    stationArray[3].set(request->getParam("url4", true)->value().c_str());
    stationArray[4].set(request->getParam("url5", true)->value().c_str());
    saveJson = true;
  }
  if (request->hasParam("lat", true))
  {
    weatherConfig["lat"].set(request->getParam("lat", true)->value().toFloat());
    weatherConfig["lon"].set(request->getParam("lon", true)->value().toFloat());
    weatherConfig["rainy1"].set(request->getParam("rainy1", true)->value().toInt() % 60);
    weatherConfig["rainy2"].set(request->getParam("rainy2", true)->value().toInt() % 60);
    weatherConfig["rainy3"].set(request->getParam("rainy3", true)->value().toInt() % 60);
    control.getClock()->setRainy(weatherConfig["rainy1"].as<int>(), weatherConfig["rainy2"].as<int>(), weatherConfig["rainy3"].as<int>());
    saveJson = true;
  }
  if (saveJson)
  {
    String Jres;
    size_t Lres = serializeJson(configFile, Jres);
    DEBUGln(Jres);

    File file = SPIFFS.open("/config.json", "w");
    if (file) {
      file.write((byte*)Jres.c_str(), Lres);
      file.close();
      DEBUGln(F("Config file saved"));
    } else {
      DEBUGln(F("Error saving config file"));
    }
  }
  request->send(200, "text/plain", "OK");
}

size_t content_len;
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    DEBUGln("Update start");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }
  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }

  if (final)
  {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true))
    {
      Update.printError(Serial);
    } else
    {
      DEBUGln("Update complete");
      delay(100);
      yield();
      delay(100);

      ESP.restart();
    }
  }
}

void handleDoFile(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    if (filename.endsWith(".bmp"))
    {
      request->_tempFile = SPIFFS.open("/bmp/" + filename, "w");
    } else if (filename.endsWith(".png"))
    {
      request->_tempFile = SPIFFS.open("/i/" + filename, "w");
    } else if (filename.endsWith(".css"))
    {
      request->_tempFile = SPIFFS.open("/css/" + filename, "w");
    } else if (filename.endsWith(".js"))
    {
      request->_tempFile = SPIFFS.open("/js/" + filename, "w");
    } else
    {
      request->_tempFile = SPIFFS.open("/" + filename, "w");
    }
  }
  if (len)
  {
    request->_tempFile.write(data, len);
  }
  if (final)
  {
    request->_tempFile.close();
    request->redirect("/");
  }
}

void updateProgress(size_t prg, size_t sz)
{
  DEBUGf("Progress: %d%%\n", (prg * 100) / content_len);
  lcd.setCursor(lcd.width() - 63, 0);
  lcd.printf("Upl:%d%%", (prg * 100) / content_len);
}

void configWeb()
{
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/i", SPIFFS, "/i");
  server.serveStatic("/config.json", SPIFFS, "/config.json");
  server.on("/", HTTP_GET, onIndexRequest);
  server.on("/doconfig", HTTP_POST, onConfigRequest);
  server.on("/doupdate", HTTP_POST, [](AsyncWebServerRequest * request) {},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data, size_t len, bool final) {
    handleDoUpdate(request, filename, index, data, len, final);
  });
  server.on("/dofile", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);
  },
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data, size_t len, bool final) {
    handleDoFile(request, filename, index, data, len, final);
  });
  server.on("/restart", HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "10");
    response->addHeader("Location", "/");
    request->send(response);
    delay(500);
    ESP.restart();
  });
  server.begin();
  Update.onProgress(updateProgress);
  DEBUGln(F("HTTP server started"));
  lcd.println("web server ready");
}
