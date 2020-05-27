#include <SoftwareSerial.h>

void ICACHE_RAM_ATTR sleepModeInterrupt(); // jesli w sleep modzie przyjada dane wlacza sie przerwanie

class A9G{
    uint8_t powerOnPin = 16;
    uint8_t powerOffPin = 15;
    uint8_t lowPowerPin = 2;
    uint8_t wakeUpPin = 13;
    SoftwareSerial* SS;

public:
    A9G(SoftwareSerial* obj){
        pinMode(powerOnPin, OUTPUT);//LOW LEVEL ACTIVE
        digitalWrite(powerOnPin, HIGH); 

        pinMode(powerOffPin, OUTPUT);//HIGH LEVEL ACTIVE
        digitalWrite(powerOffPin, LOW);

        pinMode(lowPowerPin, OUTPUT);//LOW LEVEL ACTIVE
        digitalWrite(lowPowerPin, HIGH);

        pinMode(wakeUpPin, INPUT);

        SS = obj;
        SS->begin(115200);
        Serial.begin(9600); // debug
    }
    
    String sendCommand(String command){ // metoda wysyla komende oraz zwraca odpowiedz
        String response = "";
        unsigned int timeout = 1000; // timeout w ms
        unsigned long latchedTime = millis(); // zatrzasniety czas, potrzebny do timeouta

        SS->print(command + "\r"); // wyslanie komendy

        while((latchedTime + timeout) > millis()){ // czekamy do timeoutu
            while(SS->available()) 
                response += (char)SS->read(); // odczytanie po bajcie wiadomosci
        }
        return response;        
    }

    void SendTextMessage(String message,String phoneNumber){ 
        //Serial.println(sendCommand("AT+CMGF=1")); // format wiadomosci SMS, 1 dla Text
        //Serial.println(sendCommand("AT+CMGS=\""+ phoneNumber + "\"")); // do kogo wysylamy
        sendCommand("AT+CMGF=1"); // format wiadomosci SMS, 1 dla Text
        sendCommand("AT+CMGS=\""+ phoneNumber + "\""); // do kogo wysylamy
        SS->print(message); // Tresc SMSa
        SS->write(26); // wyslanie wiadomosci
        
        while (SS->available() > 0) // az sie wszystko przesle (inne akcje mogly w tym momencie wejsc)
            Serial.write(SS->read());

}

    bool powerOn(){
        digitalWrite(powerOnPin, LOW); // Wedlug przykladu
        delay(3000);
        digitalWrite(powerOnPin, HIGH);
        delay(5000);

        if(sendCommand("AT").indexOf("OK") != -1) // jesli odpowiedz zawiera OK zwroc true
            return true;
        return false;
        
    }

    bool powerOff(){
        digitalWrite(powerOffPin, HIGH);
        delay(3000);
        digitalWrite(powerOffPin, LOW);
        delay(5000);

        if(sendCommand("AT").indexOf("OK") != -1) // jesli odpowiedz zawiera OK
            return true;
        return false;
    }

    bool lowPowerOn(){
        if(sendCommand("AT+SLEEP=1").indexOf("OK") != -1){ // jesli odpowiedz zawiera OK
          digitalWrite(lowPowerPin, LOW);
          attachInterrupt(wakeUpPin, sleepModeInterrupt, RISING);
          return true;
        }
        return false;
    }

    bool lowPowerOff(){
        digitalWrite(lowPowerPin, HIGH);
        if(sendCommand("AT+SLEEP=0").indexOf("OK") != -1) // jesli odpowiedz zaczyna sie od OK to zwroc true
            return true; 
        return false;
    }
};

SoftwareSerial SSobj(14, 12, false);
A9G GGM(&SSobj);

void ICACHE_RAM_ATTR sleepModeInterrupt() { // jesli w sleep modzie przyjada dane wlacza sie przerwanie
    GGM.lowPowerOff();
}