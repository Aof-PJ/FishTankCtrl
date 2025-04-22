#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

#define SOFTAP_WIFI_SSID  "FT-CTRL"
#define SOFTAP_WIFI_PASS  "<Your WiFi-Password>"

using namespace std;

WebServer webServer(80);

const char *index_html PROGMEM = R"(
<!DOCTYPE html>
    <head>
        <title>LED Control</title>
    </head>
    <body>
        <a href="/update">OTA UPDATE</a>
    </body>
</html>
)";

void setupWiFiAP(){
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SOFTAP_WIFI_SSID, SOFTAP_WIFI_PASS);

    String esp32IPAddress = WiFi.softAPIP().toString();

    Serial.println("Connect to ESP32 at \"" + String(SOFTAP_WIFI_SSID) + "\" IP: " + esp32IPAddress);
    Serial.println("http://" + esp32IPAddress);
}

void setupWebServer() {
    webServer.on("/", HTTP_GET, []() {
        webServer.send(200, "text/html", index_html);
    });

    webServer.begin();
}

void setup(){
    Serial.begin(115200);

    // ----- WIFI SETTING -----
    setupWiFiAP();

    // ----- WEB SERVER -----
    setupWebServer();
    ElegantOTA.begin(&webServer);
}

void loop(){
    webServer.handleClient();
    ElegantOTA.loop();
}