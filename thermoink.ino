#include "M5CoreInk.h"
#include <WiFi.h>
#include "time.h"
#include <Ticker.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>
#define LGFX_M5STACK_COREINK
#include <LovyanGFX.hpp>

static LGFX gfx;
LGFX_Sprite sp(&gfx);

int w;
int h;

const char *ssid = "XXXXXXXXXX";
const char *password = "YYYYYYYYYY";

const char *ntpServer = "ntp.jst.mfeed.ad.jp";

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;

char btnString[256] = "Push any button";

unsigned long last_update_time = 0;
unsigned long no_input_time = 0;
bool draw_request = false;

// ENT Unit II
float temp = 0;
Adafruit_SHT31 sht3x = Adafruit_SHT31(&Wire);
Adafruit_BMP280 bme = Adafruit_BMP280(&Wire);

float tmp = 0.0;
float hum = 0.0;
float pressure = 0.0;

uint32_t getBatteryVoltage(void) {
  return analogRead(35) * 25.1 / 5.1;
}

double Pf[] = {
  1000.0, 1008.0, 1013.25, 1015.0, 1017.0, 1020.0
};
int Pfi = 3;

float getHeight(float pa, float tmp)
{
#if 0
  float A = pa / 101325;
  float B = 1 / 5.25588;
  float C = pow(A, B);
  C = 1.0 - C;
  C = C / 0.0000225577;
  return C;
#else
  double hPa = pa / 100.0;
  double P = 1013.25;
  double P0 = Pf[Pfi];
  double s = 1.0 / 5.25588;
  double p = pow(P0 / hPa, s) - 1.0;
  double T = tmp + 273.15;
  double cP = pow(P / P0, s);
  return (float)(((p * T) / 0.0065) * cP);
#endif
}

void draw()
{
  unsigned long ofs = draw_request ? 1000 : 15000;
  if (millis() - last_update_time < ofs)
  {
    return;
  }

  M5.rtc.GetTime(&RTCtime);
  M5.rtc.GetDate(&RTCDate);

  gfx.startWrite();
  gfx.setTextSize(1.0);

  // 環境情報
  static const char* pn[] = {"嵐", "低", "普", "晴", "冬", "高"};
  char envBuff[64];
  sprintf(envBuff, "気温:%0.1f℃ ", tmp);
  gfx.drawString(envBuff, 5, 70);
  sprintf(envBuff, "湿度:%0.1f%% ", hum);
  gfx.drawString(envBuff, 5, 95);
  sprintf(envBuff, "気圧:%0.2fhPa ", pressure / 100.0);
  gfx.drawString(envBuff, 5, 120);
  sprintf(envBuff, "高度:%0.1fm(%s) ", getHeight(pressure, tmp), pn[Pfi]);
  gfx.drawString(envBuff, 5, 145);

  // DateTime
  static const char *wd[7] = {"日", "月", "火", "水", "木", "金", "土"};
  char dateBuff[64];
  char timeBuff[64];
  sprintf(dateBuff, "%d/%02d/%02d(%s)", RTCDate.Year, RTCDate.Month, RTCDate.Date, wd[RTCDate.WeekDay]);
  sprintf(timeBuff, "%02d:%02d", RTCtime.Hours, RTCtime.Minutes);
  gfx.drawString(dateBuff, 5, 5);
  gfx.setTextSize(1.5);
  gfx.drawString(timeBuff, 20, 30);

  // バッテリ
  gfx.drawRoundRect(120, 38, 70, 20, 4, TFT_GREEN);
  auto bv = max(min(getBatteryVoltage(), (uint32_t)4200), (uint32_t)3600);
  int br = (int)((((float)bv - 3600.0f) / 600.0f) * 66.0f);
  gfx.fillRect(122, 40, br, 16, TFT_GREEN);

  gfx.endWrite();

  draw_request = false;
  last_update_time = millis();
}

void updateEnv()
{
  pressure = bme.readPressure();
  tmp = sht3x.readTemperature();
  hum = sht3x.readHumidity();
}

void timeSetting()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // get time from NTP server(UTC+9)
  configTime(9 * 3600, 0, ntpServer);

  // Get local time
  struct tm timeInfo;
  if (getLocalTime(&timeInfo))
  {
    // Set RTC time
    RTC_TimeTypeDef TimeStruct;
    TimeStruct.Hours = timeInfo.tm_hour;
    TimeStruct.Minutes = timeInfo.tm_min;
    TimeStruct.Seconds = timeInfo.tm_sec;
    M5.rtc.SetTime(&TimeStruct);

    RTC_DateTypeDef DateStruct;
    DateStruct.WeekDay = timeInfo.tm_wday;
    DateStruct.Month = timeInfo.tm_mon + 1;
    DateStruct.Date = timeInfo.tm_mday;
    DateStruct.Year = timeInfo.tm_year + 1900;
    M5.rtc.SetDate(&DateStruct);
  }

  //disconnect WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  draw_request = true;
}

void setup()
{
  M5.begin();
  if (!M5.M5Ink.isInit())
  {
    Serial.printf("Ink Init faild");
  }
  gfx.init();

  gfx.setRotation(0);

  w = gfx.width();
  h = gfx.height();
  gfx.setFont(&fonts::lgfxJapanGothic_24);

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
  }
  if (!sht3x.begin(0x44)) {
    Serial.println("Could not find a valid SHT3X sensor, check wiring!");
  }
  no_input_time = millis();
  draw_request = true;
}

void loop()
{
  updateEnv();

  if (M5.BtnUP.wasPressed())
  {
    Pfi -= Pfi > 0 ? 1 : 0;
    draw_request = true;
  }
  if (M5.BtnDOWN.wasPressed())
  {
    Pfi += Pfi < 5 ? 1 : 0;
    draw_request = true;
  }
  if (M5.BtnMID.wasPressed())
  {
  }
  if (M5.BtnEXT.wasPressed())
  {
    timeSetting();
  }
  if (M5.BtnPWR.wasPressed())
  {
    M5.shutdown();
  }
  if (draw_request)
  {
    no_input_time = millis();
  }
  else if (millis() - no_input_time > 15 * 1000)
  {
    M5.shutdown(60);
  }
  draw();
  M5.update();
  delay(100);
}
