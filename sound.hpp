// https://doc.ubuntu-fr.org/liste_radio_france

#include <Audio.h> // https://github.com/schreibfaul1/ESP32-audioI2S
// I2S Connections
#ifdef TFT_SC01
#define I2S_DOUT      0
#else
#define I2S_DOUT      33
#endif
#define I2S_BCLK      26
#define I2S_LRC       25

Audio audio;
/*
  //https://www.radio-browser.info/search?page=1&order=clickcount&reverse=true&hidebroken=true&tagList=chillout
  //https://streamurl.link/
  // https://podcasts-francais.fr/

  // http://sv6.vestaradio.com/VertigeRadio
  //char station[] = "http://stream.dancewave.online:8080/dance.mp3";
  //char station[] = "http://strm112.1.fm/chilloutlounge_mobile_mp3";
  //char station[] = "http://kifcool.kifradio.com";
  //char station [] = "http://178.32.111.41:8027/stream-128kmp3-YogaChill";
  char station [] = "http://eu1.fastcast4u.com/proxy/kpmxz?mp=/1";
*/

const char defaultStation[] = "http://eu1.fastcast4u.com/proxy/kpmxz?mp=/1";

uint8_t stationIndex = 0;
uint8_t soundVolume = 10;
uint8_t wakeupVolume = 0;

typedef void (*callbackSound) (bool running);
callbackSound _onSound;
uint32_t sound_TimeToStop = 0;
uint32_t sound_SleepTimeTotal = 0;
uint32_t sound_SleepTimeStep = 600; // 10 min

void soundInit(callbackSound theFunc)
{
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(soundVolume); // 0...21
  sound_TimeToStop = 0;
  _onSound = theFunc;
}

bool soundStart(bool skip = false)
{
  if (!wifiok) return false;
  const char* station = defaultStation;
  if (skip)
  {
    audio.stopSong();
    stationIndex++;
    if (stationIndex >= stationArray.size())
      stationIndex = 0;
    //sound_SleepTimeTotal = 0;
    //sound_TimeToStop = 0;
  }
  if (stationArray.size() > stationIndex)
  {
    station = stationArray[stationIndex].as<const char*>();
    DEBUGf("station : %s\n", station);
  }
  if (audio.isRunning() || audio.connecttohost(station))
  {
    if (_onSound)
    {
      _onSound(audio.isRunning());
    }
    return true;
  }
  DEBUGln("Connect error");
  return false;
}

void soundStop()
{
  sound_SleepTimeTotal = 0;
  sound_TimeToStop = 1;
}

bool soundStartDuration()
{
  sound_SleepTimeTotal += sound_SleepTimeStep;
  sound_TimeToStop += sound_SleepTimeStep;
  return soundStart();
}

void soundLoop(bool topSecond)
{
  if (topSecond)
  {
    if (wakeupVolume < soundVolume)
    {
      wakeupVolume++;
      audio.setVolume(wakeupVolume);
    }
    if (sound_TimeToStop)
    {
      sound_TimeToStop--;
      if (sound_TimeToStop == 0)
      {
        audio.stopSong();
        if (_onSound)
        {
          _onSound(audio.isRunning());
        }
      }
    }
  }
  audio.loop();
}

String soundTimeRemaining()
{
  if (sound_TimeToStop)
  {
    int mn = sound_TimeToStop / 60;
    int sc = sound_TimeToStop % 60;
    char txt[10];
    sprintf(txt, "%d:%02d", mn, sc);
    return String(txt);
  }
  return "";
}

int soundPercentRemaining()
{
  if (sound_SleepTimeTotal)
  {
    return 100 * sound_TimeToStop / sound_SleepTimeTotal;
  }
  return 0;
}

// optional
void audio_showstation(const char *info)
{
  DEBUGf("station %s\n", info);
  String title = String(info);
  title.replace("- ", "\n");
  control.drawPopup(title);
}
void audio_showstreamtitle(const char *info)
{
  DEBUGf("streamtitle %s\n", info);
  /*
    String title = String(info);
    title.replace("- ", "\n");
    control.drawPopup(title);
  */
}
