/*
 * buil 11/8/2018
 * 
 * 
 * 
 */

 //-----------set firebase----------------------
#include <FirebaseArduino.h>
// Config Firebase
#define FIREBASE_HOST "waterlevel-5a226.firebaseio.com"
#define FIREBASE_AUTH "kjdlCZzDRt99gLbgeTypBSCU9lkegS7ZmNHbklMx"


#include <FS.h>                   //this needs to be first, or it all crashes and burns...
//#include <Ticker.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
//#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

ESP8266WiFiMulti WiFiMulti;
HTTPClient http;

//Warter
#include <NewPing.h>
#define TRIGGER_1_PIN  0  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_1_PIN     2
#define MAX_DISTANCE 400

NewPing sonar(TRIGGER_1_PIN, ECHO_1_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

//Line
#define LINE_TOKEN "nuXF48SEms2-Ru8xXDIfN"
#define Event "WL-0001"


//-----------Parameter Level--------------
char cnotify[4];
char cmin[4];
char cmax[4];

String nodename = "SF-Water";
String Devicename = "WL-0001";
String Min;
String Max;
String Level;
String Notify = "\u0E23\u0E30\u0E1A\u0E1A\u0E17\u0E33\u0E07\u0E32\u0E19\u0E1B\u0E01\u0E15\u0E34";
int value = 0;
int valueMax = 0;
int valueMin = 0;
int valueLevel = 0;
int LastLevel = 0;

const int PinConf = 3; //Rx Pin Config
const int led = 1; //Tx Pin led

boolean key = false;
bool shouldSaveConfig = false; //flag for saving data
boolean count2 = false,count3 = false;//flag for send line

long CountSend=1*30*1000;   //every 1 Minute
long CountSend1=10*60*1000;
long CountNotify=2*60*60*1000; //every 2 hour notify to framer
unsigned long CurT=0,CurT2H=0,CurT1,PrevT=0,PrevT2H=0,PrevT1=0;//time senddata

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PinConf, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  //digitalWrite(led, HIGH);
  delay(10);

  //-------------First config-----------------------------
    key=digitalRead(PinConf);    //Clear config for initial new connecting Wifi
    if(key==LOW) {
      firstConfig();
      Serial.print(F("Press key "));Serial.println(key);
      ESP.restart();
    }
    if (SPIFFS.begin()) {           //Check First config for connecting WiFi
      Serial.println(F("mounted file system"));
      if (!SPIFFS.exists("/config.json")) {
        firstConfig();
      }
    }
    //----------if first config is set ------------------------
    loadWiFiConfig();   //Load WiFi config from flash
    WiFi.begin();
    int con=0;
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(led, HIGH);
      delay(500);
      digitalWrite(led, LOW);
      Serial.print(".");
      if (con>=120) { break; }
      con++;
    }
    WiFi.setAutoReconnect(true);
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
  digitalWrite(led, HIGH); delay(200);
  digitalWrite(led, LOW); delay(200);
  digitalWrite(led, HIGH); delay(200);
  digitalWrite(led, LOW); delay(200);

  Line("ESP Ready...!");
  delay(10);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);       
  //digitalWrite(led, LOW);              
  // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
//  Serial.print("Ping: ");
//  Serial.print(sonar.ping_cm()); // Send ping, get distance in cm and print result (0 = outside set distance range)
//  Serial.println("cm");
  value = getSonar();
  if(value == valueLevel){
    digitalWrite(led, HIGH);
    Serial.print("Water Level Under"); Serial.println(valueLevel);
    if(count2 == false){
      Line("Water Level Under" + String(valueLevel)+"%");
      count2 = true;
    }
    delay(50);
    digitalWrite(led, LOW);
  }
  if(value == 0) {
    Serial.print("No Water");
    digitalWrite(led, HIGH);
    if(count3 == false){
      Line("No Water");
      count3 = true;
    }
  }
  if(value != valueLevel || value != 0) {
    digitalWrite(led, LOW);
    if(count2 == true) count2 = false;
    if(count3 == true) count3 = false;
  }
  
//---------- Send data logger every 5 min ---------------------
  CurT=millis();
  if ((CurT - PrevT) >= CountSend){
    PrevT=CurT;
    if(value != LastLevel){
      Firebase.set(Devicename + "/Water_Level",value);
      Serial.println("Send Data success!");
      LastLevel = value;
      digitalWrite(led, HIGH); delay(200);
      digitalWrite(led, LOW); delay(200);
      digitalWrite(led, HIGH); delay(200);
      digitalWrite(led, LOW); delay(200);
    }
    else Serial.println("Not Send Data success!");
  }
//----------------แจ้งเตือนทุกๆ 10 นาที------------------
  CurT1=millis();
  if ((CurT1 - PrevT1) >= CountSend1){
    PrevT1=CurT1;
    if(value == 0){
      digitalWrite(led, HIGH);
      Line("No Water");
    }
  }
//----------------แจ้งเตือนทุกๆ 2 ชั่วโมง------------------
  CurT2H=millis();
  if ((CurT2H - PrevT2H) >= CountNotify){
    PrevT2H=CurT2H;
    Line(Notify);
    //Line2("Code : 0x00"); //code 00 อุปกรณ์ทำงานปกติ
  }

  
}

void firstConfig() {  //Config Connecting WiFi ,Nodename, Token, LINE Token and state status=false
  WiFiManager wifiManager;
  
      wifiManager.resetSettings();
      //--Clear config--------------
      shouldSaveConfig = false;
      strcpy(cmin, "");
      strcpy(cmax, "");
      strcpy(cnotify, "");  
      //----------------------------

      WiFiManagerParameter custom_min("cmin", "Minimum", cmin, 4);
      WiFiManagerParameter custom_max("cmax", "Maximum", cmax, 4);
      WiFiManagerParameter custom_notify("cnotify", "Level", cnotify, 4);
   
      //set config save notify callback
      wifiManager.setSaveConfigCallback(saveConfigCallback);
    
      //add all your parameters here
      wifiManager.addParameter(&custom_min);
      wifiManager.addParameter(&custom_max);
      wifiManager.addParameter(&custom_notify);
                
      //set minimu quality of signal so it ignores AP's under that quality
      //defaults to 8%
      wifiManager.setMinimumSignalQuality();
      //wifiManager.startConfigPortal(ssidname, password);
      wifiManager.autoConnect("SF-Water");

      //if you get here you have connected to the WiFi
      //Serial.println("connected...yeey :)");
    
      //read updated parameters
      strcpy(cmin, custom_min.getValue());
      strcpy(cmax, custom_max.getValue());
      strcpy(cnotify, custom_notify.getValue());      
      //save the custom parameters to FS
      if (shouldSaveConfig) {
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
        json["cmin"] = cmin;
        json["cmax"] = cmax;
        json["cnotify"] = cnotify;
        
        if (SPIFFS.begin()) {
         File configFile = SPIFFS.open("/config.json", "w");
         if (!configFile) {
           Serial.println(F("failed to open config file for writing"));
         }
    
         json.prettyPrintTo(Serial);
         json.printTo(configFile);
         configFile.close();
        //end save
        }
      }
      //------------Save successSelect------------
      //writeStateStatus(false);
      //-------end save successSelect---------------
 //}
}

void loadWiFiConfig() {   //--Load WiFi parameter and Nodename, token, LINE Token
  if (SPIFFS.begin()) {
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      //Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          //Serial.println("\nparsed json");
          
          strcpy(cmin, json["cmin"]);
          strcpy(cmax, json["cmax"]);
          strcpy(cnotify, json["cnotify"]);
                    
          Min=cmax;
          Max=cmin;
          Level=cnotify;

          valueMax = Min.toInt();
          valueMin = Max.toInt();
          valueLevel = Level.toInt();
          
        } else {
          Serial.println(F("failed to load json config"));
        }
      }
      configFile.close();
    }
  } else {
    Serial.println(F("failed to mount FS"));
  }
}

int getSonar(){
  int value = 0;
  int valueAV = 0;
  int level;
  int count = 0;
  for(int i = 0;i<20;i++){
    value += sonar.ping_cm();
    count += 1;
    delay(50);
  }
  valueAV = value/count;
  if(valueAV > valueMax) valueAV = valueMax;
  if(valueAV < valueMin) valueAV = valueMin;
  Serial.print("Ping: ");
  Serial.print(valueAV); // Send ping, get distance in cm and print result (0 = outside set distance range)
  Serial.println("cm");
  level = map(valueAV,valueMax,valueMin,0,100);
  Serial.print("Level = ");Serial.print(level);Serial.println("%");
  return level;
}
void Line(String msg){
  delay(50);
  http.begin("http://maker.ifttt.com/trigger/" + String(Event) + "/with/key/" + String(LINE_TOKEN)); //HTTP
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST("{\"value1\":\"" + msg + "\"}");
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
    http.end();
}
