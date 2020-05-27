#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>


ESP8266WebServer server(80);
bool isConfigurated = false;
bool WiFiOn = false;
unsigned long latchedTime  = 0;
String configData[2] = {"", ""};
std::vector<std::pair<String, String>> allPhoneNumbers; // wektor przechowujacy wszystkie numery

void refreshTimer(){ // odswiezenie licznika wylaczajacego WiFi
    latchedTime = millis();
    Serial.println("Refresh");
}

bool checkConfiguration(){ // sprawdza czy urzadzenie jest skonfigurowane
    SPIFFS.begin();
    return SPIFFS.exists("/contacts.txt") && SPIFFS.exists("/conf.txt");
}

void enableWiFi(){ // wlaczenie WiFi
    WiFi.mode(WIFI_AP); // WiFi w trybie Access Point
    WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0)); // parametry sieci rozglaszanej przez uC
    if(isConfigurated)
        WiFi.softAP(configData[0], configData[1], 1, 0, 1);
    else
        WiFi.softAP("Fall Detector", "smiw2020", 1, 0, 1);

    WiFiOn = true;
    
}

void saveConfiguration(){ // zapis konfiguracji 
    if(server.args()){ 
        StaticJsonDocument<256> input;
        deserializeJson(input, server.arg(0));
        String SSID = input["SSID"];
        String Password = input["Pass"];
        String PhoneName = input["PhoneName"];
        String PhoneNumber = input["PhoneNumber"];
        bool isNumber = true;
        for(unsigned int i = 0; i < PhoneNumber.length(); ++i){
            if(!isDigit(PhoneNumber[i])){
                isNumber = false;
                break;
            }
        }
            
        if(SSID == "" || SSID.length() > 31 || Password.length() < 8 || Password.length() > 63 || PhoneNumber.length() != 9 || !isNumber || PhoneName == "" || PhoneName.length() > 30){
            //refreshTimer();
            server.send(400);
            return;
        }
            

        SPIFFS.begin();
        File conf = SPIFFS.open("/conf.txt", "w"); // plik otwarty do pisania
        File phones = SPIFFS.open("/contacts.txt", "w");
        conf.println(SSID);
        conf.println(Password);
        phones.println(PhoneName + '\x03' + PhoneNumber);
        conf.close();
        phones.close();
        
        server.send(200); // wyslij ze wszystko ok
        delay(1000); // aby wyslac response
        ESP.restart();
    } 
    else server.send(400); // jesli cos nie tak
}

bool loadConfiguration(){ // wczytywanie konfiguracji
    String temp = "";
    int tempIndex = 0;

    SPIFFS.begin();
    File file = SPIFFS.open("/conf.txt", "r");
    size_t fileSize = file.size(); // musi to byc bo uzywajac samego file.size() nie dziala
    if(!file.size()) return false; // jesli pusty plik
    
    char* buf = new char[fileSize];
    file.readBytes(buf, fileSize); // wczytanie danych do bufora
    file.close();

    for(unsigned int i = 0; i < fileSize; ++i){ // petla zczytujaca wiersz
        if(buf[i] == '\r'){ // jak natrafi na powrot karetki
            configData[tempIndex] = temp; // koniec wiersza
            temp = ""; // czyszczenie tempa
            ++i; // jeszcze \n
            ++tempIndex;
        } 
        else temp += buf[i]; // klejenie wiersza
    }
    delete[] buf; // bez wyciekow pamieci

     // WiFi w trybie Access Point
    
    return true;
}

void deleteConfiguration(){ // usuniecie konfiguracji
    SPIFFS.begin();
    SPIFFS.remove("/contacts.txt"); 
    SPIFFS.remove("/conf.txt");
    ESP.restart();
}

void sendContacts(){
    String serialized;
    StaticJsonDocument<512> output; // jak nie bedzie numerow to to zwiekszyc
    
    for(unsigned int i = 0; i < allPhoneNumbers.size(); ++i){
        output[i]["Name"] = allPhoneNumbers[i].first;
        output[i]["Phone"] = allPhoneNumbers[i].second;
    }
    serializeJson(output, serialized);
    server.send(200, "application/json", serialized);
}

void loadContacts(){ // funkcja uzyta przy starcie uC do zaladowania listy numerow z pamieci
    String temp = "";
    String tempName = "";
    SPIFFS.begin();
    File file = SPIFFS.open("/contacts.txt", "r");
    size_t fileSize = file.size(); // musi to byc
    char* buf = new char[fileSize];
    file.readBytes(buf, fileSize);
    file.close();
    
    for(unsigned int i = 0; i < fileSize; ++i){
        if(buf[i] == '\x03'){
            tempName = temp;
            temp = ""; // czyszczenie tempa
        }
        else if(buf[i] == '\r'){ // jak natrafi na powrot karetki (po pawulon)
            allPhoneNumbers.emplace_back(tempName, temp);
            temp = ""; // czyszczenie tempa
            tempName = "";
            ++i; // jeszcze \n
        } 
        else 
            temp += buf[i]; // klejenie numeru
    }

    delete[] buf; // bez wyciekow
}

void saveContact(){ // zapis numeru telefonu do pamieci
    refreshTimer();
    if(server.args() && allPhoneNumbers.size() <= 5){
        StaticJsonDocument<128> input;
        deserializeJson(input, server.arg(0));
        String newName = input["newName"];
        String newPhone = input["newPhone"];
        if(newPhone.length() != 9 || newName.length() > 30 || newName == ""){
            server.send(400);
            return;
        }

        SPIFFS.begin();
        File file = SPIFFS.open("/contacts.txt", "a"); // plik otwarty do dopisywania
        file.println(newName + '\x03' + newPhone); // z tego pola jest numer
        file.close(); // zamykansko time
        allPhoneNumbers.emplace_back(newName, newPhone);
    }
    server.send(200);
}

void deleteContact(){
    refreshTimer();
    if(server.args()){ 
        StaticJsonDocument<32> input;
        deserializeJson(input, server.arg(0));
        String phone2delete = input["phone"];
        if(allPhoneNumbers.size() == 1){ 
            server.send(406);
            return;
        }
        
        SPIFFS.begin(); // montowanie
        File file = SPIFFS.open("/contacts.txt", "w"); // plik otwarty do pisania
        int index = 0;
        bool flag = false;

        for(unsigned int i = 0; i < allPhoneNumbers.size(); ++i){ // petla zapisujaca nieusuniete telefony
            if(phone2delete == allPhoneNumbers[i].second){ // jesli to ten numer co ma byc usuniety to
                index = i; // index do usuniecia z vectora
                flag = true;
            }
            else
                file.println(allPhoneNumbers[i].first + '\x03' + allPhoneNumbers[i].second); // zapis numeru do pliku
        }
        if(flag) allPhoneNumbers.erase(allPhoneNumbers.begin() + index); // usuniecie numeru z vectora , jesli flaga false to znaczy ze nie ma takiego numeru (nie powinno wystapic)

        file.close(); // zamykansko time
    }
    server.send(200);
}

void pageFile(String filename){
    refreshTimer();
    SPIFFS.begin();
    File file = SPIFFS.open("/" + filename, "r"); // czytanie pliku
    if(filename.endsWith("html"))
        server.streamFile(file, "text/html"); // wysylanie strony 
    else 
        server.streamFile(file, "image/png");
    file.close();
}

void startServer(){ // wlaczenie web serwera
    if(isConfigurated){
        server.on("/", [](){ pageFile("index.html"); });
        server.on("/trash.png", [](){ pageFile("trash.png"); });
        server.on("/listcontacts", sendContacts);
        server.on("/addcontact", saveContact);
        server.on("/deletecontact", deleteContact);
    }
    else{
        server.on("/", [](){ pageFile("configuration.html"); }); // glowna strona konfiguracji
        server.on("/saveconfiguration", saveConfiguration); // tutaj zbiera dane konfiguracyjne
    }
    server.begin();
}