// Local network version
#include <DS1302.h>
#include <TimeLib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define RTC_PIN_IO 27 // DATA
#define RTC_PIN_SCLK 14 // CLK
#define RTC_PIN_CE 16 // RST

#define SERVO 26

#define FOOD_SENSOR 34
#define FOOD_SERVO_DELAY 500 // ms
#define FEED_TIMES 3
#define FOOD_THRESHOLD 3500 // (0 - 4095 || 100% - 37.5%) left

#define TEMPERATURE_SENSOR 13

#define WATER_LEVEL_SENSOR 35
#define DC_MOTER_OUT 5
#define DC_MOTER_IN 23

#define VCC_LED 18
#define GND_LED 19
#define PWM_LED 2

#define SOFTAP_WIFI_SSID  "FT-CTRL"
#define SOFTAP_WIFI_PASS  "<Your WiFi-Password>"

using namespace std;

struct FeedTimer{
    int Hour;
    int Minute;
};

DS1302 rtc(RTC_PIN_CE, RTC_PIN_IO, RTC_PIN_SCLK); // setup rtc
WebServer webServer(80);
Servo myservo;
Preferences pref;

OneWire oneWire(TEMPERATURE_SENSOR);
DallasTemperature tempSensor(&oneWire);

const static Time::Day WeekDays[] = { // available 0-6
  Time::kSunday,
  Time::kMonday,
  Time::kTuesday,
  Time::kWednesday,
  Time::kThursday,
  Time::kFriday,
  Time::kSaturday,
};

int foodLvl;
bool isFeeding;
bool isWaterChanging;
float temperatureValue;

unsigned long dateTimeToSeconds(Time time) {
  tmElements_t tm;
  tm.Year = time.yr - 1970;
  tm.Month = time.mon;
  tm.Day = time.date;
  tm.Hour = time.hr;
  tm.Minute = time.min;
  tm.Second = time.sec;
  return makeTime(tm);
}

void setupWiFiAP(){
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SOFTAP_WIFI_SSID, SOFTAP_WIFI_PASS);

    String esp32IPAddress = WiFi.softAPIP().toString();

    Serial.println("Connect to ESP32 at \"" + String(SOFTAP_WIFI_SSID) + "\" IP: " + esp32IPAddress);
    Serial.println("http://" + esp32IPAddress);
}

void feed(){
    if(!isFeeding){
        isFeeding = true;
        for(int i = 1; i <= FEED_TIMES; i++){
            myservo.write(180);
            delay(FOOD_SERVO_DELAY);
            if(i == FEED_TIMES){
                foodLvl = analogRead(FOOD_SENSOR);
            }
            myservo.write(0);
            delay(FOOD_SERVO_DELAY);
        }
        if(foodLvl > FOOD_THRESHOLD){ // if distance more than threshold
            Serial.println("FOOD IS LOW");
        }else{
            Serial.println("FOOD IS ENOUGH");
        }
        Serial.flush();
        isFeeding = false;
    }
}

void changeWater(){
    if(!isWaterChanging){
        isWaterChanging = true;
        int waterLvl = analogRead(WATER_LEVEL_SENSOR);
        int waterLvlTresh = waterLvl;
        Serial.println("PUMP OUT");
        digitalWrite(DC_MOTER_OUT, LOW); // start pump out moter
        digitalWrite(DC_MOTER_IN, HIGH); // stop pump IN moter
        while(waterLvl > 1500){
            waterLvl = analogRead(WATER_LEVEL_SENSOR);
            Serial.println(waterLvl);
            delay(500);
        }
        Serial.println("STOP PUMP OUT");
        digitalWrite(DC_MOTER_OUT, HIGH); // stop pump out moter
        Serial.println("PUMP IN");
        digitalWrite(DC_MOTER_IN, LOW); // start pump IN moter
        while(waterLvl < waterLvlTresh){
            waterLvl = analogRead(WATER_LEVEL_SENSOR);
            Serial.println(waterLvl);
            delay(500);
        }
        digitalWrite(DC_MOTER_IN, HIGH); // start pump IN moter
        isWaterChanging = false;
    }
    Serial.println("Changing water's done!!!");
}

const char *index_html PROGMEM = R"(
<!DOCTYPE html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <meta charset=\UTF-8\>
        <title>LED Control</title>
        <style>
            .checkbox-wrapper-2 .ikxBAC {
                appearance: none;
                background-color: #dfe1e4;
                border-radius: 72px;
                border-style: none;
                flex-shrink: 0;
                height: 2vw;
                margin: 0;
                position: relative;
                width: 3vw;
            }

            .checkbox-wrapper-2 .ikxBAC::before {
                bottom: -6px;
                content: "";
                left: -6px;
                position: absolute;
                right: -6px;
                top: -6px;
            }

            .checkbox-wrapper-2 .ikxBAC,
            .checkbox-wrapper-2 .ikxBAC::after {
                transition: all 100ms ease-out;
            }

            .checkbox-wrapper-2 .ikxBAC::after {
                background-color: #fff;
                border-radius: 50%;
                content: "";
                height: 1.4vw;
                left: 0.3vw;
                position: absolute;
                top: 0.3vw;
                width: 1.4vw;
            }

            .checkbox-wrapper-2 input[type=checkbox] {
                cursor: default;
            }

            .checkbox-wrapper-2 .ikxBAC:hover {
                background-color: #c9cbcd;
                transition-duration: 0s;
            }

            .checkbox-wrapper-2 .ikxBAC:checked {
                background-color: #6e79d6;
            }

            .checkbox-wrapper-2 .ikxBAC:checked::after {
                background-color: #fff;
                left: 1.3vw;
            }

            .checkbox-wrapper-2 :focus:not(.focus-visible) {
                outline: 0;
            }

            .checkbox-wrapper-2 .ikxBAC:checked:hover {
                background-color: #535db3;
            }

            table, tr, th, td {
                width: 20%;
                border:1px solid black;
                border-collapse: collapse;
                text-align: center;
            }

            .bgLvl {
                width: 20%;
                background-color: #ddd;
                display:inline-block;
            }

            .lvl {
                width: 0%; 
                height: 15px;
                background-color: #04AA6D;
            }

            .block {
                border-radius: 4px;
                padding: 2%;
                width: 95%;
                margin-top: 2%;
                font-size: 1.5vw;
            }

            .innerBlock {
                margin-left: 3%; 
                padding: 2%;
            }

            .headInnerText {
                padding: 2%;
            }

            .feedTimer {
                background-color: rgb(194, 219, 214);
            }

            .innerFeedTimer {
                background-color: rgb(255, 255, 255);
            }

            .waterData {
                background-color: rgb(160, 184, 216);
            }

            .innerWaterData {
                background-color: rgb(255, 255, 255);
            }

            .dateTimeINP {
                font-size: 1.5vw;
            }

            .button_del {
                width: 100%; 
                font-size: 1.5vw;
                border: none;
            }

            .button_now {
                padding: 1.5% 2.5%;
                font-size: 1vw;
                background-color: #04AA6D;
                border: none;
                color: white;
                border-radius: 8px;
            }
            
            .button_now:active {
                background-color: #056b46;
            }
            @media screen and (max-width: 800px) {
                table, tr, th, td{
                    width: 100%;
                }

                .bgLvl {
                    width: 60%;
                }

                .button_now {
                    font-size: 5vw;
                }

                .block{
                    font-size: 5vw;
                }

                .button_del{
                    font-size: 5vw;
                }

                .dateTimeINP {
                    font-size: 4vw;
                }

                .checkbox-wrapper-2 .ikxBAC {
                    height: 4vw;
                    width: 6vw;
                }

                .checkbox-wrapper-2 .ikxBAC::after {
                    height: 2.8vw;
                    left: 0.6vw;
                    top: 0.6vw;
                    width: 2.8vw;
                }

                .checkbox-wrapper-2 .ikxBAC:checked::after {
                    left: 2.6vw;
                }
            }
        </style>
    </head>
    <body>
        <div>
            <h1>FishTank Control</h1>
            <div class="block">
                <div class="checkbox-wrapper-2">
                    <input id="autoTime" type="checkbox" class="sc-gJwTLC ikxBAC"> Time Auto Set
                </div>
                <input class="dateTimeINP" id="DT" type='datetime-local' value=''></br>
            </div>

            <div class="block feedTimer"><h3>Feeding routine</h3>
                <div class="innerBlock innerFeedTimer">
                    <label style="display:inline-block; margin-top: 0px;">Food Level : </label>
                    <div class="bgLvl">
                        <div id="foodLvl" class="lvl"></div>
                    </div>
                    <table>
                        <thead>
                            <tr>
                                <th>Hour</th>
                                <th>Minute</th>
                                <th>Management</th>
                            </tr>
                        </thead>
                        <tbody id="feedTable">
                        </tbody>
                    </table>
                    <div style="margin-top: 1%;">
                        <label for="hour">Hour:</label>
                        <input type="number" id="hour" min="0" max="23" onkeyup="if(value != ''){if(value<0) value=0; else if(value>23) value=23;}" required>
                        <label for="minute">Minute:</label>
                        <input type="number" id="minute" min="0" max="59" onkeyup="if(value != ''){if(value<0) value=0; else if(value>59) value=59;}" required>
                        <input type='button' onclick='addAlarm()' value='+'>
                    </div>
                </div>
                <div style="margin: 1%">
                    <input class='button_now' type='button' value='FEED NOW' onclick='feedNow()'>
                </div>
            </div>
            <div class="block waterData"><h3>Water Data : </h3>
                <div class="innerBlock innerWaterData">
                    <label style="display:inline-block; margin-top: 0px;">Water Temperature : </label>
                    <div style="display:inline-block;" id="TemperatureSensor"></div>
                </div>
                <div style="margin: 1%">
                    Change Water Every <label id="routine"></label> Day(s).
                    <input class='button_now' type='button' value='Edit' onclick='editWaterRoutine()'>
                </div>
                <div style="margin: 1%">
                    <input class='button_now' type='button' value='CHANGE WATER NOW' onclick='changeWaterNow()'>
                </div>
            </div>
            <div class="block waterData"><h3>Light Control : </h3>
                <div style="margin: 1%">
                </div>
            </div>
        </div>

        <script>
            setInterval(async () => {
                const response = await fetch("/getTime");
                const data = await response.json();

                console.log(data.Time);
                document.getElementById("DT").value = data.Time.toString();
            }, 1000);

            setInterval(async () => {
                const response = await fetch("/getFood");
                const data = await response.json();

                document.getElementById("foodLvl").style.width = data.foodLvl + "%";
            }, 1000);

            setInterval(async () => {
                const response = await fetch("/getTemperature");
                const data = await response.json();

                let temperature;
                if(data.TemperatureSensor){
                    temperature = Number(data.TemperatureSensor).toFixed(2);
                    console.log(temperature)
                }else{
                    temperature = "NaN";
                }
                document.getElementById("TemperatureSensor").innerHTML = temperature + " &degC";
            }, 5000);

            var isAutoTime = document.getElementById("autoTime");
            isAutoTime.addEventListener('change', set_time);

            var DTSet = document.getElementById("DT");
            DTSet.addEventListener('change', set_rtc);

            function addAlarm(){
                var hourInput = document.getElementById("hour").value;
                var minuteInput = document.getElementById("minute").value;
    
                if (hourInput === "" || minuteInput === "" || hourInput < 0 || hourInput > 23 || minuteInput < 0 || minuteInput > 59) {
                    alert("โปรดใส่เวลาที่ถูกต้อง");
                    return;
                }

                var alarmTable = document.getElementById("feedTable");

                var row = alarmTable.insertRow();
                var cell1 = row.insertCell(0);
                var cell2 = row.insertCell(1);
                var cell3 = row.insertCell(2);

                cell1.innerHTML = (hourInput>9 ? "":"0") + hourInput;
                cell2.innerHTML = (minuteInput>9 ? "":"0") + minuteInput;
                cell3.innerHTML = "<button class='button_del' onclick='removeAlarm(this)'>del</button>";

                var rows = alarmTable.rows;
                var rowsArray = Array.from(rows); // convert rows to array
                rowsArray.sort((a, b) => {
                    console.log(a, b)
                    var hourA = parseInt(a.cells[0].innerHTML);
                    var minuteA = parseInt(a.cells[1].innerHTML);
                    var hourB = parseInt(b.cells[0].innerHTML);
                    var minuteB = parseInt(b.cells[1].innerHTML);
                    if (hourA === hourB) {
                        return minuteA - minuteB;
                    } else {
                        return hourA - hourB;
                    }
                });
                rowsArray.forEach(row => alarmTable.appendChild(row));
                
                fetch("/addFeedTimer?hour=" + hourInput + "&minute=" + minuteInput);
            }

            function removeAlarm(btn) {
                var row = btn.parentNode.parentNode;
                var hour = row.cells[0].innerHTML;
                var minute = row.cells[1].innerHTML;
                row.parentNode.removeChild(row);
                fetch("/delFeedTimer?hour=" + hour + "&minute=" + minute);
            }

            function set_time(){
                checkbox = document.getElementById("autoTime").checked;
                datetime = document.getElementById("DT");

                if(checkbox){
                    datetime.disabled = true;
                    var d = new Date();
                    var currentdate = d.getFullYear() + "-"
                        + String(d.getMonth()+1).padStart(2, '0')  + "-" 
                        + String(d.getDate()).padStart(2, '0') + "T"  
                        + String(d.getHours()).padStart(2, '0') + ":"  
                        + String(d.getMinutes()).padStart(2, '0') + ":"  
                        + String(d.getSeconds()).padStart(2, '0');
                    datetime.value = currentdate;
                    set_rtc();
                }else{
                    datetime.disabled = false;
                }
            }

            function set_rtc(){
                var datetime = document.getElementById("DT").value;
                var d = new Date(datetime);
                let day = d.getDay();
                fetch("/timeSet?datetime=" + encodeURIComponent(datetime)+"-"+day);
            }

            async function loadData() {
                const response = await fetch("/getData");
                const data = await response.json();

                var alarmTable = document.getElementById("feedTable");
                var arrData = data.feedTimer;
                for(i=0; i<arrData.length; i+=2){
                    var row = alarmTable.insertRow();
                    var cell1 = row.insertCell(0);
                    var cell2 = row.insertCell(1);
                    var cell3 = row.insertCell(2);

                    Hour = arrData[i];
                    Min = arrData[i+1];
                    cell1.innerHTML = (Hour>9 ? "":"0") + Hour;
                    cell2.innerHTML = (Min>9 ? "":"0") + Min;
                    cell3.innerHTML = "<button class='button_del' onclick='removeAlarm(this)'>del</button>";
                }

                document.getElementById("routine").innerHTML = data.CWR;
            }

            function feedNow(){
                fetch("/feedNow");
            }

            function editWaterRoutine(){
                var routine = prompt("Edit Changing Water Routine in Integer");
                if(Number(routine) && routine>0){
                    fetch("/routine?routine=" + Math.round(routine));
                    document.getElementById("routine").innerHTML = Math.round(routine);
                }else if(routine != null){
                    alert('Please enter a valid value. (Integer > 0)');
                }
            }

            function changeWaterNow(){
                fetch("/changeWaterNow");
            }
            
            loadData();
        </script>
    </body>
</html>
)";

void setupWebServer() {
    webServer.on("/", HTTP_GET, []() {
        webServer.send(200, "text/html", index_html);
    });

    webServer.on("/getTime", HTTP_GET, []() { // Send Time Data as Json to webserver
        Time now = rtc.time();
        string time = to_string(now.yr) + "-" +
                  ((now.mon < 10) ? "0" : "") + to_string(now.mon) + "-" +
                  ((now.date < 10) ? "0" : "") + to_string(now.date) + "T" +
                  ((now.hr < 10) ? "0" : "") + to_string(now.hr) + ":" +
                  ((now.min < 10) ? "0" : "") + to_string(now.min) + ":" +
                  ((now.sec < 10) ? "0" : "") + to_string(now.sec);

        StaticJsonDocument<200> doc;
        doc["Time"] = time;

        String json;
        serializeJson(doc, json);

        webServer.send(200, "application/json", json);
    });

    webServer.on("/timeSet", HTTP_GET, []() { // update time in RTC
        if (webServer.hasArg("datetime")) {
            String datetime = webServer.arg("datetime");
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
    });

    webServer.on("/getFood", HTTP_GET, []() { // update food level to web
        StaticJsonDocument<200> doc;
        // Serial.println(String(persentage) + "% , " + String(foodLvl));
        if(foodLvl == 4095){
            doc["foodLvl"] = 0;
        }else{
            float persentage = 100 - (((float)foodLvl/4095.0) * 62.5); 
            doc["foodLvl"] = persentage;
        }

        String json;
        serializeJson(doc, json);

        webServer.send(200, "application/json", json);
    });

    webServer.on("/getTemperature", HTTP_GET, []() { // update food level to web
        StaticJsonDocument<200> doc;

        doc["TemperatureSensor"] = temperatureValue;

        String json;
        serializeJson(doc, json);

        webServer.send(200, "application/json", json);
    });

    webServer.on("/addFeedTimer", HTTP_GET, []() { // Add time in NVS
        if (webServer.hasArg("hour") && webServer.hasArg("minute")) {
            String hour = webServer.arg("hour");
            String minute = webServer.arg("minute");
            
            int timerLen = pref.getBytesLength("feedTimer");
            uint8_t timerBuffer[timerLen];
            pref.getBytes("feedTimer", timerBuffer, timerLen);

            uint8_t Hr = atoi(hour.c_str());
            uint8_t Min = atoi(minute.c_str());

            timerBuffer[timerLen] = Hr;
            timerBuffer[timerLen + 1] = Min;
            timerLen += 2;

            Serial.println("Sorting Data");
            if(timerLen > 2){
                for(int i=0; i<timerLen-2; i+=2){ // sort min -> max
                    if((timerBuffer[i]*60 + timerBuffer[i+1]) > (timerBuffer[i+2]*60 + timerBuffer[i+2])){
                        uint8_t tempHr = timerBuffer[i];
                        uint8_t tempMin = timerBuffer[i+1];
                        timerBuffer[i] = timerBuffer[i+2];
                        timerBuffer[i+1] = timerBuffer[i+3];
                        timerBuffer[i+2] = tempHr;
                        timerBuffer[i+3] = tempMin;
                        i = -2;
                    }
                }
            }
            for (int x=0; x<timerLen; x++) {
                Serial.print(timerBuffer[x]);
                Serial.print(", ");
            }
            
            pref.putBytes("feedTimer", timerBuffer, timerLen);
            Serial.println("added " + String(Hr) + ":" + String(Min));
            Serial.flush();
        }
    });

    webServer.on("/delFeedTimer", HTTP_GET, []() { // Delete time in NVS
        if (webServer.hasArg("hour") && webServer.hasArg("minute")) {
            String hour = webServer.arg("hour");
            String minute = webServer.arg("minute");
            
            int timerLen = pref.getBytesLength("feedTimer");
            uint8_t timerBuffer[timerLen];
            uint8_t lastTimerBuffer[timerLen - 2];
            pref.getBytes("feedTimer", timerBuffer, timerLen);

            uint8_t Hr = atoi(hour.c_str());
            uint8_t Min = atoi(minute.c_str());

            if(timerLen > 2){
                bool isDel = false;
                for(int i=0; i<(timerLen-2); i+=2){
                    if(timerBuffer[i] == Hr && timerBuffer[i+1] == Min){
                        isDel = true;
                    }
                    if(isDel){
                        lastTimerBuffer[i] = timerBuffer[i+2];
                        lastTimerBuffer[i+1] = timerBuffer[i+3];
                    }else{
                        lastTimerBuffer[i] = timerBuffer[i];
                        lastTimerBuffer[i+1] = timerBuffer[i+1];
                    }
                }
            }
            Serial.println(sizeof(lastTimerBuffer));
            for (int x=0; x<sizeof(lastTimerBuffer); x++) {
                Serial.print(lastTimerBuffer[x]);
                Serial.print(", ");
            }
            if(sizeof(lastTimerBuffer) == 0){
                pref.remove("feedTimer");
            }else{
                pref.putBytes("feedTimer", lastTimerBuffer, sizeof(lastTimerBuffer));
            }
            Serial.println("deleted " + String(Hr) + ":" + String(Min));
            Serial.flush();
        }
    });

    webServer.on("/routine", HTTP_GET, []() { // Delete time in NVS
        if (webServer.hasArg("routine")) {
            String routine = webServer.arg("routine");
            
            int routineInNVS = pref.putInt("waterChanging", atoi(routine.c_str()));
            Serial.println("Update to Changing Water Routine Every " + routine + " Day(s)");
            Serial.flush();
        }
    });

    webServer.on("/getData", HTTP_GET, []() { // send feedTimer & Changing Water Routine to web
        Serial.println("web requested");
        Serial.flush();
        int timerLen = pref.getBytesLength("feedTimer");
        uint8_t timerBuffer[timerLen];
        pref.getBytes("feedTimer", timerBuffer, timerLen);

        int changeRoutine = pref.getInt("waterChanging", 1); // day

        String json;
        json = "{\"feedTimer\":[";
        for(int i=0; i < timerLen; i+=2){
            json = json + String(timerBuffer[i]) + "," + String(timerBuffer[i+1]) + ((i != (timerLen-2)) ? "," : "");
        }
        json += "],";
        json = json + "\"CWR\":" + String(changeRoutine);
        json += "}";
        Serial.println(json);

        webServer.send(200, "application/json", json);
    });

    webServer.on("/feedNow", HTTP_GET, []() {
        Serial.println("start feed!!!");
        feed();
    });

    webServer.on("/changeWaterNow", HTTP_GET, []() {
        Serial.println("start changing water!!!");
        changeWater();
    });
    
    webServer.begin();
}

void FoodSensor(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int timeToSleep = 1; // second
    while(true){
        // get data from RTC
        Time now = rtc.time();
        uint8_t hr_now = now.hr;
        uint8_t min_now = now.min;

        int timerLen = pref.getBytesLength("feedTimer");
        uint8_t feedTimer[timerLen];
        pref.getBytes("feedTimer", feedTimer, timerLen);

        for(int i=0; i<timerLen; i+=2){
            // Serial.println("next time to feed is " + String(feedTimer[i]) + ":" + String(feedTimer[i+1]));

            if((feedTimer[i] == hr_now) && (feedTimer[i+1] == min_now)){
                feed();
                timeToSleep = 60;
                break;
            }else{
                // timeToSleep = (feedTimer[i]-hr_now)*3600 + (feedTimer[i+1]-min_now)*60;
                timeToSleep = 1;
                break;
            }
        }

        // Serial.println("Sleep for " + String(timeToSleep) + " Second");
        // Serial.flush();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
    }
}

void TemperatureSensor(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    while(true){
        tempSensor.requestTemperatures();
        temperatureValue = tempSensor.getTempCByIndex(0);
        // if (temperatureValue != DEVICE_DISCONNECTED_C) {
        //     Serial.println("Temperature : " + String(temperatureValue));
        // }else{
        //     Serial.println("Error: Could not read temperature data");
        // }
        // Serial.flush();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}

void WaterChanging(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int stamp = dateTimeToSeconds(rtc.time());
    int timeToSleep = 6*60*60; // 6 hours
    while(true){
        int now = dateTimeToSeconds(rtc.time());
        int changeRoutine = pref.getInt("waterChanging", 1); // day

        // Serial.println(String(now) +"-"+ String(stamp) + " = " + String(now - stamp));
        if((now - stamp) >= (changeRoutine*24*60*60)){ // compare id second
            changeWater();
            stamp = now;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
    }
}

void LEDControl(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int timeToSleep = 1; // second
    int brightness = 0;
    bool isBrighter = true;
    uint8_t lightTimer[6];
    int timerLen;
    digitalWrite(VCC_LED, HIGH);
    digitalWrite(GND_LED, LOW);
    while(true){
        if(isBrighter){
            brightness += 5;
            if(brightness >= 255){
                isBrighter = false;
            }
        }else{
            brightness -= 5;
            if(brightness <= 0){
                isBrighter = true;
            }
        }
        analogWrite(PWM_LED, brightness);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
    }
}

void setup(){
    Serial.begin(115200);
    // ----- PIN SETUP -----
    pinMode(FOOD_SENSOR, INPUT);
    pinMode(WATER_LEVEL_SENSOR, INPUT);
    pinMode(DC_MOTER_OUT, OUTPUT);
    pinMode(DC_MOTER_IN, OUTPUT);
    pinMode(VCC_LED, OUTPUT);
    pinMode(GND_LED, OUTPUT);
    pinMode(PWM_LED, OUTPUT);

    digitalWrite(DC_MOTER_OUT, HIGH);
    digitalWrite(DC_MOTER_IN, HIGH); 
    isFeeding = false;
    isWaterChanging = false;

    // ----- RTC -----
    rtc.writeProtect(false); // allow to write time
    rtc.halt(false); // false = let clock keep ticking

    // ----- NVS -----
    pref.begin("fishTank_ctrl", false); // read only = false
    // pref.clear();
    
    int schLen = pref.getBytesLength("feedTimer");
    uint8_t buffer[schLen];

    pref.getBytes("feedTimer", buffer, schLen);
    Serial.println(schLen);
    for (int x=0; x<schLen; x++) {
        Serial.print(buffer[x]);
        Serial.print(", ");
    }
    Serial.println(); 

    // ----- SERVO -----
    myservo.attach(SERVO);

    // ----- TEMPERATURE SENSOR -----
    tempSensor.begin();

    // ----- TASK -----
    xTaskCreatePinnedToCore(FoodSensor, "FoodSensor", 2048, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(TemperatureSensor, "TemperatureSensor", 2048, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(WaterChanging, "WaterChanging", 2048, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(LEDControl, "LEDControl", 2048, NULL, 0, NULL, 1);

    // ----- WIFI SETTING -----
    setupWiFiAP();

    // ----- WEB SERVER -----
    setupWebServer();
    ElegantOTA.begin(&webServer);
}

void loop(){
    // Time now = rtc.time();
    // Serial.println(String(now.yr)+"-"+String(now.mon)+"-"+String(now.date)+" "+
    // String(now.hr)+":"+String(now.min)+":"+String(now.sec));
    webServer.handleClient();
    ElegantOTA.loop();
}