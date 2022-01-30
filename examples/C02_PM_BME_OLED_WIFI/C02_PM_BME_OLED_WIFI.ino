/*
This is the code for the AirGradient DIY Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

For build instructions please visit https://www.airgradient.com/diy/

Compatible with the following sensors:
Plantower PMS5003 (Fine Particle Sensor)
SenseAir S8 (CO2 Sensor)
SHT30/31 (Temperature/Humidity Sensor)

Please install ESP8266 board manager (tested with version 3.0.0)

The codes needs the following libraries installed:
"WifiManager by tzapu, tablatronix" tested with Version 2.0.3-alpha
"ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg" tested with Version 4.1.0

If you have any questions please visit our forum at https://forum.airgradient.com/

Configuration:
Please set in the code below which sensor you are using and if you want to connect it to WiFi.
You can also switch PM2.5 from ug/m3 to US AQI and Celcius to Fahrenheit

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/schools/

Kits with all required components are available at https://www.airgradient.com/diyshop/

MIT License
*/

#include <AirGradient.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "SSD1306Wire.h"
#include "BlueDot_BME280.h"

AirGradient ag = AirGradient();
SSD1306Wire display(0x3c, SDA, SCL);
BlueDot_BME280 bme1; 

// set sensors that you do not use to false
boolean hasPM = true;
boolean hasCO2 = true;
boolean hasSHT = false;
boolean hasBME = true;

// set to true to switch PM2.5 from ug/m3 to US AQI
boolean inUSaqi = true;

// set to true to switch from Celcius to Fahrenheit
boolean inF = true;

// set to true if you want to connect to wifi. The display will show values only when the sensor has wifi connection
boolean connectWIFI = true;

// change if you want to send the data to another server
String APIROOT = "http://10.0.1.125:8186/api/v2/write";

int pressureTrend[] = {0,0,0,0,0};
int currentTrend = 0;
boolean initTrend = true;

void setup() {
  Serial.begin(9600);

  if (hasBME) {
    bme1.parameter.communication = 0; 
    bme1.parameter.I2CAddress = 0x76;  
    bme1.parameter.sensorMode = 0b11;
    bme1.parameter.IIRfilter = 0b100;
    bme1.parameter.humidOversampling = 0b101;
    bme1.parameter.tempOversampling = 0b101;
    bme1.parameter.pressOversampling = 0b101;
    bme1.parameter.pressureSeaLevel = 1013.25; 
    bme1.parameter.tempOutsideCelsius = 15; 
    bme1.parameter.tempOutsideFahrenheit = 59;

    if (bme1.init() != 0x60)
    {    
      Serial.println(F("BME280 Sensor not found!"));
      hasBME = false;
    }
  };

  display.init();
  display.flipScreenVertically();
  showTextRectangle("Init", String(ESP.getChipId(), HEX), true);

  if (hasPM) ag.PMS_Init();
  if (hasCO2) ag.CO2_Init();
  if (hasSHT) ag.TMP_RH_Init(0x44);

  if (connectWIFI) connectToWifi();
  delay(2000);
}

void loop() {

  // create payload
  String payload = "airGradient,id=" + String(ESP.getChipId(), HEX) + " wifi=" + String(WiFi.RSSI()) + ",";

  if (hasPM) {
    int PM2 = ag.getPM2_Raw();
    payload = payload + "pm02=" + String(PM2);

    if (inUSaqi) {
      showTextRectangle("AQI", String(PM_TO_AQI_US(PM2)), false);
    } else {
      showTextRectangle("PM2", String(PM2), false);
    }

    delay(3000);

  }

  if (hasCO2) {
    if (hasPM) payload = payload + ",";
    int CO2 = ag.getCO2_Raw();
    payload = payload + "rco2=" + String(CO2);
    showTextRectangle("CO2", String(CO2), false);
    delay(3000);
  }

  if (hasSHT) {
    if (hasCO2 || hasPM) payload = payload + ",";
    TMP_RH result = ag.periodicFetchData();
    payload = payload + "atmp=" + String(result.t) + ",rhum=" + String(result.rh);

    if (inF) {
      showTextRectangle(String((result.t * 9 / 5) + 32), String(result.rh) + "%", false);
    } else {
      showTextRectangle(String(result.t), String(result.rh) + "%", false);
    }

    delay(3000);
  }

  if (hasBME) {
    if (hasCO2 || hasPM) payload = payload + ",";

    float currentPressure = bme1.readPressure();
    float currentTemperature = bme1.readTempC();
    float currentHumidity = bme1.readHumidity();
    
    payload = payload + "atmp=" + String(currentTemperature) + ",rhum=" + String(currentHumidity)+ ",pres=" + String(currentPressure);

    if (initTrend){
      // Set everything to the same value
      for (int i=0; i<4; i++){
        pressureTrend[i] = currentPressure;
        initTrend = false;
      }
    } else {
      // Shift existing data
      for (int i=0; i<4; i++){
        pressureTrend[i] = pressureTrend[i+1];
      }
    }

    // Last reading is the current pressure
    pressureTrend[4] = currentPressure;

    // Sum the difference between the past readings
    currentTrend = 0;
    for (int i=0; i<4; i++){
      currentTrend -= pressureTrend[i] - pressureTrend[i+1];
    }

    char presTrend = '-';
    if (currentTrend > 1) {
      presTrend = '/';
    }
    if (currentTrend < -1) {
      presTrend = '\\';
    }

    if (inF) {
      showTextRectangle(String((currentTemperature * 9 / 5) + 32,1) + "F", String(currentHumidity,0) + "%" + presTrend, false);
    } else {
      showTextRectangle(String(currentTemperature,1) + "C", String(currentHumidity,0) + "%" + presTrend, false);
    }

    delay(3000);
  }

  // send payload
  if (connectWIFI) {
    Serial.println(payload);
    Serial.println(APIROOT);
    WiFiClient client;
    HTTPClient http;
    http.begin(client, APIROOT);
    http.addHeader("content-type", "text/plain");
    int httpCode = http.POST(payload);
    String response = http.getString();
    Serial.println(httpCode);
    Serial.println(response);
    http.end();
    
    delay(21000);
  }
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (small) {
    display.setFont(ArialMT_Plain_16);
  } else {
    display.setFont(ArialMT_Plain_24);
  }
  display.drawString(32, 16, ln1);
  display.drawString(32, 36, ln2);
  display.display();
}

// Wifi Manager
void connectToWifi() {
  WiFiManager wifiManager;
  //WiFi.disconnect(); //to delete previous saved hotspot
  String HOTSPOT = "AIRGRADIENT-" + String(ESP.getChipId(), HEX);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setTimeout(120);
  if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
  if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
  else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
  else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
  else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
  else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
  else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
  else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
  else return 500;
};
