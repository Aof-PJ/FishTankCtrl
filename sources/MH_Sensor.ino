#define DATA_PIN 26

void setup() {
    Serial.begin(115200);
    pinMode(DATA_PIN, INPUT);
}
 
void loop() {
    int val = analogRead(DATA_PIN);
    Serial.print("val = ");
    Serial.println(val);
    delay(100);
}