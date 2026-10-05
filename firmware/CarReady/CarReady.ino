// FNK0089 Pico W mecanum base. Pin map and matrix patterns from Freenove.
#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>
#include <hardware/watchdog.h>
#include <WiFi.h>
#include <SimpleMDNS.h>
#include "wifi_credentials.h"
#include <Adafruit_NeoPixel.h>
#define NO_LED_FEEDBACK_CODE
#define SEND_PWM_BY_TIMER
#include <IRremote.hpp>
Adafruit_NeoPixel chassisLeds(8,16,NEO_GRB+NEO_KHZ800);
uint32_t lastIr=0, irCount=0;
bool motorTestActive=false;
uint32_t motorTestDeadline=0;

WiFiServer lanServer(80);
struct HttpSlot {WiFiClient client;String headers,response;size_t sent=0;uint32_t started=0;bool page=false;};
HttpSlot httpSlots[4];
uint32_t lastWifiAttempt=0;
bool lanStarted=false;
#include "Dashboard.h"

const uint8_t motorPins[4][2] = {{18,19},{21,20},{7,6},{9,8}};
const int motorSigns[4] = {1,1,1,1}; // Change individual signs after wheel calibration.
Servo head;
bool matrixPresent = false, armed = false, moving = false;
float distanceCm = -1;
uint32_t lastDrive = 0, lastSample = 0, lastEyes = 0;
char command[96];
size_t commandLength = 0;
bool overflowed = false;
#include "Control.h"

void stopMotors() {
  for (auto &pair : motorPins) for (uint8_t pin : pair) pwm_set_gpio_level(pin, 0);
  moving = false;
  motorTestActive=false;
}

void matrixCommand(uint8_t value) {
  Wire.beginTransmission(0x71); Wire.write(value); Wire.endTransmission();
}

void eyes(bool blink) {
  const uint8_t openEye[8] = {0,0x18,0x24,0x42,0x42,0x24,0x18,0};
  const uint8_t closedEye[8] = {0,0,0,0x7e,0x7e,0,0,0};
  const uint8_t *pattern = blink ? closedEye : openEye;
  uint16_t buffer[8] = {};
  // Match Freenove setPixel(row, col): buffer[7-col] bit row.
  for (int row=0; row<16; row++) for (int col=0; col<8; col++)
    if (pattern[row%8] & (1<<col)) buffer[7-col] |= (1u<<row);
  Wire.beginTransmission(0x71); Wire.write(0);
  for (uint16_t value : buffer) { Wire.write(uint8_t(value)); Wire.write(uint8_t(value>>8)); }
  Wire.endTransmission();
}

String statusJson() {
  if(safetyExpired)expireControl();
  String result="{\"firmware\":\"CarReady-2.4\",\"board\":\"Pico W\",\"module\":\"";
  result+=matrixPresent ? "matrix" : "ultrasonic";
  result+="\",\"armed\":"; result+=armed ? "true" : "false";
  result+=",\"moving\":"; result+=moving ? "true" : "false";
  result+=",\"distance_cm\":"; result+=(matrixPresent || distanceCm<0) ? String("null") : String(distanceCm,1);
  result+=",\"battery_adc\":"; result+=String(batteryAdcRaw);
  result+=",\"battery_volts\":"+String(batteryVolts(),2)+",\"battery_percent\":"+(batteryPercent()<0 ? String("null") : String(batteryPercent()));
  result+=",\"battery_state\":\""+String(batteryState())+"\"";
  result+=",\"wifi_connected\":"; result+=WiFi.status()==WL_CONNECTED ? "true" : "false";
  result+=",\"ip\":\""; result+=WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : String("");
  result+="\",\"line\":["+String(digitalRead(12))+","+String(digitalRead(11))+","+String(digitalRead(10))+"]";
  result+=",\"light\":["+String(lightAdc[0])+","+String(lightAdc[1])+"]";
  result+=",\"ir_count\":"+String(irCount)+",\"ir_raw\":"+String(lastIr)+controlStatus()+"}"; return result;
}
void status() {Serial.println(statusJson());}

void serveLan() {
  if(motorTestActive) return; // Diagnostic pulses last <=400ms; do not block their deadline with network writes.
  bool connected=WiFi.status()==WL_CONNECTED;
  if(!connected) {
    if(lanStarted) {for(auto &slot:httpSlots) slot.client.stop();lanServer.end();MDNS.end();lanStarted=false;fullStop();notice="Wi-Fi lost: stopped";alert("wifi",3);}
    if(uint32_t(millis()-lastWifiAttempt)>20000) {lastWifiAttempt=millis();WiFi.begin(WIFI_SSID,WIFI_PASSWORD);}
    return;
  }
  if(!lanStarted) {lanServer.begin(80,8);lanServer.setNoDelay(true);MDNS.begin("freenove-car");MDNS.addService("http","tcp",80);lanStarted=true;}
  MDNS.update();
  for(auto &slot:httpSlots) {
  WiFiClient &lanClient=slot.client;
  String &httpHeaders=slot.headers;
  if(!lanClient) {slot.response="";slot.sent=0;slot.page=false;lanClient=lanServer.accept();httpHeaders="";slot.started=millis();if(lanClient)lanClient.setTimeout(100);}
  if(!lanClient) continue;
  if(uint32_t(millis()-slot.started)>(slot.response.length() ? 4000 : 2000) || httpHeaders.length()>8192) {lanClient.stop();slot.response="";continue;}
  if(slot.response.length()) {
    // Never send more than TCP can accept. A slow/closed browser must not hold the control loop.
    int room=lanClient.availableForWrite();
    // Stream the larger dashboard directly from flash, keeping four clients within the heap budget.
    static const size_t tokenAt=strstr(dashboard,"__TOKEN__")-dashboard;
    size_t total=slot.response.length()+(slot.page ? sizeof(dashboard)-1-9+controlToken.length() : 0);
    const char *data;size_t remaining;
    if(slot.sent<slot.response.length()){data=slot.response.c_str()+slot.sent;remaining=slot.response.length()-slot.sent;}
    else {size_t position=slot.sent-slot.response.length();
      if(position<tokenAt){data=dashboard+position;remaining=tokenAt-position;}
      else if(position<tokenAt+controlToken.length()){size_t tokenPos=position-tokenAt;data=controlToken.c_str()+tokenPos;remaining=controlToken.length()-tokenPos;}
      else {size_t rawPos=position-controlToken.length()+9;data=dashboard+rawPos;remaining=sizeof(dashboard)-1-rawPos;}
    }
    size_t amount=min(size_t(512),remaining);
    if(room>0){amount=min(amount,size_t(room));slot.sent+=lanClient.write(reinterpret_cast<const uint8_t*>(data),amount);}
    if(slot.sent>=total){lanClient.stop();slot.response="";}
    continue;
  }
  int budget=512;
  while(lanClient.available() && budget-->0) {
    httpHeaders+=char(lanClient.read());
    if(httpHeaders.endsWith("\r\n\r\n")) {
      String body;const char *type="application/json";int code=200;
      if(httpHeaders.startsWith("GET / HTTP/")) {slot.page=true;type="text/html; charset=utf-8";}
      else if(httpHeaders.startsWith("GET /api/status HTTP/")) body=statusJson();
      else if(httpHeaders.startsWith("POST /api/stop HTTP/")) {fullStop();body="{\"stopped\":true}";}
      else if(httpHeaders.startsWith("POST /api/control?")) {int end=httpHeaders.indexOf(' ',5);body=controlRequest(httpHeaders.substring(5,end),httpHeaders,code);}
      else {body="{\"error\":\"not found\"}";code=404;}
      slot.response=String("HTTP/1.1 ")+code+(code==200 ? " OK\r\n" : " Error\r\n");
      size_t length=slot.page ? sizeof(dashboard)-1-9+controlToken.length() : body.length();
      slot.response+=String("Content-Type: ")+type+"\r\nCache-Control: no-store\r\nX-Frame-Options: DENY\r\nConnection: close\r\nContent-Length: "+length+"\r\n\r\n";
      slot.response+=body;slot.sent=0;slot.started=millis();break;
    }
  }
  }
}

void handleCommand() {
  int a,b,c,d,angle; char extra;
  if (!strcmp(command,"STATUS")) status();
  else if(!strcmp(command,"TESTNOECHO LIFTED")) {
    if(matrixPresent){fullStop();Serial.println("ERR fit ultrasonic for recovery test");return;}
    startRun(PILOT,"USB-recovery-test",1000);speedLimit=25;
    diagnosticEchoMuted=true;resetEchoes();autonomousRoam=true;
    roamDeadline=millis()+15000;hardRunDeadline=roamDeadline;
    Serial.println("OK lifted recovery test: echoes muted until two pivots; 15 second stop");
  }
  else if(!strcmp(command,"TESTWATCHDOG")) {
    fullStop();Serial.println("OK watchdog recovery test; restarting in 2 seconds");Serial.flush();
    while(true)tight_loop_contents(); // The hardware watchdog must recover without main-loop help.
  }
  else if (sscanf(command,"TESTMOTOR %d %d %d %c",&a,&b,&c,&extra)==3) {
    fullStop();
    if(a<1 || a>4 || b < -35 || b>35 || c<1 || c>400) {Serial.println("ERR test bounds: wheel 1..4, speed -35..35, duration 1..400ms");return;}
    int speed=b*motorSigns[a-1];
    pwm_set_gpio_level(motorPins[a-1][speed<0 ? 1 : 0],abs(speed)*100);
    moving=(speed!=0);motorTestActive=true;motorTestDeadline=millis()+c;lease(c);wheelOutput[a-1]=b;
    Serial.println("OK bounded wheel test");
  } else if(sscanf(command,"LED %d %d %d %c",&a,&b,&c,&extra)==3) {
    if(a<0 || a>255 || b<0 || b>255 || c<0 || c>255) {Serial.println("ERR LED range");return;}
    chassisLeds.fill(chassisLeds.Color(a,b,c));chassisLeds.show();Serial.println("OK LEDs");
  }
  else if (!strcmp(command,"STOP") || !strcmp(command,"DISARM")) {
    fullStop(); Serial.println("OK stopped and disarmed");
  } else if (!strcmp(command,"ARM")) {
    fullStop(); armed=true; runMode=USB_CONTROL; controller="USB"; lease(500); lastDrive=millis(); Serial.println("OK armed; DRIVE lease 500ms");
  } else if (sscanf(command,"SERVO %d %c",&angle,&extra)==1 && angle>=30 && angle<=150) {
    fullStop();setHead(angle); Serial.println("OK servo");
  } else if (!strcmp(command,"BEEP")) {
    // GPIO2 shares a PWM slice with motor GPIO18/19: use software pulse instead.
    for(int i=0;i<100;i++) {digitalWrite(2,HIGH);delayMicroseconds(250);digitalWrite(2,LOW);delayMicroseconds(250);}
    Serial.println("OK beep");
  } else if (sscanf(command,"DRIVE %d %d %d %d %c",&a,&b,&c,&d,&extra)==4) {
    if (!armed) {Serial.println("ERR disarmed");return;}
    if (a < -40 || a > 40 || b < -40 || b > 40 || c < -40 || c > 40 || d < -40 || d > 40) {fullStop();Serial.println("ERR speed range -40..40");return;}
    stopMotors(); int values[4]={a,b,c,d};
    for(int i=0;i<4;i++) {int value=values[i]*motorSigns[i];pwm_set_gpio_level(motorPins[i][value<0 ? 1 : 0],abs(value)*100);}
    moving=(a || b || c || d);lastDrive=millis();lease(500);for(int i=0;i<4;i++)wheelOutput[i]=values[i]; Serial.println("OK drive");
  } else if (!strcmp(command,"HELP")) Serial.println("STATUS | ARM | DISARM | STOP | DRIVE m1 m2 m3 m4 | TESTMOTOR wheel speed milliseconds | LED r g b | SERVO 30..150 | BEEP");
  else {fullStop();Serial.println("ERR command; stopped");}
}

void setup() {
  // Hold every motor input LOW before initializing peripherals.
  for(auto &pair:motorPins) for(uint8_t pin:pair) {pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  Serial.begin(115200); analogReadResolution(10);
  for(auto &pair:motorPins) for(uint8_t pin:pair) {
    gpio_set_function(pin,GPIO_FUNC_PWM);
    pwm_config config=pwm_get_default_config();
    pwm_config_set_clkdiv(&config,float(clock_get_hz(clk_sys))/5000000.0f);
    pwm_config_set_wrap(&config,9999); // 500Hz, independent of servo PWM.
    pwm_init(pwm_gpio_to_slice_num(pin),&config,true);
    pwm_set_gpio_level(pin,0);
  }
  stopMotors(); pinMode(2,OUTPUT); digitalWrite(2,LOW);
  for(uint8_t pin: {10,11,12}) pinMode(pin,INPUT);
  pinMode(28,INPUT);pinMode(27,INPUT);
  chassisLeds.begin();chassisLeds.setBrightness(32);chassisLeds.clear();chassisLeds.show();
  IrReceiver.begin(3,false);
  head.attach(13,500,2500); head.write(90);
  Wire.setSDA(4); Wire.setSCL(5); Wire.begin(); Wire.setClock(100000); Wire.setTimeout(25);
  delay(300); Wire.beginTransmission(0x71); matrixPresent=(Wire.endTransmission()==0);
  if(matrixPresent) {matrixCommand(0x21);matrixCommand(0x81);matrixCommand(0xe4);eyes(false);}
  else {Wire.end();pinMode(4,OUTPUT);digitalWrite(4,LOW);pinMode(5,INPUT);}
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);lastWifiAttempt=millis();controlSetup();
  watchdog_enable(2000,true); // Hardware recovery if USB/network code ever wedges the main loop.
}

void loop() {
  watchdog_update();
  sampleTelemetry();
  if(motorTestActive && (int32_t(millis()-motorTestDeadline)>=0 || !Serial)) stopMotors();
  controlTick();
  processRemote();
  serveLan();
  while(Serial.available()) {
    char ch=Serial.read();
    if(ch=='\r') continue;
    if(ch=='\n') {
      command[commandLength]=0;
      if(overflowed) {fullStop();Serial.println("ERR command too long");}
      else if(commandLength) handleCommand();
      commandLength=0;overflowed=false;
    } else if(commandLength<sizeof(command)-1 && !overflowed) command[commandLength++]=ch;
    else {overflowed=true;fullStop();}
  }
  if(!matrixPresent && uint32_t(millis()-lastSample)>=60) {
    lastSample=millis();digitalWrite(4,LOW);delayMicroseconds(2);
    digitalWrite(4,HIGH);delayMicroseconds(10);digitalWrite(4,LOW);
    unsigned long duration=pulseIn(5,HIGH,18000);
    recordEcho(duration);
  }
  if(matrixPresent && uint32_t(millis()-lastEyes)>=150) {
    lastEyes=millis();eyes((millis()%4000)>3750);
  }
}
