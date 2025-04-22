// การต่อใช้งาน
// ขา TO คือ แสดงค่าอุณหภูมิ
// ขา DO แสดงค่าเมื่อ PH เกินค่าที่ตั้งใว้
// ขา PO แสดงค่า PH
// ขา V+ คื่อต่อไฟ +5V
// ขา G ต่อ Gruond หรือ ไฟ 0V
// การปรับ VR V1 คือการปรับ Offset ของค่า PH
// การปรับ VR V2 คือการปรับ ค่า Alarm คือ การแจ้งเตือน จะออก Pin D0

#include <Wire.h> 
#define pH_SENSE 25
#define temp_SENSE 26

void setup() {
  Serial.begin(115200);
  pinMode(pH_SENSE, INPUT);
  Serial.println("PH SENSOR MODULE"); 
} 

void loop(){
    int measuringVal = analogRead(pH_SENSE);
    Serial.print("Measuring Raw Value > ");
    Serial.println(measuringVal);
    double vltValue = 5/1024.0 * measuringVal;
    Serial.print("Voltage Value > ");
    Serial.print(vltValue, 3);
    float P0 = 7 + ((2.5 - vltValue) / 0.18);
    Serial.println("");
    Serial.print("pH Value > ");
    Serial.println(P0, 3);
    Serial.println("");
    delay(3000);
}