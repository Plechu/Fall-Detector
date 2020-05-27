#include <arduino.h>
#include <Wire.h>
#include <ADXL345.h>
#include "a9g.h"
#include "web.h"

#define minute 60000

class VectorClass{
public:
    float x;
    float y;
    float z;
    float module;

    VectorClass(float x, float y, float z){ this->x = x; this->y = y; this->z = z; this->module = sqrt(x*x + y*y + z*z); }
    VectorClass makeVersor(){ return VectorClass(x/module, y/module, z/module); }
    void setValues(float x, float y, float z){ this->x = x; this->y = y; this->z = z; this->module = sqrt(x*x + y*y + z*z); }
    double calculateAngle(VectorClass outter){ 
        VectorClass inner = this->makeVersor(); // robiac wersory dlugosc wektora jest rowna 1 czyli nie trzeba mianownika
        outter = outter.makeVersor();
        return 57.2957795 * acos(inner.x*outter.x + inner.y*outter.y + inner.z*outter.z); // konwersja na stopnie
    }
};

unsigned int timeOut = 2 * minute;
bool buttonFlag = false;
VectorClass actual(0,0,0);
VectorClass refrence(-1,0,0); // pozycja stojaca
ADXL345 accel(ADXL345_STD);

  float impactTh = 2;
  float angleTh = 65;
  float timeTh = 2;
  float maximum = 0;

void ICACHE_RAM_ATTR handleButton()
{ // przerwanie wywolane przez nacisniecie przycisku pomiaru
  detachInterrupt(digitalPinToInterrupt(0)); // wylaczenie przerwania
  latchedTime = millis(); // timer
  buttonFlag = true;      // informacja czy nacisnieto przycisk
}

void setup(){
  Serial.begin(9600);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

  isConfigurated = checkConfiguration();
  if (isConfigurated){
    Serial.println("Skonfigurowany");
    WiFi.softAPdisconnect(); // wylaczenie AP
    WiFi.mode(WIFI_STA);     // lepiej to zrobic

    pinMode(0, INPUT_PULLUP); // pullup dla przycisku lewego
    loadConfiguration();
    loadContacts();
    attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);

  Wire.begin();

  byte deviceID = accel.readDeviceID();
  if (deviceID != 0) {
    Serial.print("0x");
    Serial.print(deviceID, HEX);
    Serial.println("");

    Serial.println("After 2s A9G will POWER ON.");
    delay(2000);
    if(GGM.powerOn())
       Serial.println("A9G POWER ON.");
    else 
      Serial.println("A9G Failed");
  } else {
    Serial.println("read device id: failed");
    while(1) {
      delay(100);
    }
  }

  if (!accel.writeRate(ADXL345_RATE_100HZ)) {
    Serial.println("write rate: failed");
    while(1) {
      delay(100);
    }
  }

  if (!accel.writeRange(ADXL345_RANGE_16G)) {
    Serial.println("write range: failed");
    while(1) {
      delay(100);
    }
  }

  if (!accel.start()) {
    Serial.println("start: failed");
    while(1) {
      delay(100);
    }
  }

  }
  else{
  Serial.println("Nie skonfigurowany");
  enableWiFi();
}
startServer();
}

void loop(){
  if (isConfigurated){
    if (millis() - latchedTime > 250 && buttonFlag && digitalRead(0) == HIGH)
    {
      enableWiFi();
      buttonFlag = false;
      Serial.println("klik");
    }

    if (millis() - latchedTime > 5000 && buttonFlag && !WiFiOn && digitalRead(0) == LOW){
      Serial.println("Reset");
      deleteConfiguration();
    }

    if (millis() - latchedTime > timeOut && WiFiOn)
    {                          // wlaczenie przycisku po timeoucie
      WiFi.softAPdisconnect(); // wylaczenie AP
      WiFi.mode(WIFI_STA);     // lepiej to zrobic
      WiFiOn = false;
      attachInterrupt(digitalPinToInterrupt(0), handleButton, FALLING);
    }

    if (accel.update()){
      actual.setValues(accel.getX(), accel.getY(), accel.getZ());
      if(actual.module > maximum){ 
      maximum = actual.module;
      Serial.println(maximum);
      }
  }else{
    Serial.println("dead");
  }
    
    if(actual.module > impactTh){ // algorytm
        Serial.println("Gleba?");
        delay(timeTh*1000);
        if(actual.calculateAngle(refrence) > angleTh){
          Serial.println("Upadek");
          for(unsigned int i = 0; i < allPhoneNumbers.size(); ++i){
            GGM.SendTextMessage("Upadek", "+48" + allPhoneNumbers[i].second);
            delay(5000);
          }
          delay(2000);
        } 
    }
  }
  if (WiFiOn)
    server.handleClient();
}
