#pragma once
// Freenove's shipped battery conversion: ADC / 1023 * 3.3 * 3.95.
float batteryAdcFiltered=0;
int batteryAdcRaw=0,lightAdc[2]={0,0};
bool telemetryReady=false,lightCalibrated=false;
uint32_t telemetryAt=0;
int adcAverage(uint8_t pin) {
  analogRead(pin); // Allow the ADC mux to settle after switching channels.
  long sum=0;for(int i=0;i<5;i++){sum+=analogRead(pin);delayMicroseconds(20);}
  return sum/5;
}
void sampleTelemetry() {
  if(telemetryReady && uint32_t(millis()-telemetryAt)<100)return;
  telemetryAt=millis();batteryAdcRaw=adcAverage(26);
  batteryAdcFiltered=telemetryReady ? batteryAdcFiltered*.8f+batteryAdcRaw*.2f : batteryAdcRaw;
  lightAdc[0]=adcAverage(28);lightAdc[1]=adcAverage(27);telemetryReady=true;
}
float batteryVolts() {return batteryAdcFiltered/1023.0f*3.3f*3.95f;}
int batteryPercent() {float v=batteryVolts();if(v<3 || v>8.8)return -1;return constrain(int((v-6.7f)/1.7f*100+.5f),0,100);}
const char* batteryState() {
  float v=batteryVolts();
  if(v<3)return "Battery off / USB only";
  if(v>8.8)return "Check voltage reading";
  if(v<6.7)return "Low battery: movement stopped";
  if(v<7.2)return "Low: recharge soon";
  if(v<7.6)return "Moderate";
  return "Good";
}
