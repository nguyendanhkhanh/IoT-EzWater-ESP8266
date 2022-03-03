#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <NTPtimeESP.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <time.h>
#include "EEPROM.h"

/************************* WiFi Access Point *********************************/
//#define WLAN_SSID "khanh"
//#define WLAN_PASS "244466666"
//#define WLAN_SSID "Kiras"
//#define WLAN_PASS "244466666"
String WLAN_SSID;                        //string variable to store ssid
String WLAN_PASS; 
unsigned long rst_millis;
/************************* cloudmqtt Setup *********************************/

#define serveruri "driver.cloudmqtt.com"
#define port       18643
#define username  "cqbfckol"
#define password  "mpSkyZ4D1N6f"

/*************************pin Setup *********************************/
const int DHTpin = 4; //đọc data từ chân  gpio4
const int RL1pin = 14;
const int RL2pin = 12;
const int RL3pin = 13;
const int RL4pin = 15;

const int ON = HIGH;
const int OFF = LOW;

#define LENGTH(x) (strlen(x) + 1)   // length of char string
#define EEPROM_SIZE 200
#define PIN_BUTTON 0
#define PIN_LED 16
#define LED_ON() digitalWrite(PIN_LED, HIGH)
#define LED_OFF() digitalWrite(PIN_LED, LOW)
#define LED_TOGGLE() digitalWrite(PIN_LED, digitalRead(PIN_LED) ^ 0x01)
/*************************Do am dat Setup ******************************/
int value_soil, realvalue;
/*************************DHT Setup ************************************/
const int DHTtype = DHT11; //khai báo loại cảm biến
DHT dht(DHTpin, DHTtype);  //khởi tạo dht
float humidity;
float temperature;
unsigned long readTime;
unsigned long feedBackTime;
unsigned long alarmTime;
unsigned long confirmTime;
/*************************Instance Setup ************************************/
//tạo 1 client
WiFiClient myClient;
//**************************** Server NTP *********************************
NTPtime NTPch("vn.pool.ntp.org");
/*
   Cac truong co trong struct DateTime:
   struct strDateTime
  {
  byte hour;
  byte minute;
  byte second;
  int year;
  byte month;
  byte day;
  byte dayofWeek;
  boolean valid;
  };
*/
strDateTime dateTime;
byte nowHour = 0;     // Gio
byte nowMinute = 0;   // Phut
//byte nowSecond = 0; // Giay
/*
   int actualyear = dateTime.year;       // Nam
   byte actualMonth = dateTime.month;    // Thang
   byte actualday =dateTime.day;         // Ngay
   byte actualdayofWeek = dateTime.dayofWeek;
*/
byte onHour = 0;     // Gio On
byte onMinute = 0;   // Phut On
byte offHour = 0;     // Gio Off
byte offMinute = 0;   // Phut Off

/*************************** Sketch Code ************************************/
void callback(char *tp, byte *message, unsigned int length);
void sensorRead();
void reconnect();
void feedBack();
/*************************** Smartconfig ************************************/
Ticker ticker;

bool longPress()
{
  static int lastPress = 0;
  if (millis() - lastPress > 3000 && digitalRead(PIN_BUTTON) == 0) {
    return true;
  } else if (digitalRead(PIN_BUTTON) == 1) {
    lastPress = millis();
  }
  return false;
}
// nhap nhay LED
void tick()
{
  //toggle state
  int state = digitalRead(PIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(PIN_LED, !state);     // set pin to the opposite state
}

bool in_smartconfig = false;
void enter_smartconfig()
{
  if (in_smartconfig == false) {
    in_smartconfig = true;
    ticker.attach(0.1, tick);
    WiFi.beginSmartConfig();
  }
}

void exit_smart()
{
  ticker.detach();
  LED_ON();
  in_smartconfig = false;
}
/******************************************************************/
//*****************khởi tạo pubsubclient***************************
PubSubClient mqtt(serveruri, port, callback, myClient);

void setup()
{
  Serial.begin(9600);
  WiFi.disconnect();
  pinMode(PIN_BUTTON, INPUT);
  EEPROM.begin(EEPROM_SIZE);
  WLAN_SSID = readStringFromFlash(0); // Read SSID stored at address 0
  Serial.print("SSID = ");
  Serial.println(WLAN_SSID);
  WLAN_PASS = readStringFromFlash(40); // Read Password stored at address 40
  Serial.print("psss = ");
  Serial.println(WLAN_PASS);
  WiFi.begin(WLAN_SSID.c_str(), WLAN_PASS.c_str());
  delay(10000);
  /*
    //Serial.setDebugOutput(true);

    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUTTON, INPUT);
    ticker.attach(1, tick);
    //Serial.println("Setup done");
  */
  dht.begin(); //khởi động cảm biến
  pinMode(5, INPUT_PULLUP); //doc cam bien do am dat
  //  Connect to WiFi access point.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);


  WiFi.begin(WLAN_SSID, WLAN_PASS);
  delay(10000);
  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.beginSmartConfig();

    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("SmartConfig received.");

    //Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("WiFi Connected.");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // read the connected WiFi SSID and password
    WLAN_SSID = WiFi.SSID();
    WLAN_PASS = WiFi.psk();
    Serial.print("SSID:");
    Serial.println(WLAN_SSID);
    Serial.print("PSS:");
    Serial.println(WLAN_PASS);
    Serial.println("Store SSID & PSS in Flash");
    writeStringToFlash(WLAN_SSID.c_str(), 0); // storing ssid at address 0
    writeStringToFlash(WLAN_PASS.c_str(), 40); // storing pss at address 40
    WiFi.begin(WLAN_SSID.c_str(), WLAN_PASS.c_str());
    delay(5000);
  }
 
  // Get MacAddress and remove ":"
  String MacAddress = WiFi.macAddress();
  MacAddress.remove(2,1);
  MacAddress.remove(4,1);
  MacAddress.remove(6,1);
  MacAddress.remove(8,1);
  MacAddress.remove(10,1);
  Serial.print("ESP8266 Board MAC Address:  ");
  Serial.println(MacAddress);

  // kết nối với mqtt server
  while (1)
  {
    delay(500);
    if (mqtt.connect("ESP8266", username, password))
      break;
  }
  Serial.println("connected to MQTT server.....");

  //nhận dữ liệu có topic "ESPn" từ server
  mqtt.subscribe("ESPn/RL1");
  mqtt.subscribe("ESPn/RL2");
  mqtt.subscribe("ESPn/RL3");
  mqtt.subscribe("ESPn/RL4");
  mqtt.subscribe("APPgH1/RL1");
  mqtt.subscribe("APPgM1/RL1");
  mqtt.subscribe("APPgH2/RL1");
  mqtt.subscribe("APPgM2/RL1");

  //set mode
  pinMode(DHTpin, INPUT);
  pinMode(RL1pin, OUTPUT);
  pinMode(RL2pin, OUTPUT);
  pinMode(RL3pin, OUTPUT);
  pinMode(RL4pin, OUTPUT);

  //set bit first time
  digitalWrite(RL1pin, OFF);
  digitalWrite(RL2pin, OFF);
  digitalWrite(RL3pin, OFF);
  digitalWrite(RL4pin, OFF);
}

void loop()
{
  rst_millis = millis();
  while (digitalRead(PIN_BUTTON) == LOW)
  {
    // Wait till boot button is pressed 
  }
  // check the button press time if it is greater than 3sec clear wifi cred and restart ESP 
  if (millis() - rst_millis >= 3000)
  {
    Serial.println("Reseting the WiFi credentials");
    writeStringToFlash("", 0); // Reset the SSID
    writeStringToFlash("", 40); // Reset the Password
    Serial.println("Wifi credentials erased");
    Serial.println("Restarting the ESP");
    delay(500);
    ESP.restart();            // Restart ESP
  }

  if (WiFi.status() == WL_DISCONNECTED || !mqtt.connected())
  {
    reconnect();
  }

  //làm mqtt luôn sống
  mqtt.loop();

  //phản hồi trạng thái relay lên server
  if (mqtt.connected())
  {
    if (millis() > feedBackTime + 1000)
    {
      feedBack();
    }


    //check if 5 seconds has elapsed since the last time we read the sensors.
    if (millis() > readTime + 5000)
    {
      sensorRead();
    }

    if (millis() > alarmTime + 5000)
    {
      alarm();
    }
    /*if (millis() > confirmTime + 5000)
      {
      confirmAlarm();
      }
    */
  }

  //nhan thoi gian tu NTp server
  // Tham so dau tien la Time zone duoi dang floating point (7.0 la VN);
  // Tham so thu hai la DayLightSaving: 1 voi thoi gian; 2 la thoi gian US (o VN khong su dung)
  dateTime = NTPch.getNTPtime(7.0, 0);
  if (dateTime.valid) {
    nowHour = dateTime.hour;      // Gio
    nowMinute = dateTime.minute;  // Phut
    //actualsecond = dateTime.second;  // Giay
    //Serial.println(nowHour);
    //Serial.println(nowMinute);
    /*
      actualyear = dateTime.year;       // Nam
      actualMonth = dateTime.month;    // Thang
      actualday =dateTime.day;         // Ngay
      actualdayofWeek = dateTime.dayofWeek;
    */
  }

  //
}

//********************* hàm trả dữ liệu về *****************************
void callback(char *tp, byte *message, unsigned int length)
{
  //bien nhan gio Bat, Tat tu client
  //Hien tai neu khong cap nhat, day se la thoi gian hang ngay


  String topic(tp);
  String content = String((char *)message);
  content.remove(length);

  //điều khiển relay 1
  if (topic == "ESPn/RL1")
  {
    if (content == "1")
    {
      digitalWrite(RL1pin, ON);
      Serial.println("relay 1 ON");
    }
    if (content == "0")
    {
      digitalWrite(RL1pin, OFF);
      Serial.println("relay 1 OFF");
    }
  }

  //điều khiển relay 2
  if (topic == "ESPn/RL2")
  {
    if (content == "1")
    {
      digitalWrite(RL2pin, ON);
      Serial.println("relay 2 ON");
    }
    if (content == "0")
    {
      digitalWrite(RL2pin, OFF);
      Serial.println("relay 2 OFF");
    }
  }

  //điều khiển relay 3
  if (topic == "ESPn/RL3")
  {
    if (content == "1")
    {
      digitalWrite(RL3pin, ON);
      Serial.println("relay 3 ON");
    }
    if (content == "0")
    {
      digitalWrite(RL3pin, OFF);
      Serial.println("relay 3 OFF");
    }
  }

  //điều khiển relay 4
  if (topic == "ESPn/RL4")
  {
    if (content == "1")
    {
      digitalWrite(RL4pin, ON);
      Serial.println("relay 4 ON");
    }
    if (content == "0")
    {
      digitalWrite(RL4pin, OFF);
      Serial.println("relay 4 OFF");
    }
  }

  //Nhan topic hen gio bat
  if (topic == "APPgH1/RL1") {
    onHour = content.toInt();
    Serial.print("Set time ON is ");
    Serial.print(onHour);
    Serial.print("h");
  }
  if (topic == "APPgM1/RL1") {
    onMinute = content.toInt();
    Serial.print(onMinute);
    Serial.println("p");
  }
  if (topic == "APPgH2/RL1") {
    offHour = content.toInt();
    Serial.print("Set time OFF is ");
    Serial.print(offHour);
    Serial.print("h");
  }
  if (topic == "APPgM2/RL1") {
    offMinute = content.toInt();
    Serial.print(offMinute);
    Serial.println("p");
  }


}

//************************* hàm đọc giá trị sensor ***************
void sensorRead()
{
  readTime = millis();
  humidity = dht.readHumidity();       //đọc nhiệt độ
  temperature = dht.readTemperature(); // đọc độ ẩm

  /*//kiểm tra sensor có hoạt động??
    if (isnan(humidity) || isnan(temperature))
    {
    Serial.println("Failed to read from DHT sensor!");
    return;
    }
    // Serial.print(humidity);
    // Serial.println("%");
    // Serial.print(temperature);
    // Serial.println("độ c");
  */
  //Doc do am dat
  //kiểm tra sensor dat có hoạt động??
  if (isnan(analogRead(A0)))
  {
    Serial.println("Failed to read from Soil sensor!");
    return;
  }
  for (int i = 1; i <= 10; i++)
  {
    realvalue += analogRead(A0);
  }
  value_soil = realvalue / 10;
  realvalue = 0 ;
  int percent_soil = map (value_soil, 892, 1024, 0 , 100); //chuyen gia tri do am dat ve phan tram
  percent_soil = 100 - percent_soil;
  delay(2000);

  // Get MacAddress and remove ":"
  String MacAddress = WiFi.macAddress();
  MacAddress.remove(2,1);
  MacAddress.remove(4,1);
  MacAddress.remove(6,1);
  MacAddress.remove(8,1);
  MacAddress.remove(10,1);
  Serial.print("ESP8266 Board MAC Address:  ");
  Serial.println(MacAddress);

  // init mqttChannel
  String mqttMainChannel = "ESPs/enviroment/";
  String mqttChannel = mqttMainChannel + MacAddress;

  // convert data to JSON
  StaticJsonDocument<200> doc;
  JsonObject content  = doc.to<JsonObject>();
  doc["temperature"] = temperature;
  doc["humidity"]   = humidity;
  doc["soil_humidity"]   = percent_soil;

  char content_string[256];
  serializeJson(content, content_string);

  // push data to mqtt
  mqtt.publish(mqttChannel.c_str(), content_string);
}

//*********************** hàm reconnect ********************
void reconnect()
{
  // lặp đến khi kết nối lại
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Attempting connection...");
    WiFi.reconnect();
    mqtt.connect("ESP8266", username, password);
    delay(500);
    // chờ để kết nối lại
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("reconnected");
      return;
    }
    else
    {
      Serial.print("failed to connect WiFi!!");
      Serial.println(" try again in 5 seconds...");
      // chờ 5s
      delay(5000);
    }
  }

  while (!mqtt.connected())
  {
    Serial.println("Attempting connection...");
    mqtt.connect("ESP8266", username, password);
    delay(500);
    // chờ để kết nối lại
    if (mqtt.connected())
    {
      Serial.println("reconnected");
      mqtt.subscribe("ESPn/RL1");
      mqtt.subscribe("ESPn/RL2");
      mqtt.subscribe("ESPn/RL3");
      mqtt.subscribe("ESPn/RL4");
      mqtt.subscribe("APPgH1/RL1");
      mqtt.subscribe("APPgM1/RL1");
      mqtt.subscribe("APPgH2/RL1");
      mqtt.subscribe("APPgM2/RL1");
      return;
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // chờ 5s
      delay(5000);
    }
  }
}

void feedBack() {
  feedBackTime = millis();

  // Get MacAddress and remove ":"
  String MacAddress = WiFi.macAddress();
  MacAddress.remove(2,1);
  MacAddress.remove(4,1);
  MacAddress.remove(6,1);
  MacAddress.remove(8,1);
  MacAddress.remove(10,1);
  Serial.println(MacAddress);
  // init mqttChannel
  String mqttMainChannel = "ESPs/status/realtime/";
  String mqttChannel = mqttMainChannel + MacAddress;

  // convert data to JSON
  StaticJsonDocument<200> doc;
  JsonObject content_1  = doc.to<JsonObject>();
  doc["relay_id"] = 1;
  doc["status"]   = String(digitalRead(RL1pin));
  char content_string_1[256];
  serializeJson(content_1, content_string_1);
  Serial.println(content_string_1);

  JsonObject content_2  = doc.to<JsonObject>();
  doc["relay_id"] = 2;
  doc["status"]   = String(digitalRead(RL2pin));
  char content_string_2[256];
  serializeJson(content_2, content_string_2);

  JsonObject content_3  = doc.to<JsonObject>();  
  doc["relay_id"] = 3;
  doc["status"]   = String(digitalRead(RL3pin));;
  char content_string_3[256];
  serializeJson(content_3, content_string_3);

  JsonObject content_4  = doc.to<JsonObject>();
  doc["relay_id"] = 4;
  doc["status"]   = String(digitalRead(RL4pin));
  char content_string_4[256];
  serializeJson(content_4, content_string_4);

  // push data to mqtt
  mqtt.publish(mqttChannel.c_str(), content_string_1);
  mqtt.publish(mqttChannel.c_str(), content_string_2);
  mqtt.publish(mqttChannel.c_str(), content_string_3);
  mqtt.publish(mqttChannel.c_str(), content_string_4);

//  mqtt.publish("ESPg/RL1", String(digitalRead(RL1pin)).c_str());
//  mqtt.publish("ESPg/RL2", String(digitalRead(RL2pin)).c_str());
//  mqtt.publish("ESPg/RL3", String(digitalRead(RL3pin)).c_str());
//  mqtt.publish("ESPg/RL4", String(digitalRead(RL4pin)).c_str());
}
// cap nhat tgian bat tat khi reload

void confirmAlarm() {
  confirmTime = millis();
  mqtt.publish("ESPgH1/RL1", String(onHour).c_str());
  mqtt.publish("ESPgM1/RL1", String(onMinute).c_str());
  mqtt.publish("ESPgH2/RL1", String(offHour).c_str());
  mqtt.publish("ESPgM2/RL1", String(offMinute).c_str());
}



void alarm() {
  alarmTime = millis();
  if (nowHour == onHour && nowMinute == onMinute) {
    digitalWrite(RL1pin, ON);
    confirmAlarm();
    Serial.println("Time to ON relay 1");
    Serial.println("relay 1 ON");
  }
  if (nowHour == offHour && nowMinute == offMinute) {
    digitalWrite(RL1pin, OFF);
    Serial.println("Time to OFF relay 1");
    Serial.println("relay 1 OFF");
  }

}

void milliAlarm() {

}

void writeStringToFlash(const char* toStore, int startAddr) {
  int i = 0;
  for (; i < LENGTH(toStore); i++) {
    EEPROM.write(startAddr + i, toStore[i]);
  }
  EEPROM.write(startAddr + i, '\0');
  EEPROM.commit();
}


String readStringFromFlash(int startAddr) {
  char in[128]; // char array of size 128 for reading the stored data 
  int i = 0;
  for (; i < 128; i++) {
    in[i] = EEPROM.read(startAddr + i);
  }
  return String(in);
}
