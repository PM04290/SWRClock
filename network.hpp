#include <ESPmDNS.h>
#ifdef USE_ETHERNET
#include <ETH.h>
#endif
#include "time.h"

bool wifiok = false;

const char* defaultNTP = "europe.pool.ntp.org";
const char* defaultTZ = "CET-1CEST,M3.5.0,M10.5.0/3";

struct tm timeinfo;

void configWifi()
{
  char txt[40];

  DEBUGln(WiFi.macAddress());
  lcd.print("MAC: ");
  lcd.println(WiFi.macAddress());

  // Mode normal
  DEBUG(F("Wifi search"));
  lcd.print("wifi ");
  lcd.print(Wifi_ssid);
  WiFi.begin(Wifi_ssid, Wifi_pass);
  int tentativeWiFi = 0;
  // Attente de la connexion au réseau WiFi / Wait for connection
  while ( WiFi.status() != WL_CONNECTED && tentativeWiFi < 20)
  {
    lcd.print(".");
    delay( 500 ); DEBUG( "." );
    tentativeWiFi++;
  }
  wifiok = WiFi.status() == WL_CONNECTED;
  if (wifiok)
  {
    // Connexion WiFi établie / WiFi connexion is OK
    DEBUG(F("\nConnected to ")); DEBUGln( Wifi_ssid );
    DEBUG(F("IP address: ")); DEBUGln( WiFi.localIP() );

    lcd.println("");
    lcd.println("Wifi OK");
    lcd.println(WiFi.localIP().toString().c_str());
  } else {
    DEBUGln(F("\nNo wifi STA, set AP mode"));
    // Mode AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_pass);
    // Default IP Address is 192.168.4.1
    // if you want to change uncomment below
    // softAPConfig (local_ip, gateway, subnet)
    DEBUGf("AP WIFI :%s\n", AP_ssid );
    DEBUGf("AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());

    lcd.print("Wifi AP ");
    lcd.println(AP_ssid);
    lcd.print(WiFi.softAPIP().toString().c_str());
  }
  if (MDNS.begin(AP_ssid))
  {
    MDNS.addService("http", "tcp", 80);
    lcd.printf("web : %s.local\n", AP_ssid);
  }
}

bool getNTP(int &h, int &m, int &s)
{
  if (!wifiok) return false;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    DEBUGln("Failed to obtain time");
    return false;
  }
  h = timeinfo.tm_hour;
  m = timeinfo.tm_min;
  s = timeinfo.tm_sec;
  return true;
}
