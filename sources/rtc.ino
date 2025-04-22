#include <DS1302.h>
#include <TimeLib.h>

#define RTC_PIN_IO 27 // DATA
#define RTC_PIN_SCLK 14 // CLK
#define RTC_PIN_CE 16 // RST

using namespace std;

DS1302 rtc(RTC_PIN_CE, RTC_PIN_IO, RTC_PIN_SCLK); // setup rtc

const static Time::Day WeekDays[] = { // available 0-6
  Time::kSunday,
  Time::kMonday,
  Time::kTuesday,
  Time::kWednesday,
  Time::kThursday,
  Time::kFriday,
  Time::kSaturday,
};

void setup(){
    Serial.begin(115200);
    // ----- RTC -----
    rtc.writeProtect(false); // allow to write time
    rtc.halt(false); // false = let clock keep ticking

    String datetime ="2024-01-01T15:50:00";
    // Serial.println(datetime);
    int year = datetime.substring(0, 4).toInt();
    int month = datetime.substring(5, 7).toInt();
    int date = datetime.substring(8, 10).toInt();
    int hour = datetime.substring(11, 13).toInt();
    int minute = datetime.substring(14, 16).toInt();
    int second = datetime.substring(17, 19).toInt();
    int day = datetime.substring(20, 21).toInt();
    Time now(year, month, date, hour, minute, second, WeekDays[day]);
    rtc.time(now);
}

void loop(){
    Time now = rtc.time();
    string time = to_string(now.yr) + "-" +
        ((now.mon < 10) ? "0" : "") + to_string(now.mon) + "-" +
        ((now.date < 10) ? "0" : "") + to_string(now.date) + "T" +
        ((now.hr < 10) ? "0" : "") + to_string(now.hr) + ":" +
        ((now.min < 10) ? "0" : "") + to_string(now.min) + ":" +
        ((now.sec < 10) ? "0" : "") + to_string(now.sec);
    Serial.println(time.c_str());
    delay(1000);
}