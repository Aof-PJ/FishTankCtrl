#include <Firebase_ESP_Client.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define API_KEY "<Web_API_Key>"
#define DATABASE_URL "<Realtime_DB_URL>"

FirebaseData fbdo;
FirebaseData sensor;
FirebaseData timeListener;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJsonArray arr;

String deviceID = "";
bool signupOK = false;

#include <DS1302.h>
#include <TimeLib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <ElegantOTA.h>
#include <ESPmDNS.h>
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
#define WATER_LEVEL_THRESHOLD 2100
#define DC_MOTER_OUT 5
#define DC_MOTER_IN 23

#define VCC_LED 18
#define GND_LED 19
#define PWM_LED 25

#define LED_ONBOARD 2

#define SOFTAP_WIFI_SSID  "FT-CTRL"
#define SOFTAP_WIFI_PASS  "<Your WiFi-Password>"
String APP_WIFI_SSID = "";
String APP_WIFI_PASS = "";
String HOSTNAME = "fishtankctrl";

using namespace std;

DS1302 rtc(RTC_PIN_CE, RTC_PIN_IO, RTC_PIN_SCLK); // setup rtc
WebServer webServer(80);
Servo myservo;
Preferences pref;

OneWire oneWire(TEMPERATURE_SENSOR);
DallasTemperature tempSensor(&oneWire);
SemaphoreHandle_t keyProcess;

const static Time::Day WeekDays[] = { // available 0-6
  Time::kSunday,
  Time::kMonday,
  Time::kTuesday,
  Time::kWednesday,
  Time::kThursday,
  Time::kFriday,
  Time::kSaturday,
};

float foodLvl;
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

static char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

void setupWiFiSTA(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASS);

    for(int i=0; i<3; i++){
        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
            if(i==2){
                Serial.println("Failed to connect to the Wi-Fi network, restarting...");
                delay(2000);
                pref.remove("APP_WIFI_SSID");
                pref.remove("APP_WIFI_PASS");
                ESP.restart();
            }else{
                Serial.println("Failed to connect to the Wi-Fi network, trying again...");
                delay(5000);
            }
        }else{
            break;
        }
    }

    if (!MDNS.begin(HOSTNAME.c_str())) {   // Set the hostname to "esp32.local"
        Serial.println("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
    }

    digitalWrite(LED_ONBOARD, HIGH);
    
    Serial.println("mDNS responder started");
    Serial.println("Wi-Fi connected, browse to http://" + WiFi.localIP().toString());
}

void feed(){
    if(!isFeeding){
        isFeeding = true;
        for(int i = 1; i <= FEED_TIMES; i++){
            myservo.write(180);
            delay(FOOD_SERVO_DELAY);
            if(i == FEED_TIMES){
                int foodAnalog = analogRead(FOOD_SENSOR);
                if(foodAnalog == 4095){
                    foodLvl = 0.0;
                }else{
                    foodLvl = 100 - (((float)foodLvl/4095.0) * 62.5); 
                }
            }
            myservo.write(0);
            delay(FOOD_SERVO_DELAY);
        }
        if (Firebase.RTDB.setFloat(&fbdo, "fishTank_ctrl/"+deviceID+"/Sensor/FoodLvl", foodLvl)){
            Serial.println("save Food Level" + String(foodLvl));
        }else{
            Serial.println("Level FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
        }
        
        Serial.flush();
        isFeeding = false;
    }
}

void changeWater(){
    if(!isWaterChanging){
        Serial.println("CHANGE!!!");
        isWaterChanging = true;
        int waterLvl = analogRead(WATER_LEVEL_SENSOR);
        int waterLvlTresh = waterLvl;
        Serial.println("START PUMP OUT");
        digitalWrite(DC_MOTER_OUT, LOW); // start pump out moter
        digitalWrite(DC_MOTER_IN, HIGH); // stop pump IN moter
        while(waterLvl > 1500){
            waterLvl = analogRead(WATER_LEVEL_SENSOR);
            Serial.println(waterLvl);
            delay(500);
        }
        digitalWrite(DC_MOTER_OUT, HIGH); // stop pump out moter
        digitalWrite(DC_MOTER_IN, LOW); // start pump IN moter
        Serial.println("START PUMP IN");
        while(waterLvl < waterLvlTresh){
            waterLvl = analogRead(WATER_LEVEL_SENSOR);
            Serial.println(waterLvl);
            delay(500);
        }
        digitalWrite(DC_MOTER_IN, HIGH); // stop pump IN moter
        isWaterChanging = false;
    }
    Serial.println("Changing water's done!!!");
}

const char *wifi_setup_html PROGMEM = R"(
<!DOCTYPE html>
    <head>
        <meta charset=\UTF-8\>
        <title>ESP32 Wi-Fi Setup</title>
    </head>
    <body>
        <h1>Wi-Fi Setup</h1>
        <form action="/" method="POST">
            <p>SSID : <input type="text" name="ssid" required></p>
            <p>Password : <input type="password" name="password" required></p>
            <input type="submit" value="save">
        </form>
        <a href="/setdevicename">set device name</a>
    </body>
    <script>
    </script>
</html>
)";

const char *set_device_name PROGMEM = R"(
<!DOCTYPE html>
    <head>
        <meta charset=\UTF-8\>
        <title>ESP32 Wi-Fi Setup</title>
    </head>
    <body>
        <h1>Device Setup</h1>
        <form action="/setdevicename" method="POST">
            <p>Device Name : <input type="text" name="device" required></p>
            <p>Password : <input type="password" name="password" required></p>
            <input type="submit" value="save">
        </form>
        <a href="/">Wi-Fi setting</a>
    </body>
    <script>
    </script>
</html>
)";

void setupWebServer() {
    webServer.on("/", HTTP_GET, []() {
        webServer.send(200, "text/html", wifi_setup_html);
    });

    webServer.on("/", HTTP_POST, []() {
        webServer.send(200, "text/html", wifi_setup_html);
        String ssid = webServer.arg("ssid");
        String password = webServer.arg("password");
        Serial.println(ssid + ", " + password);
        pref.putString("APP_WIFI_SSID", ssid);
        pref.putString("APP_WIFI_PASS", password);
        ESP.restart();
    });

    webServer.on("/setdevicename", HTTP_GET, []() {
        webServer.send(200, "text/html", set_device_name);
    });

    webServer.on("/setdevicename", HTTP_POST, []() {
        webServer.send(200, "text/html", set_device_name);
        String device = webServer.arg("device");
        String password = webServer.arg("password");
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            if (Firebase.ready() && signupOK){
                if (Firebase.RTDB.setString(&fbdo, "fishTank_ctrl/"+deviceID+"/deviceName", device)){
                    Serial.println("save Device Name");
                }else{
                    Serial.println("Device FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }
                if (Firebase.RTDB.setString(&fbdo, "fishTank_ctrl/"+deviceID+"/password", password)){
                    Serial.println("save password");
                }else{
                    Serial.println("password FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }

                // general data
                int changeRoutine = pref.getInt("waterChanging", 1); // day
                if (Firebase.RTDB.setInt(&fbdo, "fishTank_ctrl/"+deviceID+"/WaterChanging", changeRoutine)){
                    Serial.println("save water routine");
                }else{
                    Serial.println("Water FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }

                if (Firebase.RTDB.setFloat(&fbdo, "fishTank_ctrl/"+deviceID+"/Sensor/FoodLvl", foodLvl)){
                    Serial.println("save Food Level");
                }else{
                    Serial.println("Level FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }

                const char *light[6] = {"startHr", "startMin", "maxHr", "maxMin", "closeHr", "closeMin"};
                uint8_t ligthCycle[6]; 
                pref.getBytes("LightCycle", ligthCycle, 6);
                for(int i=0; i<6; i++){
                    if (Firebase.RTDB.setInt(&fbdo, "fishTank_ctrl/"+deviceID+"/LightCycle/"+String(light[i]), ligthCycle[i])){
                        Serial.println("save " + String(light[i]));
                    }else{
                        Serial.println("Light FAILED");
                        Serial.println("REASON: " + fbdo.errorReason());
                    }
                }
            }
        }
        xSemaphoreGive(keyProcess);
    });

    webServer.begin();
}

void FoodSensor(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int timeToSleep = 5; // second
    while(true){
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            // Serial.println("Start Food");
            // get data from RTC
            Time now = rtc.time();
            uint8_t hr_now = now.hr;
            uint8_t min_now = now.min;
            int timerLen;
            uint8_t feedTimer[0];

            if (Firebase.ready() && signupOK){
                if (Firebase.RTDB.getJSON(&fbdo, "/fishTank_ctrl/"+deviceID+"/FeedTimer")){
                    arr = fbdo.jsonArray();
                    timerLen = arr.size();
                    feedTimer[timerLen];
                    FirebaseJsonData tempE;
                    for (int i = 0; i < arr.size(); i++){
                        arr.get(tempE, i);
                        feedTimer[i] = tempE.to<uint8_t>();
                    }
                    // Serial.println("Got Data from DB");
                    pref.putBytes("feedTimer", feedTimer, timerLen);
                }else{
                    Serial.println("Food FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                    if(fbdo.errorReason() == "path not exist"){
                        pref.remove("feedTimer");
                    }
                }
            }else{
                timerLen = pref.getBytesLength("feedTimer");
                feedTimer[timerLen];
                pref.getBytes("feedTimer", feedTimer, timerLen);
            }

            for(int i=0; i<timerLen; i+=2){
                // Serial.println("Feed : " + String(feedTimer[i]) + ":" + String(feedTimer[i+1]));
                if((feedTimer[i] == hr_now) && (feedTimer[i+1] == min_now)){
                    feed();
                    timeToSleep = 60;
                    break;
                }else{
                    int difTime = (feedTimer[i]-hr_now)*3600 + (feedTimer[i+1]-min_now)*60; // difference from now & feed time(feed-now)
                    timeToSleep = 5;
                    
                    if(difTime >= 0){
                        Serial.println("next time to feed is " + String(feedTimer[i]) + ":" + String(feedTimer[i+1]));
                        break;
                    }
                }
            }

            // Serial.println("Sleep for " + String(timeToSleep) + " Second");
            // Serial.flush();
            xSemaphoreGive(keyProcess);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
        }
    }
}

void TemperatureSensor(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    while(true){
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            // Serial.println("Start Temp");
            tempSensor.requestTemperatures();
            temperatureValue = tempSensor.getTempCByIndex(0);
            
            if (Firebase.ready() && signupOK){
                if (Firebase.RTDB.setFloat(&fbdo, "fishTank_ctrl/"+deviceID+"/Sensor/Temperature", temperatureValue)){
                    Serial.println("Temp DONE");
                }else{
                    Serial.println("Temp FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }
            }
            xSemaphoreGive(keyProcess);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(30000));
        }
    }
}

void WaterChanging(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int stamp = dateTimeToSeconds(rtc.time());
    int changeRoutine;
    while(true){
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            // Serial.println("Start Water");
            int timeToSleep = 6*60*60; // 6 hours
            int now = dateTimeToSeconds(rtc.time());
            if(now < 0 || now <= 1000000000){
                timeToSleep = 10;
                xSemaphoreGive(keyProcess);
                vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
                continue;
            }
            if (Firebase.ready() && signupOK){
                if (Firebase.RTDB.getInt(&fbdo, "fishTank_ctrl/"+deviceID+"/WaterChanging")){
                    changeRoutine = fbdo.intData();
                    // Serial.println("Water DONE");
                    pref.putInt("waterChanging", changeRoutine); // day
                }else{
                    Serial.println("Water FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                    timeToSleep = 5; // if fail, read again in 5 sec
                    changeRoutine = 1;
                }
            }else{
                changeRoutine = pref.getInt("waterChanging", 1); // day
            }

            // Serial.println(String(now) +"-"+ String(stamp) + " = " + String(now - stamp));
            if(stamp > 1000000000){
                if((now - stamp) >= (changeRoutine*24*60*60)){ // compare id second
                    Serial.println(String(now) + ", " + String(stamp));
                    changeWater();
                    stamp = now;
                }
            }else{
                timeToSleep = 10;
                stamp = now;
            }

            xSemaphoreGive(keyProcess);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
        }
    }
}

void LEDControl(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int timeToSleep = 10; // second
    int brightness = 0;
    uint8_t lightTimer[6];
    int timerLen;
    digitalWrite(GND_LED, LOW);
    digitalWrite(VCC_LED, HIGH);
    while(true){
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            // Serial.println("Start LED");
            bool isDataValid = true;
            if (Firebase.ready() && signupOK){
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/startHr")){
                    lightTimer[0] = fbdo.intData();
                }else{isDataValid = false;}
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/startMin")){
                    lightTimer[1] = fbdo.intData();
                }else{isDataValid = false;}
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/maxHr")){
                    lightTimer[2] = fbdo.intData();
                }else{isDataValid = false;}
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/maxMin")){
                    lightTimer[3] = fbdo.intData();
                }else{isDataValid = false;}
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/closeHr")){
                    lightTimer[4] = fbdo.intData();
                }else{isDataValid = false;}
                if (Firebase.RTDB.getInt(&fbdo, "/fishTank_ctrl/"+deviceID+"/LightCycle/closeMin")){
                    lightTimer[5] = fbdo.intData();
                }else{isDataValid = false;}
                if(isDataValid && (lightTimer[0] != lightTimer[1] != lightTimer[2] != lightTimer[3] != lightTimer[4] != lightTimer[5])){ // if data valid
                    Serial.println("Data Valid");
                    pref.putBytes("LightCycle", lightTimer, 6);
                }else{
                    Serial.println("Data Invalid");
                    pref.getBytes("LightCycle", lightTimer, 6);
                }
            }else{
                pref.getBytes("LightCycle", lightTimer, 6);
            }

            Time now = rtc.time();
            float time_now = now.hr*3600 + now.min*60 + now.sec;
            float start_time = lightTimer[0]*3600 + lightTimer[1]*60;
            float max_time = lightTimer[2]*3600 + lightTimer[3]*60;
            float close_time = lightTimer[4]*3600 + lightTimer[5]*60;

            // Serial.println(String(start_time) + ", " + String(max_time) + ", " + String(close_time) + ", " + String(time_now));

            if(start_time <= time_now && time_now <= max_time){
                brightness = ((time_now-start_time)/(max_time-start_time)) * 255;
            }else if(max_time < time_now && time_now <= close_time){
                brightness = (1 - ((time_now-max_time)/(close_time-max_time))) * 255;
            }else{
                brightness = 0;
            }
            analogWrite(PWM_LED, brightness);
            Serial.println("Brightness : " + String(brightness));

            xSemaphoreGive(keyProcess);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToSleep * 1000));
        }
    }
}

void updateTime2DB(void *pvParameters){
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    int timeToSleep = 30; // second
    while(true){
        if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE) {
            if (Firebase.ready() && signupOK){ // reset do now in db
                Time now = rtc.time();
                string time = to_string(now.yr) + "-" +
                    ((now.mon < 10) ? "0" : "") + to_string(now.mon) + "-" +
                    ((now.date < 10) ? "0" : "") + to_string(now.date) + "T" +
                    ((now.hr < 10) ? "0" : "") + to_string(now.hr) + ":" +
                    ((now.min < 10) ? "0" : "") + to_string(now.min) + ":" +
                    ((now.sec < 10) ? "0" : "") + to_string(now.sec);

                if (Firebase.RTDB.setString(&fbdo, "fishTank_ctrl/"+deviceID+"/Time/Now", time)){
                    // Serial.println(time.c_str());
                }else{
                    Serial.println("NOW FAILED");
                    Serial.println("REASON: " + fbdo.errorReason());
                }
            }
        }
        Serial.flush();
        xSemaphoreGive(keyProcess);
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
    pinMode(LED_ONBOARD, OUTPUT);

    digitalWrite(LED_ONBOARD, LOW);
    digitalWrite(DC_MOTER_IN, HIGH); // stop pump IN moter
    digitalWrite(DC_MOTER_OUT, HIGH); // stop pump OUT moter

    isFeeding = false;
    isWaterChanging = false;

    // ----- RTC -----
    rtc.writeProtect(false); // allow to write time
    rtc.halt(false); // false = let clock keep ticking

    // ----- NVS -----
    pref.begin("fishTank_ctrl", false); // read only = false
    // pref.clear();

    // ----- SERVO -----
    myservo.attach(SERVO);

    // ----- TEMPERATURE SENSOR -----
    tempSensor.begin();

    // ----- WIFI SETTING -----
    APP_WIFI_SSID = pref.getString("APP_WIFI_SSID", "");
    APP_WIFI_PASS = pref.getString("APP_WIFI_PASS", "");
    Serial.println("SSID : " + APP_WIFI_SSID);
    Serial.println("PASS : " + APP_WIFI_PASS);

    if(APP_WIFI_SSID != "" && APP_WIFI_PASS != ""){
        setupWiFiSTA();

        // ----- DB API CONFIG -----
        /* Assign the api key (required) */
        config.api_key = API_KEY;

        /* Assign the RTDB URL (required) */
        config.database_url = DATABASE_URL;

        /* Sign up */
        if (Firebase.signUp(&config, &auth, "", "")){
            Serial.println("ok");
            signupOK = true;
        }
        else{
            Serial.printf("%s\n", config.signer.signupError.message.c_str());
        }
        /* Assign the callback function for the long running token generation task */
        config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
        
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);

        deviceID = pref.getString("DeviceID", "").c_str();
        Serial.println(deviceID);

        sensor.keepAlive(5, 5, 1);
        if (!Firebase.RTDB.beginStream(&sensor, "/fishTank_ctrl/"+String(deviceID)+"/ProcessNow"))
            Serial.printf("stream begin error, %s\n\n", sensor.errorReason().c_str());
        else
            Serial.println("Set sensor listener");

        timeListener.keepAlive(5, 5, 1);
        if (!Firebase.RTDB.beginStream(&timeListener, "/fishTank_ctrl/"+String(deviceID)+"/Time/LastUpdate/UpdateFlag"))
            Serial.printf("stream begin error, %s\n\n", timeListener.errorReason().c_str());
        else
            Serial.println("Set time listener");

        if (Firebase.ready() && signupOK){ // reset do now in db
            if (Firebase.RTDB.setBool(&fbdo, "fishTank_ctrl/"+deviceID+"/ProcessNow/FeedNow", false)){
                    Serial.println("FEED RESET");
            }else{
                Serial.println("RF FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
                ESP.restart();
            }

            if (Firebase.RTDB.setBool(&fbdo, "fishTank_ctrl/"+deviceID+"/ProcessNow/ChangeWaterNow", false)){
                Serial.println("CHANGE RESET");
            }else{
                Serial.println("RC FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
                ESP.restart();
            }
        }
    }else{
        setupWiFiAP();
    }

    // ----- WEB SERVER -----
    setupWebServer();
    ElegantOTA.begin(&webServer);

    // ----- TASK -----
    vSemaphoreCreateBinary(keyProcess);
    xTaskCreatePinnedToCore(FoodSensor, "FoodSensor", 8192, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(TemperatureSensor, "TemperatureSensor", 8192, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(WaterChanging, "WaterChanging", 8192, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(LEDControl, "LEDControl", 8192, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(updateTime2DB, "updateTime2DB", 8192, NULL, 0, NULL, 1);

    if(pref.getBool("isFirstTime", true)){
        // set default light cycle
        uint8_t lightTimer[6] = {6, 0, 12, 0, 19, 0};
        pref.putBytes("LightCycle", lightTimer, 6);
        pref.putBool("isFirstTime", false);
        char deviceID[17];
        rand_string(deviceID ,16);
        Serial.println(deviceID);
        pref.putString("DeviceID", deviceID);
    }
}

void loop(){
    webServer.handleClient();
    ElegantOTA.loop();

    // db listener
    if (Firebase.ready() && signupOK){
        if (!Firebase.RTDB.readStream(&sensor)){
            Serial.printf("sream read error, %s\n\n", sensor.errorReason().c_str());
            ESP.restart();
        }

        if (!Firebase.RTDB.readStream(&timeListener)){
            Serial.printf("sream read error, %s\n\n", timeListener.errorReason().c_str());
            ESP.restart();
        }

        if (sensor.streamAvailable()){
            if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE){
                string path = sensor.dataPath().c_str();
                bool status = string(sensor.stringData().c_str())=="true" ? true:false;
                if(path == "/FeedNow" && status){
                    feed();
                    if (Firebase.RTDB.setBool(&fbdo, "fishTank_ctrl/"+deviceID+"/ProcessNow/FeedNow", false)){
                        Serial.println("FEED DONE");
                    }else{
                        Serial.println("FEED FAILED");
                        Serial.println("REASON: " + fbdo.errorReason());
                    }
                }else if(path == "/ChangeWaterNow" && status){
                    changeWater();
                    if (Firebase.RTDB.setBool(&fbdo, "fishTank_ctrl/"+deviceID+"/ProcessNow/ChangeWaterNow", false)){
                        Serial.println("CHANGE DONE");
                    }else{
                        Serial.println("CHANGE FAILED");
                        Serial.println("REASON: " + fbdo.errorReason());
                    }
                }
            }
            xSemaphoreGive(keyProcess);
        }

        if (timeListener.streamAvailable()){ // get data from db to rtc
            bool status = string(timeListener.stringData().c_str())=="true" ? true:false;
            String timeBuffer;
            if(status == true){
                if (xSemaphoreTake(keyProcess, portMAX_DELAY) == pdTRUE){
                    if (Firebase.RTDB.setBool(&fbdo, "fishTank_ctrl/"+deviceID+"/Time/LastUpdate/UpdateFlag", false)){
                        Serial.println("FLAG DONE");
                        if (Firebase.RTDB.getString(&fbdo, "fishTank_ctrl/"+deviceID+"/Time/LastUpdate/Stamp")){
                            timeBuffer = fbdo.stringData();
                            Serial.println(timeBuffer.c_str());
                            // Serial.println(timeBuffer);
                            int year = timeBuffer.substring(0, 4).toInt();
                            int month = timeBuffer.substring(5, 7).toInt();
                            int date = timeBuffer.substring(8, 10).toInt();
                            int hour = timeBuffer.substring(11, 13).toInt();
                            int minute = timeBuffer.substring(14, 16).toInt();
                            int second = timeBuffer.substring(17, 19).toInt();
                            int day = timeBuffer.substring(20, 21).toInt();
                            Time now(year, month, date, hour, minute, second, WeekDays[day]);
                            rtc.time(now);
                        }else{
                            Serial.println("Stamp FAILED");
                            Serial.println("REASON: " + fbdo.errorReason());
                        }
                    }else{
                        Serial.println("CHANGE FAILED");
                        Serial.println("REASON: " + fbdo.errorReason());
                    }
                }
                xSemaphoreGive(keyProcess);
            }
        }
    }
}