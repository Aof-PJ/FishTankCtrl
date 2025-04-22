// #include <ESP32PWM.h>
#include <ESP32Servo.h>

const int servo = 26;
Servo myservo;

void setup(){
    Serial.begin(115200);
    myservo.attach(servo);
}

int angle = 0;

void loop(){
    angle = 180;
    myservo.write(angle);
    delay(500);
    angle = 0;
    myservo.write(angle);
    delay(10000);
}