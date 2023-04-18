//  https://lang-ship.com/blog/files/LovyanGFX/LovyanGFX_BASIC.pdf

int PADDING_Y = 10;

int SEGMENT_WIDTHNESS = 16;

int DIGIT_HEIGHT = 200;
int DIGIT_WIDTH  = 100;

#define SEGMENT_HEIGHT (DIGIT_HEIGHT / 2)

#define GAP (DIGIT_WIDTH/10)

#define GX_ABC (SEGMENT_WIDTHNESS/8)
#define GY_DE (SEGMENT_WIDTHNESS/5)
#define GY_FG (SEGMENT_WIDTHNESS/4)

int MENU_TOP = 220;
int MENU_HEIGHT = 100;
int MENU_WIDTH = 460;

int ITEM_GAP = 5;
int ITEM_HEIGHT = 60;
int ITEM_WIDTH = 110;

#define TIMEOUT_EDITING 15000
#define TIMEOUT_TAPCOUNT 2000

#define TEMPO_AUTO_OFF_POPUP   5 // in seconds
#define TEMPO_AUTO_OFF_SUBMENU 7 // in seconds

#define MAX_ITEMS 5
byte itemsSizing[MAX_ITEMS] = {1, 1, 2, 1, 1};

#define INDEX_CLOCK 0
#define INDEX_BTN1  1
#define INDEX_BTN2  2
#define INDEX_BTN3  3
#define INDEX_BTN4  4
#define INDEX_BTN5  5

#define LONG_PRESS_DURATION 800 // ms

static const lgfx::U8g2font font_XXS( myVerdana10_tf );
static const lgfx::U8g2font font_XS( myVerdana14_tf );
static const lgfx::U8g2font font_S( myVerdana18_tf );
static const lgfx::U8g2font font_M( myVerdana22_tf );
static const lgfx::U8g2font font_L( myVerdana28_tf );
static const lgfx::U8g2font font_XL( myRoboto36r_tf );
static const lgfx::U8g2font font_XXL( myRoboto48r_tf );

typedef enum {
  Start = 0,
  Stop = 1,
  Timeout = 2
} editingevent_t;

typedef enum {
  unknown = 0,
  clockAlarm = 1,
  saveAlarm = 2
} stateevent_t;

typedef enum {
  none = 0,
  text = 1,
  icon = 2,
  icontext = 3
} drawmode_t;

typedef void (*callbackTouch) (uint32_t index, bool longPress, uint16_t arg2);
typedef void (*callbackEditing) (editingevent_t event);
typedef void (*callbackState) (stateevent_t state);


size_t readFile(const char * path)
{
  DEBUGf("Reading file: %s\n", path);
  size_t result = 0;
  File file = SPIFFS.open(path);
  if (!file || file.isDirectory())
  {
    DEBUGf("failed to open icon : %s\n", path);
  }
  size_t numBytesToRead = file.size();
  //DEBUGf("size:%d\n", numBytesToRead);
  if (numBytesToRead >= MAX_ICON_SIZE)
  {
    DEBUGf("icon too big : %s\n", path);
  }
  if (file.readBytes((char*)tmpIcon, numBytesToRead))
  {
    result = numBytesToRead;
  }
  file.close();
  return result;
}

String getStringPartByNr(String data, char separator, int index)
{
  int stringData = 0;        //variable to count data part nr
  String dataPart = "";      //variable to hole the return text

  for (int i = 0; i < data.length(); i++)
  {
    if (data[i] == separator)
    {
      //Count the number of times separator character appears in the text
      stringData++;
    } else if (stringData == index)
    {
      //get the text when separator is the rignt one
      dataPart.concat(data[i]);
    } else if (stringData > index)
    {
      //return text and stop if the next separator appears - to save CPU-time
      return dataPart;
    }
  }
  return dataPart;
}

class Xitem
{
  public:
    Xitem()
    {
      _x = _y = _w = _h = 0;
      _visible = false;
      _down = false;
      _onTouch = NULL;
      _onEditing = NULL;
      _onState = NULL;
      _blinked = false;
      _blinkAllowed = false;
      _longPressAutoAction = false;
      _timeoutEditing = 0;
      _tapCount = 0;
      _timeoutTapCount = 0;
      _drawMode = drawmode_t::none;
      _icon = NULL;
      _text = NULL;
    }
    virtual void draw()
    {
      // none, must be overwrited
    }
    virtual void blink()
    {
      _blinked = !_blinked;
      if (_blinkAllowed)
      {
        draw();
      }
    }
    void allowBlink(bool allowed)
    {
      _blinkAllowed = allowed;
    }
    Xitem* isTouched(int32_t x, int32_t y)
    {
      int32_t xmax = _x + _w;
      int32_t ymax = _y + _h;
      if (_visible && (x >= _x) && (x < xmax) && (y >= _y) && (y < ymax))
      {
        return this;
      }
      return NULL;
    }
    void setDown(bool down)
    {
      if (!_down && down)
      {
        _tapCount++;
        _timeoutTapCount = (millis() + TIMEOUT_TAPCOUNT) | 1; // | 1 to prevent sum = 0
      }
      _down = down;
      draw();
    }
    bool isEditing()
    {
      return _timeoutEditing > 0;
    }
    int getTapCount()
    {
      return _tapCount;
    }
    bool checkEditingTimeout()
    {
      if (_timeoutEditing)
      {
        if (millis() > _timeoutEditing)
        {
          _timeoutEditing = 0;
          if (_onEditing)
          {
            _onEditing(editingevent_t::Timeout);
          }
          return true;
        }
      }
      return false;
    }
    bool checkTapTimeout()
    {
      if (_timeoutTapCount)
      {
        if (millis() > _timeoutTapCount)
        {
          _timeoutTapCount = 0;
          _tapCount = 0;
          return true;
        }
      }
      return false;
    }
    virtual void startEditing()
    {
      extendEditing();
      if (_onEditing)
      {
        _onEditing(editingevent_t::Start);
      }
    }
    void extendEditing()
    {
      _timeoutEditing = (millis() + TIMEOUT_EDITING) | 1; // | 1 to prevent sum = 0
    }
    void stopEditing()
    {
      _timeoutEditing = 0;
      if (_onEditing) {
        _onEditing(editingevent_t::Stop);
      }
    }
    void setCallbackTouch(callbackTouch theFunc)
    {
      _onTouch = theFunc;
    }
    void setCallbackEditing(callbackEditing theFunc)
    {
      _onEditing = theFunc;
    }
    void setCallbackState(callbackState theFunc)
    {
      _onState = theFunc;
    }
    virtual void doTouch(bool longPress)
    {
      if (_onTouch)
      {
        _onTouch(0, longPress, _tapCount);
      }
    }
    void setIcon(const char* icon)
    {
      if (_icon)
      {
        free(_icon);
        _icon = NULL;
      }
      if (icon)
      {
        _icon = (char*)malloc(strlen(icon) + 1);
        strcpy(_icon, icon);
      }
      _drawMode = drawmode_t::none;
      if (_icon) _drawMode = (drawmode_t)((byte)_drawMode | (byte)drawmode_t::icon);
      if (_text) _drawMode = (drawmode_t)((byte)_drawMode | (byte)drawmode_t::text);
      draw();
    }
    void setText(const char* text)
    {
      if (_text)
      {
        free(_text);
        _text = NULL;
      }
      if (text)
      {
        _text = (char*)malloc(strlen(text) + 1);
        strcpy(_text, text);
      }
      _drawMode = drawmode_t::none;
      if (_icon) _drawMode = (drawmode_t)((byte)_drawMode | (byte)drawmode_t::icon);
      if (_text) _drawMode = (drawmode_t)((byte)_drawMode | (byte)drawmode_t::text);
      draw();
    }
    void setLongPressAuto(bool longPressAutoAction)
    {
      _longPressAutoAction = longPressAutoAction;
    }
    bool getLongPressAuto()
    {
      return _longPressAutoAction;
    }
    virtual void setVisible(bool visible)
    {
      _visible = visible;
    }
    bool isVisible()
    {
      return _visible;
    }
    void getCoord(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
    {
      *x = _x;
      *w = _w;
      *y = _y;
      *h = _h;
    }
  protected:
    int _x;
    int _y;
    int _w;
    int _h;
    bool _visible;
    bool _down;
    bool _blinked;
    bool _blinkAllowed;
    bool _longPressAutoAction;
    uint32_t _timeoutEditing;
    int _tapCount;
    uint32_t _timeoutTapCount;
    char* _icon;
    char* _text;
    drawmode_t _drawMode;
    callbackTouch _onTouch;
    callbackEditing _onEditing;
    callbackState _onState;
};

LGFX_Sprite segmentHon(&lcd);
LGFX_Sprite segmentHoff(&lcd);
LGFX_Sprite segmentV1on(&lcd);
LGFX_Sprite segmentV1off(&lcd);
LGFX_Sprite segmentV2on(&lcd);
LGFX_Sprite segmentV2off(&lcd);
class XitemClock : public Xitem
{
  public:
    XitemClock(bool clockShowSecond) : Xitem()
    {
      _x = 0;
      _y = 0;
      _w = lcd.width();
      _h = MENU_TOP - PADDING_Y;
      _visible = true;
      _clockShowSecond = clockShowSecond;
      _hr = _mn = _sc = 0;
      _alarmDays = 0x7F; // allDays
      _alarmHr = 8;
      _alarmMn = 0;
      _alarmActive = false;
      _alarmTriggered = false;
      _rainy1 = 0;
      _rainy2 = 0;
      _rainy3 = 0;
      _refreshAll = true;
    }
    void draw() override
    {
      if (_visible)
      {
        if (_refreshAll)
        {
          lcd.fillRect(_x, _y, _w, _h, COLOR_BLACK);
          _refreshAll = false;
        }
        int h = _timeoutEditing ? _editHr : _hr;
        int m = _timeoutEditing ? _editMn : _mn;
        int s = _timeoutEditing ? 0xff : _sc;
        int middle = lcd.width() / 2;
        auto dotColor = _timeoutEditing ? (_blinked ? SEGMENT_SEL : SEGMENT_OFF) : (_alarmActive ? SEGMENT_ALM : SEGMENT_ON);
        if (_clockShowSecond)
        {
          middle = lcd.width() / 3;
          lcd.fillRoundRect((middle * 2) - (SEGMENT_WIDTHNESS / 2), PADDING_Y + (SEGMENT_HEIGHT / 2) - (SEGMENT_WIDTHNESS / 2), SEGMENT_WIDTHNESS, SEGMENT_WIDTHNESS, 4, dotColor);
          lcd.fillRoundRect((middle * 2) - (SEGMENT_WIDTHNESS / 2), PADDING_Y + (3 * SEGMENT_HEIGHT / 2) - (SEGMENT_WIDTHNESS / 2), SEGMENT_WIDTHNESS, SEGMENT_WIDTHNESS, 4, dotColor);
        }
        lcd.fillRoundRect(middle - (SEGMENT_WIDTHNESS / 2), PADDING_Y + (SEGMENT_HEIGHT / 2) - (SEGMENT_WIDTHNESS / 2), SEGMENT_WIDTHNESS, SEGMENT_WIDTHNESS, 4, dotColor);
        lcd.fillRoundRect(middle - (SEGMENT_WIDTHNESS / 2), PADDING_Y + (3 * SEGMENT_HEIGHT / 2) - (SEGMENT_WIDTHNESS / 2), SEGMENT_WIDTHNESS, SEGMENT_WIDTHNESS, 4, dotColor);

        int W = lcd.width();
        if (_hr < 10)
          drawDigit(GAP, PADDING_Y, 0xff);
        else
          drawDigit(GAP, PADDING_Y, (int)(h / 10));
        if (_clockShowSecond)
        {
          int T = W / 3;
          drawDigit(T - DIGIT_WIDTH - (5 * GAP / 2), PADDING_Y, h % 10);

          drawDigit(T + (5 * GAP / 2) , PADDING_Y, (int)(m / 10));
          drawDigit((T * 2) - DIGIT_WIDTH - (5 * GAP / 2), PADDING_Y, m % 10);

          drawDigit((T * 2) + (5 * GAP / 2), PADDING_Y, (int)(s / 10));
          drawDigit(W - DIGIT_WIDTH - GAP, PADDING_Y, s % 10);
        } else
        {
          drawDigit(DIGIT_WIDTH + (GAP * 3), PADDING_Y, h % 10);

          drawDigit(W - (DIGIT_WIDTH * 2) - (GAP * 3), PADDING_Y, (int)(m / 10));
          drawDigit(W - DIGIT_WIDTH - GAP, PADDING_Y, m % 10);
        }
      }
    }
    void redrawClipRect(Xitem* item)
    {
      int x, y, w, h;
      item->getCoord(&x, &y, &w, &h);
      lcd.fillRect(x, y, w, h, COLOR_BLACK);
      lcd.setClipRect(x, y, w, h);
      _visible = true;
      draw();
      _visible = false;
      lcd.clearClipRect();
    }
    void setAlarm(byte h, byte m, bool active)
    {
      _alarmHr = h;
      _alarmMn = m;
      _alarmActive = active;
    }
    void getAlarm(byte* h, byte* m, bool* active)
    {
      *h = _alarmHr;
      *m = _alarmMn;
      *active = _alarmActive;
    }
    void setRainy(byte r1, byte r2, byte r3)
    {
      _rainy1 = r1;
      _rainy2 = r2;
      _rainy3 = r3;
    }
    void setTime(byte h, byte m, int s, int precipitation = 0)
    {
      bool refresh = false;
      if (h != _hr || m != _mn)
      {
        _hr = h;
        _mn = m;
        refresh = true;
      }
      if (_clockShowSecond && (s != _sc))
      {
        _sc = s;
        refresh = true;
      }
      if (_alarmActive)
      {
        byte deltaMn = 0;
        if (precipitation > 30)
        {
          deltaMn = _rainy1;
        }
        if (precipitation > 50)
        {
          deltaMn = _rainy2;
        }
        if (precipitation > 80)
        {
          deltaMn = _rainy3;
        }
        int totMn = (_alarmHr * 60) + _alarmMn;
        totMn = totMn - deltaMn;
        if (totMn < 0)
        {
          totMn += (60*24);
        }
        int targetHr = totMn / 60;
        int targetMn = totMn % 60;
        if (h == targetHr && m == targetMn && s == 0)
        {
          _alarmTriggered = true;
          if (_onState)
          {
            _onState(stateevent_t::clockAlarm);
          }
        }
      }
      if (_visible && refresh)
      {
        draw();
      }
    }
    void startEditing()
    {
      _editHr = _alarmHr;
      _editMn = _alarmMn;
      Xitem::startEditing();
    }
    void moveAlarm(int deltaH, int deltaM)
    {
      _editHr += deltaH;
      if (_editHr < 0) _editHr = 23;
      if (_editHr > 23) _editHr = 0;
      _editMn += deltaM;
      if (_editMn < 0) _editMn = 59;
      if (_editMn > 59) _editMn = 0;
      if (_timeoutEditing)
      {
        draw();
      }
    };
    void saveAlarmTime()
    {
      //
      _alarmHr = _editHr;
      _alarmMn = _editMn;
      if (_onState)
      {
        _onState(stateevent_t::saveAlarm);
      }
    };
    bool getAlarmActive()
    {
      return _alarmActive;
    };
    void setAlarmActive(bool alarmActive)
    {
      _alarmActive = alarmActive;
    }
    bool getAlarmTriggered()
    {
      return _alarmTriggered;
    }
    void changeColor(int color)
    {
      segmentHon.fillRoundRect(0, 0, segmentHon.width(), segmentHon.height(), SEGMENT_WIDTHNESS / 2, color);
      segmentV1on.fillRoundRect(0, 0, segmentV1on.width(), segmentV1on.height(), SEGMENT_WIDTHNESS / 2, color);
      segmentV2on.fillRoundRect(0, 0, segmentV2on.width(), segmentV2on.height(), SEGMENT_WIDTHNESS / 2, color);
      draw();
    }
  private:
    byte _hr;
    byte _mn;
    byte _sc;
    byte _editHr;
    byte _editMn;
    byte _alarmHr;
    byte _alarmMn;
    byte _rainy1;
    byte _rainy2;
    byte _rainy3;
    byte _alarmDays;
    bool _alarmActive;
    bool _alarmTriggered;
    bool _refreshAll;
    void drawDigit(int x, int y, byte number)
    {
      int xABC, yA, yB, yC;
      int xD, xE, yDE;
      int xF, xG, yFG;
      xABC = x + SEGMENT_WIDTHNESS - GX_ABC;
      yA = y;
      yB = y + (DIGIT_HEIGHT / 2) - (SEGMENT_WIDTHNESS / 2);
      yC = y + DIGIT_HEIGHT - SEGMENT_WIDTHNESS;
      xD = x;
      xE = x + DIGIT_WIDTH - SEGMENT_WIDTHNESS;
      yDE = y + SEGMENT_WIDTHNESS - GY_DE;
      xF = x;
      xG = x + DIGIT_WIDTH - SEGMENT_WIDTHNESS;
      yFG = y + (DIGIT_HEIGHT / 2) + (SEGMENT_WIDTHNESS / 2) - GY_FG;
      switch (number)
      {
        case 0:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHoff.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2on.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 1:
          segmentHoff.pushSprite(&lcd, xABC, yA);
          segmentHoff.pushSprite(&lcd, xABC, yB);
          segmentHoff.pushSprite(&lcd, xABC, yC);
          segmentV1off.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 2:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1off.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2on.pushSprite(&lcd, xF, yFG);
          segmentV2off.pushSprite(&lcd, xG, yFG);
          break;
        case 3:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1off.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 4:
          segmentHoff.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHoff.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 5:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1off.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 6:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1off.pushSprite(&lcd, xE, yDE);
          segmentV2on.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 7:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHoff.pushSprite(&lcd, xABC, yB);
          segmentHoff.pushSprite(&lcd, xABC, yC);
          segmentV1off.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 8:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2on.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        case 9:
          segmentHon.pushSprite(&lcd, xABC, yA);
          segmentHon.pushSprite(&lcd, xABC, yB);
          segmentHon.pushSprite(&lcd, xABC, yC);
          segmentV1on.pushSprite(&lcd, xD, yDE);
          segmentV1on.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2on.pushSprite(&lcd, xG, yFG);
          break;
        default:
          segmentHoff.pushSprite(&lcd, xABC, yA);
          segmentHoff.pushSprite(&lcd, xABC, yB);
          segmentHoff.pushSprite(&lcd, xABC, yC);
          segmentV1off.pushSprite(&lcd, xD, yDE);
          segmentV1off.pushSprite(&lcd, xE, yDE);
          segmentV2off.pushSprite(&lcd, xF, yFG);
          segmentV2off.pushSprite(&lcd, xG, yFG);
          break;
      }
    }
  protected:
    bool _clockShowSecond;
};

LGFX_Sprite item(&lcd);
class XitemSubmenu : public Xitem
{
  public:
    XitemSubmenu(uint8_t idx, int x, callbackTouch theFunc, const char* icon) : Xitem()
    {
      _idx = idx;
      _x = x;
      _y = MENU_TOP - ITEM_GAP - (68 * (idx + 1));
      _w = ITEM_WIDTH;
      _h = 68;
      setCallbackTouch(theFunc);
      setIcon(icon);
    }
    void draw() override
    {
      if (_visible)
      {
        item.createSprite(_w, _h);
        item.setSwapBytes(true);
        item.fillRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_BACK);
        //
        if (_down)
        {
          item.drawRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_TEXT);
          item.drawRoundRect(1, 1, _w - 2, _h - 2, 7, COLOR_ITEM_TEXT);
        }
        if (_icon)
        {
          LGFX_Sprite tmpSpr;
          tmpSpr.createFromBmpFile(SPIFFS, _icon);
          tmpSpr.pushSprite(&item, (_w / 2) - 24, (_h / 2) - 24, COLOR_BLACK);
        }
        //
        item.pushSprite(&lcd, _x, _y);
        item.deleteSprite();
      } else {
        lcd.fillRect(_x, _y, _w, _h, COLOR_BLACK);
      }
    }
    void doTouch(bool longPress) override
    {
      if (_onTouch)
      {
        _onTouch(_idx, longPress, _tapCount);
      }
    }
  protected:
    uint8_t _idx;
};
class XitemMenu : public Xitem
{
  public:
    XitemMenu(uint8_t idx, int xMargin, byte groupSize) : Xitem()
    {
      _idx = idx;
      _x = ITEM_GAP + xMargin; //((ITEM_WIDTH + ITEM_GAP) * _idx);
      _y = MENU_TOP + ITEM_GAP;
      _w = ITEM_WIDTH * groupSize;
      _h = ITEM_HEIGHT;
      _visible = true;
      // submenu
      _submenuList = NULL;
      _submenuCount = 0;
      _submenuOpened = false;
    }
    XitemSubmenu* addSubmenu(callbackTouch theFunc, const char* icon)
    {
      _submenuList = (XitemSubmenu**)realloc(_submenuList, (_submenuCount + 1) * sizeof(XitemSubmenu*));
      if (_submenuList == nullptr)
        return nullptr;

      _submenuList[_submenuCount] = new XitemSubmenu(_submenuCount, _x, theFunc, icon);
      _submenuCount++;
      return _submenuList[_submenuCount - 1];
    }
    XitemSubmenu* getSubmenu(uint8_t idx)
    {
      if (idx < _submenuCount)
      {
        return _submenuList[idx];
      }
      return NULL;
    }
    void showSubmenu()
    {
      if (!_submenuOpened)
      {
        _submenuOpened = true;
        for (uint8_t i = 0; i < _submenuCount; i++)
        {
          _submenuList[i]->setVisible(true);
          _submenuList[i]->draw();
        }
        draw();
      }
    }
    void hideSubmenu(XitemClock* clock)
    {
      if (_submenuOpened)
      {
        _submenuOpened = false;
        for (uint8_t i = 0; i < _submenuCount; i++)
        {
          _submenuList[i]->setVisible(false);
          _submenuList[i]->draw();
          clock->redrawClipRect(_submenuList[i]);
        }
        draw();
      }
    }
    bool isSubmenuOpened()
    {
      return _submenuOpened;
    }
    byte getSubmenuCount()
    {
      return _submenuCount;
    }
    void draw() override
    {
      if (_visible)
      {
        item.createSprite(_w, _h);
        item.setSwapBytes(true);
        item.fillScreen(COLOR_BLACK);
        item.fillRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_BACK);
        if (_down)
        {
          item.drawRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_TEXT);
          item.drawRoundRect(1, 1, _w - 2, _h - 2, 7, COLOR_ITEM_TEXT);
        }
        // icon
        int xi = (_w / 2) - 24;
        if (_drawMode == drawmode_t::icontext)
        {
          xi = 8;
        }
        if (_icon && (_drawMode == drawmode_t::icon || _drawMode == drawmode_t::icontext))
        {
          LGFX_Sprite tmpSpr;
          tmpSpr.createFromBmpFile(SPIFFS, _icon);
          tmpSpr.pushSprite(&item, xi, (_h / 2) - 24, COLOR_BLACK);
        }
        // text
        int xt = (_w / 2);
        item.setTextDatum(lgfx::middle_center);
        if (_drawMode == drawmode_t::icontext)
        {
          item.setTextDatum(lgfx::middle_right);
          xt = _w - 8;
        }
        if (_text && (_drawMode == drawmode_t::text || _drawMode == drawmode_t::icontext))
        {
          if (_text && strncmp(_text, "#%", 2) == 0)
          {
            // draw circle percent
            int radius = 22; // just ender icon size
            int angle = 360 * atoi(&_text[2]) / 100;
            item.drawCircle(_w / 2, _h / 2, radius, SEGMENT_ON);
            item.drawCircle(_w / 2, _h / 2, radius - 1, SEGMENT_ON);
            item.fillArc(_w / 2, _h / 2, radius - 2, 13, 0, angle, COLOR_ITEM_TEXT);
          } else
          {
            item.setFont(&font_M);
            item.setTextColor(COLOR_ITEM_TEXT);
            item.drawString(_text, xt, _h / 2);
          }
        }
        item.pushSprite(&lcd, _x, _y);
        item.deleteSprite();
      }
    }
    void doTouch(bool longPress) override
    {
      if (_onTouch)
      {
        _onTouch(INDEX_BTN1 + _idx, longPress, _tapCount);
      }
    }
  protected:
    uint8_t _idx;
    XitemSubmenu** _submenuList;
    uint8_t _submenuCount;
    bool _submenuOpened;
};

class XitemMenuWeather : public XitemMenu
{
  public:
    XitemMenuWeather(uint8_t idx, int xMargin, byte groupSize) : XitemMenu(idx, xMargin, groupSize)
    {
      _temperature = 23.4;
      _precipitation = 0;
      _wCode = 0;
    }
    void draw() override
    {
      if (_visible)
      {
        item.createSprite(_w, _h);
        item.setSwapBytes(true);
        item.fillScreen(COLOR_BLACK);
        item.fillRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_BACK);
        if (_down)
        {
          item.drawRoundRect(0, 0, _w, _h, 8, COLOR_ITEM_TEXT);
          item.drawRoundRect(1, 1, _w - 2, _h - 2, 7, COLOR_ITEM_TEXT);
        }
        //
        LGFX_Sprite tmpSpr;
        if (_icon)
        {
          tmpSpr.createFromBmpFile(SPIFFS, _icon);
          tmpSpr.pushSprite(&item, 8, (_h / 2) - 24, COLOR_BLACK);
          tmpSpr.deleteSprite();
        }
        item.setFont(&font_M);
        item.setTextColor(COLOR_ITEM_TEXT);

        item.setTextDatum(lgfx::top_right);
        item.drawString(String(_temperature, 1) + "°", _w - 8, 6);
        //
        byte tconv[8] = {6, 18, 31, 43, 56, 68, 81, 93};
        char bmp[20];
        byte di = 0;
        byte bi = 0;
        for (byte i = 0; i < 8; i++)
        {
          if (_precipitation > tconv[i])
          {
            di = 1 + (i % 2);
          }
          if (i % 2 == 1)
          {
            sprintf(bmp, "/bmp/drop-%d.bmp", di);
            tmpSpr.createFromBmpFile(SPIFFS, bmp);
            tmpSpr.pushSprite(&item, _w - 6 - (22 * (4 - bi)), _h - 24 - 7, COLOR_BLACK);
            tmpSpr.deleteSprite();
            di = 0;
            bi++;
          }
        }
        //
        item.pushSprite(&lcd, _x, _y);
        item.deleteSprite();
      }
    }
    void setWeather(float temperature, float precipitation, byte wCode, const char* iconWCode)
    {
      _temperature = temperature;
      _precipitation = precipitation;
      _wCode = wCode;
      // at last because it drawing
      setIcon(iconWCode);
    }
  protected:
    float _temperature;
    float _precipitation;
    byte _wCode;
};

class XitemPopup : public Xitem
{
  public:
    XitemPopup() : Xitem()
    {
      _x = 70;
      _y = PADDING_Y;
      _w = lcd.width() - 140;
      _h = DIGIT_HEIGHT / 3;
      _lineCount = 0;
    }
    void draw() override
    {
      if (_visible)
      {
        _lineCount = 1;
        const char* str = _txt.c_str();
        while (*str) if (*str++ == '\n') ++_lineCount;
        int textH = lcd.fontHeight(&font_S);
        _h = textH + (_lineCount * textH);
        lcd.setFont(&font_XS);
        lcd.setTextDatum(lgfx::middle_center);
        lcd.setTextColor(COLOR_ITEM_TEXT);
        lcd.drawRoundRect(_x, _y, _w, _h, 8, COLOR_ITEM_WHITE);
        lcd.drawRoundRect(_x + 1, _y + 1, _w - 2, _h - 2, 7, COLOR_ITEM_WHITE);
        lcd.fillRoundRect(_x + 2, _y + 2, _w - 4, _h - 4, 6, COLOR_ITEM_BACK);
        //
        int y = textH;
        for (byte i = 0; i < _lineCount; i++)
        {
          String s = getStringPartByNr(_txt, '\n', i);
          lcd.drawString(s, _x + _w / 2, _y + y);
          y += textH;
        }
        //
      } else
      {
        _lineCount = 0;
        lcd.fillRoundRect(_x, _y, _w, _h, 6, COLOR_BLACK);
      }
    }
    void setText(String txt)
    {
      _txt = txt;
      draw();
    }
    bool newIsSmaller(const char* str)
    {
      byte count = 1;
      while (*str) if (*str++ == '\n') ++count;
      return count < _lineCount;
    }
  protected:
    String _txt;
    byte _lineCount;
};

class Xcontroller
{
  public:
    Xcontroller()
    {
      _clock = NULL;
      _itemDown = NULL;
      _itemDownTime = 0;
      _itemEditing = NULL;
      _popupTime = 0;
      _submenuTime = 0;
      _weatherIdx = -1;
    }
    void begin(bool clockShowSecond)
    {
      lcd.clear();
      if (clockShowSecond)
        DIGIT_WIDTH = lcd.width() / 8;
      else
        DIGIT_WIDTH = lcd.width() / 5;
      DIGIT_HEIGHT = DIGIT_WIDTH * 2.08;

      SEGMENT_WIDTHNESS = DIGIT_WIDTH / 6;

      initClockSegment(&segmentHon, DIGIT_WIDTH - (SEGMENT_WIDTHNESS * 2) + (GX_ABC * 2), SEGMENT_WIDTHNESS, true);
      initClockSegment(&segmentHoff, DIGIT_WIDTH - (SEGMENT_WIDTHNESS * 2) + (GX_ABC * 2), SEGMENT_WIDTHNESS, false);

      initClockSegment(&segmentV1on, SEGMENT_WIDTHNESS, (DIGIT_HEIGHT / 2) - (3 * SEGMENT_WIDTHNESS / 2) + GY_DE + GY_FG, true);
      initClockSegment(&segmentV1off, SEGMENT_WIDTHNESS, (DIGIT_HEIGHT / 2) - (3 * SEGMENT_WIDTHNESS / 2) + GY_DE + GY_FG, false);

      initClockSegment(&segmentV2on, SEGMENT_WIDTHNESS, (DIGIT_HEIGHT / 2) - (3 * SEGMENT_WIDTHNESS / 2) + GY_DE + GY_FG, true);
      initClockSegment(&segmentV2off, SEGMENT_WIDTHNESS, (DIGIT_HEIGHT / 2) - (3 * SEGMENT_WIDTHNESS / 2) + GY_DE + GY_FG, false);

      MENU_TOP = PADDING_Y + DIGIT_HEIGHT + PADDING_Y;
      MENU_HEIGHT = lcd.height() - MENU_TOP - PADDING_Y;
      MENU_WIDTH = lcd.width();

      int countZone = 0;
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        countZone += itemsSizing[i];
      }

      ITEM_WIDTH = (MENU_WIDTH - (ITEM_GAP * countZone)) / countZone;
      ITEM_HEIGHT = MENU_HEIGHT - (ITEM_GAP * 2);

      // after all sizing calculation
      _clock = new XitemClock(clockShowSecond);
      int xMargin = 0;
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        if (itemsSizing[i] == 2)
        {
          _items[i] = new XitemMenuWeather(i, xMargin, itemsSizing[i]);
          _weatherIdx = i;
        } else
        {
          _items[i] = new XitemMenu(i, xMargin, itemsSizing[i]);
        }
        xMargin += (ITEM_WIDTH * itemsSizing[i]) + ITEM_GAP;
      }
      _popup = new XitemPopup();
    }
    void setItemEditing(Xitem* item, bool doevent)
    {
      _itemEditing = item;
      if (_itemEditing)
      {
        _itemEditing->startEditing();
      }
    }
    void drawItems()
    {
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        _items[i]->draw();
      }
    }
    void drawPopup(String txt)
    {
      if (!_popup->isVisible())
      {
        _clock->setVisible(false);
        _popup->setVisible(true);
      } else {
        if (_popup->newIsSmaller(txt.c_str()))
        {
          _popup->setVisible(false);
          _clock->redrawClipRect(_popup);
          _popup->setVisible(true);
        }
      }
      _popup->setText(txt);
      // in last for not filling black in drawing
      _popupTime = TEMPO_AUTO_OFF_POPUP;
    }
    void openSubmenu(byte idx)
    {
      if (idx == 0 || idx > MAX_ITEMS) return;
      _clock->setVisible(false);
      _items[idx - 1]->showSubmenu();
      _submenuTime = TEMPO_AUTO_OFF_SUBMENU;
    }
    void closeAllSubmenu()
    {
      int x, y, w, h;
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        _items[i]->hideSubmenu(_clock);
      }
      _clock->setVisible(_popupTime == 0);
    }
    bool isSubmenuOpened()
    {
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        if (_items[i]->isSubmenuOpened()) return true;
      }
      return false;
    }
    void loop(bool topSecond)
    {
      static bool maskedByLongPress = false;
      int32_t x, y;
      if (lcd.getTouch(&x, &y))
      {
        if (!maskedByLongPress)
        {
          Xitem* detectedItem;
          detectedItem = _clock->isTouched(x, y);
          if (detectedItem == NULL)
          {
            for (byte i = 0; detectedItem == NULL && i < MAX_ITEMS; i++)
            {
              detectedItem = _items[i]->isTouched(x, y);
              if (detectedItem == NULL && (((XitemMenu*)_items[i])->isSubmenuOpened()))
              {
                int nb = ((XitemMenu*)_items[i])->getSubmenuCount();
                for (byte j = 0; detectedItem == NULL && j < nb; j++)
                {
                  detectedItem = ((XitemMenu*)_items[i])->getSubmenu(j)->isTouched(x, y);
                  if (detectedItem)
                  {
                    _submenuTime = TEMPO_AUTO_OFF_SUBMENU;
                  }
                }
              }
            }
          }
          if (detectedItem)
          {
            if (_itemDown != NULL && _itemDown != detectedItem)
            {
              _itemDown->setDown(false);
              _itemDown = NULL;
              _itemDownTime = 0;
            }
            if (_itemDown == NULL)
            {
              _itemDownTime = millis();
              _itemDown = detectedItem;
              _itemDown->setDown(true);
            }
          }
          if (_itemDown && _itemDown->getLongPressAuto() && (millis() > _itemDownTime + LONG_PRESS_DURATION))
          {
            maskedByLongPress = true;
            _itemDown->setDown(false);
            _itemDown->doTouch(true);
          }
        }
      } else
      {
        if (!maskedByLongPress && _itemDown != NULL)
        {
          bool longpress = (millis() - _itemDownTime) > 800;
          _itemDown->setDown(false);
          _itemDown->doTouch(longpress);
        }
        maskedByLongPress = false;
        _itemDownTime = 0;
        _itemDown = NULL;
      }
      if (topSecond)
      {
        _clock->blink();
        for (byte i = 0; i < MAX_ITEMS; i++)
        {
          _items[i]->blink();
        }
        if (_popupTime)
        {
          _popupTime--;
          if (_popupTime == 0)
          {
            _popup->setVisible(false);
            _clock->redrawClipRect(_popup);
            _clock->setVisible(!isSubmenuOpened());
          }
        }
        if (_submenuTime)
        {
          _submenuTime--;
          if (_submenuTime == 0)
          {
            closeAllSubmenu();
          }
        }
        adjustLighting();
      }

      // check editing timeout
      if (_clock->checkEditingTimeout())
      {

      }
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        if (_items[i]->checkEditingTimeout())
        {

        }
      }
      // check tap count timeout
      if (_clock->checkTapTimeout())
      {

      }
      for (byte i = 0; i < MAX_ITEMS; i++)
      {
        if (_items[i]->checkTapTimeout())
        {

        }
      }
    }
    XitemClock* getClock()
    {
      return _clock;
    }
    XitemMenu* getMenu(byte idx)
    {
      if (idx == 0 || idx > MAX_ITEMS) return NULL;
      return _items[idx - 1];
    }
    XitemMenuWeather* getWeather()
    {
      if (_weatherIdx < 0 || _weatherIdx > MAX_ITEMS) return NULL;
      return (XitemMenuWeather*)_items[_weatherIdx];
    }
    void adjustLighting()
    {
      int a1024 = analogRead(LIGHT_SENSOR);
      int k = (100 * a1024) / 1023;
      if (k < lumSens)
      {
        lcd.setBrightness(lumLevel0);
      } else
      {
        lcd.setBrightness(lumLevel1);
      }
    }
  private:
    XitemClock* _clock;
    XitemMenu* _items[MAX_ITEMS];
    XitemPopup* _popup;
    Xitem* _itemDown;
    Xitem* _itemEditing;
    int _weatherIdx;
    uint32_t _popupTime;
    uint32_t _itemDownTime;
    uint32_t _submenuTime;

    void initClockSegment(LGFX_Sprite* sp, int w, int h, bool isOn)
    {
      sp->createSprite(w, h);
      sp->setColorDepth(16);
      sp->fillScreen(transpalette);
      if (isOn)
        sp->fillRoundRect(0, 0, w, h, SEGMENT_WIDTHNESS / 2, SEGMENT_ON);
      else
        sp->fillRoundRect(0, 0, w, h, SEGMENT_WIDTHNESS / 2, SEGMENT_OFF);
    };
};
