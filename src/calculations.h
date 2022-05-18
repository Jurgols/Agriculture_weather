#include <Arduino.h>

// Volumetric water content calculation parameters
float slope = 2.48; // slope from linear fit
float intercept = -0.93;


float float_map(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//Saturation vapor pressure
float calculate_svp(float temp){
  float leaf_temp = temp - 2.0;
  return (610.7*pow(10, ((7.5*leaf_temp)/(237.3+leaf_temp))))/1000;
}

//Air vapor pressure
float calculate_avp(float temp, float humidity){
  return calculate_svp(temp)*(humidity/100);
}

//Vapor pressure deficit
float calculate_vpd(float temp, float humidity){
  return calculate_svp(temp)-calculate_avp(temp, humidity);
}

double batt_lvl(int x){
  return  constrain(97.34482 + (10.20562 - 97.34482)/(1 + pow(pow((x/3.590438),42.12697),0.8958739)),0,100);
}
float batt_voltage(int adc){
  return float_map(adc, 0,4095,0,6.6) + 0.24;
}
float sleep_min(int x){
  //ENTER Battery level as precentage
  return 2772 - 84.65*x + 0.92477*pow(x,2) - 0.0035756*pow(x,3);
}
// vwc calculate precentage from voltage
float soil_vwc(float voltage){
  float soil_percent = constrain((((1.0 / voltage) * slope) + intercept) * 100, 0, 100);
  return soil_percent;
}