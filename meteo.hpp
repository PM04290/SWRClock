/*
  https://open-meteo.com/en/docs
  Weather code
  0   Clear sky
  1,2,3     Mainly clear, partly cloudy, and overcast
  45,48     Fog and depositing rime fog
  51,53,55  Drizzle: Light, moderate, and dense intensity
  56,57     Freezing Drizzle: Light and dense intensity
  61,63,65  Rain: Slight, moderate and heavy intensity
  66,67     Freezing Rain: Light and heavy intensity
  71,73,75  Snow fall: Slight, moderate, and heavy intensity
  77        Snow grains
  80,81,82  Rain showers: Slight, moderate, and violent
  85,86     Snow showers slight and heavy
  95        *Thunderstorm: Slight or moderate
  96,99     *Thunderstorm with slight and heavy hail

*/
#include <HTTPClient.h>
#include "src/jsmn.h"

const char* meteoTemplate = "http://api.open-meteo.com/v1/forecast?latitude=%0.3f&longitude=%0.3f&hourly=precipitation_probability,weathercode&models=best_match&current_weather=true&forecast_days=1&timezone=auto";

float latitude = 39.455;  // somewhere in atlantic...
float longitude = -31.133;
float temperature = 99.9;
float precipitation = 0.0;
int weatherCode = 0;

char weatherCodeBmp[50] = "/bmp/weather-0.bmp";

WiFiClient client;
HTTPClient http;

String httpGETRequest()
{
  char url[300];
  sprintf(url, meteoTemplate, weatherConfig["lat"].as<float>(), weatherConfig["lon"].as<float>());
  DEBUGln(url);
  HTTPClient http;
  http.begin(client, url);
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode == 200)
  {
    payload = http.getString();
  }
  else
  {
    DEBUGf("Meteo HTTP GET error code: %d\n", httpResponseCode);
  }
  http.end();
  return payload;
}

static void jsontok_print(const char *json, int i, jsmntok_t t)
{
  const char* TYPES[] = {"UDF", "OBJ", "ARR", "?", "STR", "5", "6", "7", "PRM"};
  if (t.type == JSMN_STRING || t.type == JSMN_PRIMITIVE)
    DEBUGf("[%d] %s %d start:%d end:%d size:%d p:%d (%.*s)\n", i, TYPES[t.type], t.index, t.start, t.end, t.size, t.parent, t.end - t.start, json + t.start);
  else
    DEBUGf("[%d] %s %d start:%d end:%d size:%d p:%d ***\n", i, TYPES[t.type], t.index, t.start, t.end, t.size, t.parent);
}

static jsmntype_t jsonkeypath(char* path, bool start, const char *json, jsmntok_t *tok, int idx)
{
  //jsontok_print(json, idx, tok[idx]);
  //
  int len = tok[idx].end - tok[idx].start;
  jsmntype_t parent_type = JSMN_UNDEFINED;
  if (tok[idx].parent > 0)
  {
    parent_type = jsonkeypath(path, false, json, tok, tok[idx].parent);
    if (tok[idx].type == JSMN_STRING || tok[idx].type == JSMN_PRIMITIVE)
    {
      if (parent_type != JSMN_ARRAY)
      {
        strcat(path, "/");
        strncat(path, json + tok[idx].start, len);
      } else {
        strcat(path, "[");
        strcat(path, String(tok[idx].index).c_str());
        strcat(path, "]");
      }
    }
  } else
  {
    strncpy(path, json + tok[idx].start, len);
    path[len] = 0;
  }
  return tok[idx].type;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING
      && (int)strlen(s) == tok->end - tok->start
      && strncmp(json + tok->start, s, tok->end - tok->start) == 0)
  {
    return 1;
  }
  return 0;
}

bool getMeteo()
{
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[200]; // We expect no more than 200 tokens
  char path_cache[100]; // mandatory to build keypath

  String jsonStr = httpGETRequest();

  jsmn_init(&p);
  r = jsmn_parse(&p, jsonStr.c_str(), jsonStr.length(), t, sizeof(t) / sizeof(t[0]));
  if (r < 0)
  {
    DEBUGf("Failed to parse JSON: %d\n", r);
    return false;
  }

  // Assume the top-level element is an object
  if (r < 1 || t[0].type != JSMN_OBJECT)
  {
    DEBUGln(F("Object expected"));
    return false;
  }

  char path[100];
  path[0] = 0;
  char s1[40], s2[40];
  bool found = false;
  sprintf(s1, "hourly/weathercode[%d]", hours);
  sprintf(s2, "hourly/precipitation_probability[%d]", hours);
  // Loop over all keys of the root object
  for (i = 1; i < r; i++)
  {
    jsonkeypath(path_cache, true, jsonStr.c_str(), t, i);
    if (strcmp(path_cache, "current_weather/temperature") == 0)
    {
      const char* data = jsonStr.c_str() + t[i + 1].start;
      temperature = atof(data);
      DEBUGf("temperature %f\n", temperature);
      found = true;
    }
    if (strcmp(path_cache, s1) == 0)
    {
      const char* data = jsonStr.c_str() + t[i].start;
      weatherCode = atoi(data);
      DEBUGf("Wcode %d\n", weatherCode);
      found = true;
    }
    if (strcmp(path_cache, s2) == 0)
    {
      const char* data = jsonStr.c_str() + t[i].start;
      precipitation = atoi(data);
      DEBUGf("precipitation %f\n", precipitation);
      found = true;
    }
    if (t[i].parent == 0 || t[t[i].parent].type == JSMN_STRING || t[t[i].parent].type == JSMN_OBJECT)
      i++;
  }
  return found;
}

const char* getBmpByCode(byte wCode)
{
  switch (wCode)
  {
    case 0:
      strcpy(weatherCodeBmp, "/bmp/weather-0.bmp");
      break;
    case 1:
    case 2:
    case 3:
      strcpy(weatherCodeBmp, "/bmp/weather-1-2-3.bmp");
      break;
    case 45:
    case 48:
      strcpy(weatherCodeBmp, "/bmp/weather-45-48.bmp");
      break;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
      strcpy(weatherCodeBmp, "/bmp/weather-51-53-55-56-57.bmp");
      break;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
      strcpy(weatherCodeBmp, "/bmp/weather-61-63-65-66-67.bmp");
      break;
    case 71:
    case 73:
    case 75:
    case 77:
      strcpy(weatherCodeBmp, "/bmp/weather-71-73-75-77.bmp");
      break;
    case 80:
    case 81:
    case 82:
      strcpy(weatherCodeBmp, "/bmp/weather-80-81-82.bmp");
      break;
    case 85:
    case 86:
      strcpy(weatherCodeBmp, "/bmp/weather-85-86.bmp");
      break;
    case 95:
      strcpy(weatherCodeBmp, "/bmp/weather-95.bmp");
      break;
    case 96:
    case 99:
      strcpy(weatherCodeBmp, "/bmp/weather-96-99.bmp");
      break;
    default:
      strcpy(weatherCodeBmp, "/bmp/weather-80-81-82.bmp");
      break;
  }
  DEBUGf("%d %s\n", weatherCode, weatherCodeBmp);
  return (const char*)weatherCodeBmp;
}
