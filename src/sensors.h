#include <Arduino.h>
#include <SimpleKalmanFilter.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include <AM232X.h>
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
AM232X am2320;
OneWire oneWire(DS18_pin);
DallasTemperature ds_temp(&oneWire);


float soil_voltage(uint8_t pin){
  int soil_adc = analogRead(pin);
  float voltage = (soil_adc / 4095.0) * 3.3;
  return voltage;
}

float read_temp(){
  am2320.begin();
  am2320.wakeUp();
  delay(2000);
  am2320.read();
  return am2320.getTemperature();
}
float read_hum(){
  am2320.begin();
  am2320.wakeUp();
  return am2320.getHumidity();
}

float read_soil_temp(){
  ds_temp.begin();
  ds_temp.requestTemperaturesByIndex(0);
  return ds_temp.getTempCByIndex(0);
}

float read_leafs(){
  mlx.begin();
  return mlx.readObjectTempC();
}
class SoilMoisture
{
 //Member variables
 float moistureEstimate;
 uint8_t pin;
 int sensorType;
 unsigned int rawValue;
 private:
 //Member object type class
 SimpleKalmanFilter* kalmanObj;
 private:
 float _function(float x, float a, float b){
   return a*exp(b*x);
 }
 public:
 //construtor
 SoilMoisture(uint8_t analog_pin, int sensorType, int mea_err, int est_err, float variance){
  //create object add to member variabels
  kalmanObj = new SimpleKalmanFilter(mea_err, est_err, variance);
  this->pin = analog_pin;
  this->sensorType = sensorType;
  moistureEstimate = 0;
  rawValue = 0;
 }

void Update(){
  float soilvwc;
  if(sensorType){
    rawValue = analogRead(pin);
    float voltage = soil_voltage(pin);
    soilvwc = constrain((5.5667f * voltage * voltage * voltage) - (19.6738f * voltage * voltage) + (31.0893f * voltage) - 6.7511f, 0, 100);
  }
  else{
    soilvwc = constrain(_function(analogRead(pin), 4.66, 0.000949),0,100);
  }
  moistureEstimate = kalmanObj->updateEstimate(soilvwc);
  }
  float readVWC(){
    return moistureEstimate;
  }
  int readRawValue(){
    return rawValue;
  }


};
