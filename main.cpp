
#include <Arduino.h>
#include <Adafruit_MAX31865.h>
#include <Wire.h>
#include "AIS_SIM7020E_API.h"
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <http_ota.h>



LiquidCrystal_I2C lcd(0x27, 20, 4);
AIS_SIM7020E_API nb;

// WiFi&OTA 
String HOSTNAME = "";
#define PASSWORD "green7650" // the password for OTA upgrade, can set it in any char you want
int beforprogressbar;

const char* ssid = "greenio";
const char* password = "green7650";

// MQTT DATA
// String address = "20.195.50.89"; // Your IPaddress or mqtt server url        Test url >>> http://www.hivemq.com/demos/websocket-client/
String address = "mqtt://mqttdtmspeacom.southeastasia.azurecontainer.io"; // Your IPaddress or mqtt server url        Test url >>> http://www.hivemq.com/demos/websocket-client/
String serverPort = "1883";           // Your server port
String clientID = "JessyMSI";         // Your client id < 120 characters
String topic = "TestESP";             // Your topic     < 128 characters
String Json = "";                  // Your payload   < 500 characters
String usernameMQTT= "dtms";                 // username for mqtt server, username <= 100 characters
String passwordMQTT = "peaadmin";                 // password for mqtt server, password <= 100 characters
unsigned int subQoS = 0;
unsigned int pubQoS = 0;
unsigned int pubRetained = 0;
unsigned int pubDuplicate = 0;

const long SentMQTT_time  = 5*1000, ReadSensor_time = 1000, OTA_time = 2*60*1000; // time in millisecond
unsigned long previousSentMQTT_time = 0, previousReadSensor_time = 0, previousOTA_time = 0;

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF 430.0
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL 100.0

Adafruit_MAX31865 thermo1 = Adafruit_MAX31865(33);
Adafruit_MAX31865 thermo2 = Adafruit_MAX31865(32);

float input_voltage = 0.0;
float tmp = 0.0;
float r1 = 1000.0;
float r2 = 2000.0;

float Temp1 = 0, Temp2 = 0;

byte C_dregree[8] = { // องศาC
    0b11000,
    0b11000,
    0b00000,
    0b00111,
    0b01000,
    0b01000,
    0b01000,
    0b00111};

void callback(String &topic, String &payload, String &QoS, String &retained)
{
  Serial.println("-------------------------------");
  Serial.println("# Message from Topic \"" + topic + "\" : " + nb.toString(payload));
  Serial.println("# QoS = " + QoS);
  if (retained.indexOf(F("1")) != -1)
  {
    Serial.println("# Retained = " + retained);
  }
}


void setupOTA()
{
  ArduinoOTA.setHostname(HOSTNAME.c_str());
  ArduinoOTA.setPassword(PASSWORD);
  ArduinoOTA.onStart([]()
                     {
                       Serial.println("Start Updating....");
                       Serial.printf("Start Updating....Type:%s\n", (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem"); 
                       lcd.clear();
                       lcd.setCursor(2, 1);
                       lcd.print("---Updateting---"); });
  ArduinoOTA.onEnd([]()
                   {
    Serial.println("Update Complete!");
    lcd.clear();
    lcd.setCursor(2,1);
    lcd.print("Update Complete!"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    String pro = String(progress / (total / 100)) + "%";
    int progressbar = (progress / (total / 100));
    Serial.print("Progress : ");
    Serial.println((progress / (total / 100)));
  if(beforprogressbar != progressbar) {
    lcd.setCursor(2,2);
    lcd.printf("Progress : %d %%",progressbar); 
 }
beforprogressbar = progressbar; });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    String info = "Error Info:";
    switch (error)
    {
      case OTA_AUTH_ERROR:
        info += "Auth Failed";
        Serial.println("Auth Failed");
        break;

      case OTA_BEGIN_ERROR:
        info += "Begin Failed";
        Serial.println("Begin Failed");
        break;

      case OTA_CONNECT_ERROR:
        info += "Connect Failed";
        Serial.println("Connect Failed");
        break;

      case OTA_RECEIVE_ERROR:
        info += "Receive Failed";
        Serial.println("Receive Failed");
        break;

      case OTA_END_ERROR:
        info += "End Failed";
        Serial.println("End Failed");
        break;
    }
    Serial.println(info);
    ESP.restart(); });
  ArduinoOTA.begin();
}

void connect_wifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.reconnect();
  }
  else
  {
    Serial.println("Connected to WiFi");
    // OTA_git_CALL();
  }
}


//=========== MQTT Function ================
void setupMQTT()
{
  if (!nb.connectMQTT(address, serverPort, clientID, usernameMQTT, passwordMQTT))
  {
    Serial.println("\nReconconnectMQTT");
  }
  else
  {
    Serial.println("\n MQTT Sucess");
  }
}

void connectStatus()
{
  if (!nb.MQTTstatus())
  {
    if (!nb.NBstatus())
    {
      Serial.println("reconnectNB ");
      nb.begin();
    }
    Serial.println("reconnectMQ ");
    setupMQTT();
  }
}

void setup()
{

  Project = "PEA_SIM7020E_PT100_POWERMETER_LCD2004_MQTT";
  FirmwareVer = "0.0";

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  connect_wifi();
  setupOTA();

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, C_dregree);
  
  // กำหนดค่าโมดูล SIM7020E
  nb.begin();
  nb.connectMQTT(address, serverPort, clientID, usernameMQTT, passwordMQTT);
  nb.setCallback(callback);
}

void Sent_MQTT()
{
  Json = "";
  Json.concat("{\"VBAT\":");
  Json.concat(input_voltage);
  Json.concat(",\"Temp1\":");
  Json.concat(String(Temp1, 1));
  Json.concat(",\"Temp2\":");
  Json.concat(String(Temp2, 1));
  Json.concat(",\"ver\":");
  Json.concat(FirmwareVer);
  Json.concat("}");
  nb.publish(topic, Json);
}

void ReadPT100()
{
  thermo1.begin(MAX31865_2WIRE); // set to 2WIRE or 4WIRE as necessary
  Temp1 = thermo1.temperature(RNOMINAL, RREF);
  Serial.printf("Temperature 1 = %.1f \n", Temp1);

  thermo2.begin(MAX31865_2WIRE);
  Temp2 = thermo2.temperature(RNOMINAL, RREF);
  Serial.printf("Temperature 2 = %.1f \n", Temp2);
}

void ReadBat()
{
  int analog_value = analogRead(34);
  tmp = (analog_value * 3.3) / 4096.0;
  input_voltage = tmp / (r2 / (r1 + r2));
  if (input_voltage < 0.1)
  {
    input_voltage = 0.0;
  }
}

void LCD_DISPLAY()
{
  lcd.clear();
  lcd.setCursor(2, 0);lcd.printf("PEA MQTT STATION");
  lcd.setCursor(0, 1);lcd.printf("Temp1:%.1f ", Temp1);lcd.write((uint8_t)0);
  lcd.setCursor(0, 2);lcd.printf("Temp2:%.1f ", Temp2);lcd.write((uint8_t)0);
  lcd.setCursor(0, 3);lcd.printf("V_Bat:%.1f v. ", input_voltage);
  lcd.setCursor(13, 3);lcd.printf("Ver:%s",FirmwareVer);
}

void loop()
{
  unsigned long current_time = millis();

  if (current_time - previousSentMQTT_time >= SentMQTT_time)    // ส่งข้อมูล MQTT ขึ้น Hive Broker
  {
    connectStatus();
    Sent_MQTT();
    previousSentMQTT_time = current_time;
  }

  if (current_time - previousReadSensor_time >= ReadSensor_time)
  {
    ReadPT100();
    ReadBat();
    LCD_DISPLAY();
    previousReadSensor_time = current_time;
  }

  if (current_time - previousOTA_time >= OTA_time)
  {
     connect_wifi();
     OTA_git_CALL();
     previousOTA_time = current_time;
  }

  nb.MQTTresponse();
  ArduinoOTA.handle();
}


