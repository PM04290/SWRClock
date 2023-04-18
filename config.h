/*
  Clock segment config
     AAAA
    D    E
    D    E
     BBBB
    F    G
    F    G
     CCCC

*/
#pragma once

#define VERSION "0.2"

// ------------------------
// Select your display here
#define TFT_SC01
//#define TFT_SC01Plus
//#define TFT_480x320
// ------------------------

#define EEPROM_MAX_SIZE    256
#define EEPROM_TEXT_SIZE   48
#define EEPROM_TEXT_OFFSET 16

#define EEPROM_ALARM_HR 0
#define EEPROM_ALARM_MN 1
#define EEPROM_ALARM_ON 2
#define EEPROM_STATION  3
#define EEPROM_VOLUME   4
#define EEPROM_LUM_LVL0 5
#define EEPROM_LUM_LVL1 6
#define EEPROM_LUM_SENS 7
#define EEPROM_LUM_OUT  8
#define EEPROM_CLK_R    9
#define EEPROM_CLK_G    10
#define EEPROM_CLK_B    11

#define JSON_MAX_SIZE 1000

#define COLOR_BLACK lcd.color565(0, 0, 0)
#define COLOR_RED   lcd.color565(255, 0, 0)

#define SEGMENT_ON  lcd.color565(clkR, clkG, clkB)
#define SEGMENT_OFF lcd.color565(10, 10, 10)
#define SEGMENT_SEL lcd.color565(255, 64, 64)
#define SEGMENT_ALM lcd.color565(64, 255, 64)

#define COLOR_ITEM_BACK lcd.color565(18, 18, 18)
#define COLOR_ITEM_TEXT lcd.color565(253, 126, 20)
#define COLOR_ITEM_WHITE lcd.color565(253, 253, 253)

#define MAX_ICON_SIZE 5000

char Wifi_ssid[EEPROM_TEXT_SIZE] = "";  // STA WiFi SSID
char Wifi_pass[EEPROM_TEXT_SIZE] = "";  // STA WiFi password

char AP_ssid[10] = "swrclock0";// AP WiFi SSID
char AP_pass[9] = "12345678";  // AP WiFi password

static auto transpalette = 0;

// https://javl.github.io/image2cpp/

#ifdef TFT_SC01
#define LIGHT_SENSOR  35
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_FT5x06  _touch_instance;
  public:
    LGFX(void)
    {
      {
        auto cfg = _bus_instance.config();
        cfg.spi_host = HSPI_HOST;
        cfg.pin_mosi = 13;
        cfg.pin_miso = -1;
        cfg.pin_sclk = 14;
        cfg.pin_dc   = 21;
        cfg.spi_3wire  = true;
        cfg.freq_write = 40000000;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      {
        auto cfg = _panel_instance.config();
        cfg.pin_cs   = 15;
        cfg.pin_rst  = 22;
        cfg.readable = false;
        _panel_instance.config(cfg);
      }
      {
        auto cfg = _light_instance.config();
        cfg.pin_bl = 23;
        cfg.invert = false;
        cfg.freq   = 44100;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }
      {
        auto cfg = _touch_instance.config();
        cfg.pin_int    = 39;
        cfg.pin_sda    = 18;
        cfg.pin_scl    = 19;
        cfg.i2c_addr   = 0x38;
        cfg.i2c_port   = 1;
        cfg.freq       = 400000;
        cfg.x_min      = 0;
        cfg.x_max      = 319;
        cfg.y_min      = 0;
        cfg.y_max      = 479;
        cfg.bus_shared = false;
        cfg.offset_rotation = 0;
        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
      }
      setPanel(&_panel_instance);
    }
};
#endif

#ifdef TFT_SC01Plus
#define LIGHT_SENSOR  35
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;  // ST7796UI
    lgfx::Bus_Parallel8 _bus_instance;    // MCU8080 8B
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_FT5x06  _touch_instance;
  public:
    LGFX(void)
    {
      {
        auto cfg = _bus_instance.config();
        cfg.freq_write = 20000000;
        cfg.pin_wr = 47;
        cfg.pin_rd = -1;
        cfg.pin_rs = 0;
        // LCD data interface, 8bit MCU (8080)
        cfg.pin_d0 = 9;
        cfg.pin_d1 = 46;
        cfg.pin_d2 = 3;
        cfg.pin_d3 = 8;
        cfg.pin_d4 = 18;
        cfg.pin_d5 = 17;
        cfg.pin_d6 = 16;
        cfg.pin_d7 = 15;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      {
        auto cfg = _panel_instance.config();

        cfg.pin_cs           =    -1;
        cfg.pin_rst          =    4;
        cfg.pin_busy         =    -1;
        cfg.panel_width      =   320;
        cfg.panel_height     =   480;
        cfg.offset_x         =     0;
        cfg.offset_y         =     0;
        cfg.offset_rotation  =     0;
        cfg.dummy_read_pixel =     8;
        cfg.dummy_read_bits  =     1;
        cfg.readable         =  false;
        cfg.invert           = true;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       =  true;
        _panel_instance.config(cfg);
      }
      {
        auto cfg = _light_instance.config();
        cfg.pin_bl = 45;
        cfg.invert = false;
        cfg.freq   = 44100;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }
      {
        auto cfg = _touch_instance.config();
        cfg.x_min      = 0;
        cfg.x_max      = 319;
        cfg.y_min      = 0;
        cfg.y_max      = 479;
        cfg.pin_int    = 7;
        cfg.bus_shared = true;
        cfg.offset_rotation = 0;
        cfg.i2c_port = 1;
        cfg.i2c_addr = 0x38;
        cfg.pin_sda  = 6;
        cfg.pin_scl  = 5;
        cfg.freq = 400000;
        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
      }
      setPanel(&_panel_instance);
    }
};
#endif

#ifdef TFT_480x320
#define LIGHT_SENSOR  35
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;
  public:
    LGFX(void)
    {
      {
        auto cfg = _bus_instance.config();
        cfg.spi_host = VSPI_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 40000000;
        cfg.freq_read  = 16000000;
        cfg.spi_3wire = true;
        cfg.use_lock   = true;
        cfg.pin_sclk = 18;
        cfg.pin_mosi = 23;
        cfg.pin_miso = 19;
        cfg.pin_dc   = 2;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      {
        auto cfg = _panel_instance.config();
        cfg.pin_cs           =    5;
        cfg.pin_rst          =    4;
        cfg.pin_busy         =   -1;
        cfg.panel_width      =   320;
        cfg.panel_height     =   480;
        cfg.offset_x         =     0;
        cfg.offset_y         =     0;
        cfg.offset_rotation  =     0;
        cfg.dummy_read_pixel =     8;
        cfg.dummy_read_bits  =     1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        _panel_instance.config(cfg);
      }
      {
        auto cfg = _light_instance.config();
        cfg.pin_bl = 16;
        cfg.invert = false;
        cfg.freq   = 44100;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }
      {
        auto cfg = _touch_instance.config();
        // run "calibrateTouch" in controller.init to now values
        cfg.x_min      = 400;
        cfg.x_max      = 3680;
        cfg.y_min      = 3700;
        cfg.y_max      = 280;
        cfg.offset_rotation = 0;
        cfg.pin_int    = 27;
        cfg.spi_host = HSPI_HOST;
        cfg.freq = 1000000;
        cfg.pin_sclk = 14;
        cfg.pin_mosi = 13;
        cfg.pin_miso = 12;
        cfg.pin_cs   = 15;
        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
      }
      setPanel(&_panel_instance);
    }
};
#endif
