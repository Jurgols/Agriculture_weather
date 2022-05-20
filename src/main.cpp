#include <Arduino.h>

#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include "Wire.h"
#include "time.h"
#include <HTTPClient.h> /// FOR ESP32 HTTP FOTA UPDATE ///
#include <HTTPUpdate.h> /// FOR ESP32 HTTP FOTA UPDATE ///
//#include <WiFiClient.h> /// FOR ESP32 HTTP FOTA UPDATE ///
#include <ArduinoOTA.h>
#include "InfluxDbClient.h"
#include "InfluxDbCloud.h"

#define DS18_pin 25
#include "sensors.h"
#include "calculations.h"
#include "hidden.h"

unsigned long startmillis =  millis();
WiFiClient client;
RTC_DATA_ATTR unsigned long int set_time;

//Pin connected to soil sensor
#define soil_pin 32
#define sensors_on 27
#define vb_pin 14

#define DEBUG 0

#if DEBUG == 1
#define debug(x) Serial.print(x)
// #define debugf(x) Serial.printf(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugf(x) 
#define debugln(x)
#endif

#define DEVICE "Field_ESP32"
// InfluxDB  server url.
#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
// InfluxDB 2 bucket name
#define INFLUXDB_BUCKET "field_weather"
//Riga time
#define TZ_INFO "EET-2EEST,M3.5.0/3,M10.5.0/4"
unsigned long timestamp;
struct timeval current_time;

WiFiMulti wifiMulti;
InfluxDBClient influx_client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor("wifi_sensor");

SoilMoisture soilwatch(soil_pin,1,6,5,0.009);

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void checkForUpdates();

struct tm * myTime;

void setup() {


  

  setCpuFrequencyMhz(80);
  WiFi.mode(WIFI_OFF);
  btStop();
  Serial.begin(115200);
  debugln("Execution");

  if(getTime() + 1 > set_time){
    Wire.begin(SDA,SCL);
    pinMode(sensors_on, OUTPUT);
    digitalWrite(sensors_on, HIGH);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    gpio_hold_en(GPIO_NUM_27);
    esp_sleep_enable_timer_wakeup(1e+5);
    esp_light_sleep_start();

    //Filter capacative sensor readings
    wakeUpAM2320();
    for(int i = 0; i < 30; i++){
      soilwatch.Update();
      // debugln(soilwatch.readRawValue());
      esp_sleep_enable_timer_wakeup(67000); //66 ms
      esp_light_sleep_start();
    }
    debug("\n\nExecution time: ");
    debugln((millis() - startmillis)/1000);
    float soil_moisture = soilwatch.readVWC();
    float air_temp = read_temp();
    float air_hum = read_hum();
    float soil_temp = read_soil_temp();
    float leaf_temp = read_leafs();
    int batt_adc = analogRead(vb_pin); //battery adc
    gpio_hold_dis(GPIO_NUM_27);
    digitalWrite(sensors_on, LOW);



    
    WiFi.setSleep(false);
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    // debugln(WiFi.getMode());
    wifiMulti.addAP(SSID1, PSK1);
    wifiMulti.addAP(SSID2, PSK2);
    wifiMulti.addAP(SSID3, PSK3);
    for(int i = 0; i < 20; i++){
      debug("Networks discovered: ");
      debugln(WiFi.scanNetworks(false, true));
      if(wifiMulti.run() == WL_CONNECTED) break;
      delay(500);
    }
    debug("\n\nExecution time: ");
    debugln((millis() - startmillis)/1000);
    if(wifiMulti.run() != WL_CONNECTED) esp_deep_sleep(10 * 6e+7);
    setCpuFrequencyMhz(160);


    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    sensor.addTag("device", DEVICE);
    sensor.addTag("SSID", WiFi.SSID());
    sensor.addTag("location","Ciedras, Māras");

    sensor.clearFields();
    sensor.addField("rssi", WiFi.RSSI());
    sensor.addField("air_temperature", air_temp);
    sensor.addField("air_humidity", air_hum);
    sensor.addField("VPD", calculate_vpd(air_temp, air_hum));
    sensor.addField("soil_vwc", soil_moisture);
    sensor.addField("leaf_temp", leaf_temp);
    sensor.addField("soil_temp", soil_temp);
    sensor.addField("leaf_delta", air_temp - leaf_temp);
    sensor.addField("battery_lvl", batt_lvl(batt_voltage(batt_adc)));
    sensor.addField("battery_voltage", batt_voltage(batt_adc));

    // Check server connection
    if (influx_client.validateConnection()) {
      debug("Connected to InfluxDB: ");
      debugln(influx_client.getServerUrl());
    } else {
      debug("InfluxDB connection failed: ");
      debugln(influx_client.getLastErrorMessage());
    }
    
    debugln("Writing: ");
    debugln(influx_client.pointToLineProtocol(sensor));
    debug("\n\nExecution time: ");
    debugln((millis() - startmillis)/1000);
    // Write point
    if (!influx_client.writePoint(sensor)) {
      debugln("InfluxDB write failed: ");
      debugln(influx_client.getLastErrorMessage());
    }
    checkForUpdates();
    WiFi.mode(WIFI_OFF);
    btStop();
    debug("Battery adc: ");
    debugln(batt_adc);
    debug("Battery level: ");
    debugln(batt_voltage(2228));
    debugln(batt_voltage(2457));
    debugln(batt_lvl(batt_voltage(batt_adc)));
    debug("Sleep minutes: ");
    debugln(sleep_min(batt_lvl(batt_voltage(batt_adc))));
    debug("Sleep seconds: ");
    debugln(sleep_min(batt_lvl(batt_voltage(batt_adc)))*60);
    set_time = getTime() + (sleep_min(batt_lvl(batt_voltage(batt_adc)))*60);
    esp_deep_sleep(sleep_min(batt_lvl(batt_voltage(batt_adc))) * 6e+7);
  }
}

void loop() {
  debug("Next measurement: ");
  debugln(set_time);
  

  debug("\n\nExecution time: ");
  debugln((millis() - startmillis)/1000);
  debugln("\n\nEntering Deep Sleep for 1 Minute");
  esp_deep_sleep(1 * 6e+7);
}

void checkForUpdates() {
  String fwURL = String( fwUrlBase );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );
  debugln( "Checking for firmware updates." );
  debug( "Firmware version URL: " );
  debugln( fwVersionURL );
  HTTPClient httpClient;
  httpClient.begin( client, fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();
    debug( "Current firmware version: " );
    debugln( FW_VERSION );
    debug( "Available firmware version: " );
    debugln( newFWVersion );
    int newVersion = newFWVersion.toInt();
    if( newVersion > FW_VERSION ) {
      debugln( "Preparing to update" );
      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = httpUpdate.update( client, fwImageURL ); /// FOR ESP32 HTTP FOTA
      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str()); /// FOR ESP32
          char currentString[64];
          sprintf(currentString, "\nHTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str()); /// FOR ESP32
          break;

        case HTTP_UPDATE_NO_UPDATES:
          debugln("HTTP_UPDATE_NO_UPDATES");
          break;
        case HTTP_UPDATE_OK:
          break;
      }
    }
    else {
      debugln( "Already on latest version" );
    }
  }
  else {
    debug( "Firmware version check failed, got HTTP response code " );
    debugln( httpCode );
    char currentString[64];
    sprintf(currentString, "\nFirmware version check failed, got HTTP response code : %d\n",httpCode);
  }
  httpClient.end();
}
/// End of main function that performs HTTP OTA /// 
