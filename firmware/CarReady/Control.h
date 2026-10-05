#pragma once
#include "Motion.h"
#include "Telemetry.h"
#include "RoamPolicy.h"
#include "EchoPolicy.h"
#include "LinePolicy.h"
#include <pico/time.h>
#include <pico/rand.h>
#include <hardware/sync.h>
void stopMotors();
enum RunMode {IDLE,MANUAL,LINE,LIGHT,PILOT,SHOW,USB_CONTROL,REMOTE_MANUAL};
RunMode runMode=IDLE;
String controller="",notice="Stopped",controlToken="";
bool frontGuard=true;
int driveX=0,driveY=0,driveR=0,speedLimit=25,headAngle=90,lightThreshold=3;
int lightBaseline[2]={0,0},wheelOutput[4]={0,0,0,0};
float echoes[5]={-1,-1,-1,-1,-1};int echoPosition=0,echoCount=0;
uint32_t echoTime=0,modeStart=0,lastEffects=0;
bool diagnosticEchoMuted=false; // USB-only lifted-car fault injection; cleared by every Stop/reset.
volatile uint32_t beepUntil=0;
volatile uint16_t buzzerStep=6554,buzzerDuty=32768;
int buzzerVolume=60,buzzerFrequency=2000,userTone=2000;
uint32_t melodyStarted=0;bool melodyActive=false;
void setTone(int frequency){buzzerFrequency=frequency;buzzerStep=uint32_t(frequency)*65536/20000;}
void playTone(int frequency,int duration){setTone(frequency);beepUntil=millis()+duration;}
int pilotStage=0;uint32_t pilotTime=0;float pilotLeft=-1,pilotRight=-1;
float pilotCenter=-1;uint32_t lastCruiseScan=0;
int scanLeftAngle=60;
bool escapeEnabled=true,pilotEscape=false,pilotSurvey=false;
int pilotTurns=0,pilotDirection=0;
String pilotAction="Stopped";
int wideLeftAngle(){return scanLeftAngle==60 ? 30 : 150;}
float pilotScan[5]={-1,-1,-1,-1,-1};bool pilotExploring=false;
int lineBlack=0;String lineAction="Stopped"; // Owner's center-only black fixture: raw 1/0/1.
uint32_t lineLastSeen=0;
float radarDistance[9]={-1,-1,-1,-1,-1,-1,-1,-1,-1};uint32_t radarTime[9]={};
void clearRadar(){for(int i=0;i<9;i++){radarDistance[i]=-1;radarTime[i]=0;}}
int scanAngle(int index){int angles[]={30,60,90,120,150};return scanLeftAngle==60 ? angles[index] : 180-angles[index];}
bool autonomousRoam=false;
uint32_t roamDeadline=0;
volatile uint32_t hardRunDeadline=0;
bool soundAlerts=true,partySound=false;
int ledBrightness=12;
String alertName="";uint32_t alertStarted=0,lastWarning=0,lastPartyBeat=0;
int alertPulses=0;
uint32_t lastBatterySample=0;uint8_t lowBatterySamples=0;
String ledStyle="auto";
volatile bool safetyActive=false,safetyExpired=false;
volatile uint32_t safetyDeadline=0;
repeating_timer_t safetyTimer,buzzerTimer;

bool safetyCallback(repeating_timer_t*) {
  if(safetyActive && (int32_t(millis()-safetyDeadline)>=0 || (hardRunDeadline && int32_t(millis()-hardRunDeadline)>=0))) {
    safetyActive=false;safetyExpired=true;
    for(auto &pair:motorPins) for(uint8_t pin:pair) pwm_set_gpio_level(pin,0);
  }return true;
}
bool buzzerCallback(repeating_timer_t*) {
  static uint16_t phase=0;
  if(int32_t(millis()-beepUntil)<0) {phase+=buzzerStep;gpio_put(2,phase<buzzerDuty);}
  else {phase=0;gpio_put(2,0);}return true;
}
void lease(uint32_t duration) {safetyDeadline=millis()+duration;safetyExpired=false;safetyActive=true;}
void fullStop() {
  safetyActive=false;safetyExpired=false;armed=false;runMode=IDLE;controller="";
  driveX=driveY=driveR=0;stopMotors();for(int &v:wheelOutput)v=0;
  beepUntil=0;gpio_put(2,0);chassisLeds.clear();chassisLeds.show();notice="Stopped and disarmed";
  autonomousRoam=false;roamDeadline=0;hardRunDeadline=0;partySound=false;ledStyle="auto";
  alertPulses=0;alertName="";lastWarning=millis()-1500;
  melodyActive=false;pilotAction="Stopped";pilotTurns=0;pilotDirection=0;pilotEscape=false;pilotSurvey=false;
  lineLastSeen=0;lineAction="Stopped";
  diagnosticEchoMuted=false;
}
int warningPitch(const String &name){return name=="obstacle" ? 1400 : name=="sensor" ? 900 : name=="battery" ? 550 : name=="turn" ? 1800 : name=="finished" ? 2200 : 1100;}
void alert(const char *name,int pulses) {if(alertName==name && (alertPulses || uint32_t(millis()-lastWarning)<800))return;lastWarning=millis();alertName=name;alertStarted=millis();alertPulses=pulses;if(soundAlerts)playTone(warningPitch(alertName),80);}
void alertTick() {
  if(!alertPulses)return;
  uint32_t elapsed=millis()-alertStarted;
  if(elapsed>=uint32_t(alertPulses*240)){alertPulses=0;return;}
  if(soundAlerts && elapsed%240<100) {
    int pitch=warningPitch(alertName);
    playTone(pitch+(elapsed/240%2)*250,25);
  }
}
void haltMotion() {stopMotors();for(int &v:wheelOutput)v=0;driveX=driveY=driveR=0;}
void resetEchoes() {echoPosition=echoCount=0;echoTime=0;for(float &e:echoes)e=-1;}
uint32_t headMovedAt=0;
void setHead(int angle) {headAngle=angle;head.write(angle);headMovedAt=millis();resetEchoes();}
void recordEcho(unsigned long duration) {
  float value=duration ? duration*.017f : -1;
  distanceCm=value;
  if(diagnosticEchoMuted)value=-1;
  if(uint32_t(millis()-headMovedAt)<180)return;
  echoes[echoPosition]=value;echoPosition=(echoPosition+1)%5;if(echoCount<5)echoCount++;
  echoTime=millis();
  int bin=constrain((headAngle-30+7)/15,0,8);radarDistance[bin]=stableEcho(echoes,echoCount,(echoPosition+4)%5);radarTime[bin]=millis();
}
float filteredEcho() {
  if(echoCount<3 || uint32_t(millis()-echoTime)>200) return -1;
  // A current miss stops travel immediately; one older missed echo need not poison five later readings.
  return stableEcho(echoes,echoCount,(echoPosition+4)%5);
}
const char* modeName() {
  switch(runMode){case MANUAL:return "manual";case LINE:return "line";case LIGHT:return "light";case PILOT:return "pilot";case SHOW:return "show";case USB_CONTROL:return "USB";case REMOTE_MANUAL:return "remote";default:return "idle";}
}
bool guardedMotion(int x,int y,int rotation) {
  if(frontGuard && y>0) {
    float distance=filteredEcho();
    if(matrixPresent || headAngle!=90 || distance<0 || distance<25) {
      haltMotion();notice=matrixPresent ? "Forward guard needs the ultrasonic module" : (distance<0 ? "Forward blocked: no reliable front echo" : "Forward blocked: obstacle closer than 25 cm");alert(distance<0 ? "sensor" : "obstacle",distance<0 ? 3 : 2);return false;
    }
  }
  int values[4];mecanumMix(x,y,rotation,speedLimit,values);
  uint32_t interrupts=save_and_disable_interrupts();
  if(!safetyActive || int32_t(millis()-safetyDeadline)>=0){restore_interrupts(interrupts);haltMotion();return false;}
  for(int i=0;i<4;i++) {
    int v=values[i]*motorSigns[i];
    // Clear the opposite input first when changing direction.
    pwm_set_gpio_level(motorPins[i][v>=0 ? 1 : 0],0);
    pwm_set_gpio_level(motorPins[i][v>=0 ? 0 : 1],abs(v)*100);
    wheelOutput[i]=values[i];
  }
  restore_interrupts(interrupts);
  moving=(x||y||rotation);return true;
}
void startRun(RunMode mode,const String &who,uint32_t duration) {
  fullStop();controller=who;runMode=mode;armed=true;modeStart=millis();
  frontGuard=(mode!=SHOW);if(headAngle!=90)setHead(90);lease(duration);
  if(mode==SHOW)ledStyle="auto";
  if(mode==LINE)lineLastSeen=millis();
  if(mode==PILOT){haltMotion();clearRadar();setHead(90);pilotStage=7;pilotTime=millis();pilotLeft=pilotRight=pilotCenter=-1;lastCruiseScan=millis();pilotAction="Checking forward path";pilotExploring=false;}
  notice=mode==SHOW ? "Lifted-car crab and spin show" : "Mode ready";
}
void calibrateLight() {
  fullStop();long left=0,right=0;
  for(int i=0;i<16;i++){left+=adcAverage(28);right+=adcAverage(27);}
  lightBaseline[0]=left/16;lightBaseline[1]=right/16;lightCalibrated=true;notice="Ambient light baseline set; now aim the flashlight";
}
#include "Pilot.h"
void effectsTick() {
  if(uint32_t(millis()-lastEffects)<50)return;lastEffects=millis();
  if(alertPulses){chassisLeds.fill(chassisLeds.Color((millis()/100)%2 ? 255 : 15,0,0));}
  else if(ledStyle=="off"){chassisLeds.clear();}
  else if(ledStyle=="red")chassisLeds.fill(chassisLeds.Color(255,0,0));
  else if(ledStyle=="green")chassisLeds.fill(chassisLeds.Color(0,255,0));
  else if(ledStyle=="blue")chassisLeds.fill(chassisLeds.Color(0,0,255));
  else if(ledStyle=="yellow")chassisLeds.fill(chassisLeds.Color(255,160,0));
  else if(ledStyle=="cyan")chassisLeds.fill(chassisLeds.Color(0,255,255));
  else if(ledStyle=="purple")chassisLeds.fill(chassisLeds.Color(180,0,255));
  else if(ledStyle=="white")chassisLeds.fill(chassisLeds.Color(255,255,255));
  else if(ledStyle=="chase"){chassisLeds.clear();chassisLeds.setPixelColor((millis()/100)%8,chassisLeds.Color(0,255,160));}
  else if(ledStyle=="breathe"){int level=abs(255-int((millis()/8)%510));chassisLeds.fill(chassisLeds.Color(0,level/2,level));}
  else if(ledStyle=="rainbow" || ledStyle=="party" || runMode==SHOW){for(int i=0;i<8;i++)chassisLeds.setPixelColor(i,chassisLeds.ColorHSV(uint16_t(millis()*(ledStyle=="party" ? 65 : 25)+i*8000)));}
  else if(moving)chassisLeds.fill(chassisLeds.Color(runMode==MANUAL || runMode==REMOTE_MANUAL ? 0 : 20,runMode==MANUAL || runMode==REMOTE_MANUAL ? 60 : 200,runMode==MANUAL || runMode==REMOTE_MANUAL ? 255 : 40));
  else if(armed)chassisLeds.fill(chassisLeds.Color(100,55,0));else chassisLeds.clear();
  chassisLeds.show();
  if(ledStyle=="party" && partySound && !alertPulses && uint32_t(millis()-lastPartyBeat)>=400){lastPartyBeat=millis();static const int notes[]={523,659,784,1047,784,659,587,784};playTone(notes[(millis()/400)%8],(millis()/400)%4==0 ? 140 : 70);}
  if(melodyActive && !alertPulses){uint32_t t=millis()-melodyStarted;static const int notes[]={523,659,784,1047,784,659};if(t>=1800)melodyActive=false;else if(t%300<200)playTone(notes[t/300],25);}
}
void expireControl() {
  bool finished=roamDeadline && int32_t(millis()-roamDeadline)>=0;
  fullStop();notice=finished ? "Roaming timer finished" : "Control timeout: stopped";
  alert(finished ? "finished" : "timeout",finished ? 2 : 3);
}
void controlTick() {
  alertTick();
  if(armed && roamDeadline && int32_t(millis()-roamDeadline)>=0){expireControl();return;}
  if(safetyExpired){expireControl();return;}
  if(armed && autonomousRoam)lease(min(uint32_t(750),roamDeadline-millis()));
  if(diagnosticEchoMuted && runMode==PILOT && pilotTurns>=2 && pilotStage==7){diagnosticEchoMuted=false;resetEchoes();}
  if(!armed){effectsTick();return;}
  // Reject sustained low supply, allowing short motor-start and ADC transients.
  if(uint32_t(millis()-lastBatterySample)>=100) {
    lastBatterySample=millis();
    int battery=batteryAdcRaw;
    lowBatterySamples=battery<525 ? min(3,int(lowBatterySamples)+1) : 0;
    if(lowBatterySamples>=3){fullStop();notice="Low battery: stopped; recharge";alert("battery",4);return;}
  }
  if(runMode==MANUAL || runMode==REMOTE_MANUAL) {
    guardedMotion(driveX,driveY,driveR);
  }else if(runMode==LINE) {
    int bits=(digitalRead(12)<<2)|(digitalRead(11)<<1)|digitalRead(10);
    if(!lineBlack)bits^=7;int steering=lineSteering(bits);
    if(bits!=0 && steering!=99)lineLastSeen=millis();
    if(steering==99 || (bits==0 && uint32_t(millis()-lineLastSeen)>500)) {haltMotion();lineAction=bits==7 ? "All black / no floor: stopped" : "White gap exceeded 500 ms: waiting for line";notice=lineAction;alert("line",1);}
    else {lineAction=steering==0 ? (bits==0 ? "Crossing short white gap: straight" : "Centered: straight forward") : steering<0 ? "Correcting left" : "Correcting right";if(guardedMotion(0,min(speedLimit,28),steering))notice=lineAction;}
  }else if(runMode==LIGHT) {
    int left=max(0,lightAdc[0]-lightBaseline[0]),right=max(0,lightAdc[1]-lightBaseline[1]);
    if(max(left,right)<lightThreshold){haltMotion();notice="Waiting for a brighter light target";}
    else {int rotation=constrain((right-left)/2,-14,14);guardedMotion(0,min(speedLimit,18),rotation);notice=moving ? "Following light target" : notice;}
  }else if(runMode==PILOT)pilotTick();
  else if(runMode==SHOW) {
    uint32_t elapsed=millis()-modeStart;
    if(elapsed>=10000){fullStop();notice="Show finished";return;}
    static const int pattern[8][3]={{-22,0,0},{22,0,0},{0,0,-22},{0,0,22},{-15,15,0},{15,-15,0},{15,15,0},{-15,-15,0}};
    int step=(elapsed/1200)%8;
    if(elapsed%1200<850)guardedMotion(pattern[step][0],pattern[step][1],pattern[step][2]);else haltMotion();
    static int lastStep=-1;if(step!=lastStep){lastStep=step;beepUntil=millis()+65;}
  }
  effectsTick();
}
String valueOf(const String &target,const String &key) {
  int start=target.indexOf('?');if(start<0)return "";
  String query=target.substring(start+1);int at=0;
  while(at<query.length()){int end=query.indexOf('&',at);if(end<0)end=query.length();String item=query.substring(at,end);int eq=item.indexOf('=');if(eq>=0 && item.substring(0,eq)==key)return item.substring(eq+1);at=end+1;}return "";
}
String headerOf(const String &headers,const String &name) {
  int at=headers.indexOf("\r\n")+2;
  while(at>1 && at<headers.length()){int end=headers.indexOf("\r\n",at);if(end<0)break;String line=headers.substring(at,end);int colon=line.indexOf(':');if(colon>=0 && line.substring(0,colon).equalsIgnoreCase(name)){String value=line.substring(colon+1);value.trim();return value;}at=end+2;}return "";
}
bool parameterInt(const String &value,int low,int high,int &out) {
  if(value.length()==0 || value.length()>5)return false;
  for(int i=0;i<value.length();i++)if(!(value[i]>='0' && value[i]<='9') && !(i==0 && value[i]=='-' && value.length()>1))return false;
  long result=value.toInt();if(result<low || result>high)return false;out=result;return true;
}
String controlRequest(const String &target,const String &headers,int &code) {
  String who=headerOf(headers,"X-Car-Owner"),token=headerOf(headers,"X-Car-Token");
  auto fail=[&](int status,const char *error){code=status;return String("{\"error\":\"")+error+"\"}";};
  if(token!=controlToken || who.length()<3 || who.length()>64)return fail(403,"Control token missing; reload the page");
  if(armed && controller!=who)return fail(409,"Another controller is active; Stop first");
  String op=valueOf(target,"op");int x,y,r,s,angle;
  if(op=="arm") {
    if(!parameterInt(valueOf(target,"guard"),0,1,s))return fail(400,"Invalid guard setting");
    startRun(MANUAL,who,750);frontGuard=s;notice="Manual armed; hold a direction to move";
  }else if(op=="drive") {
    if(!armed || runMode!=MANUAL)return fail(409,"Arm manual controls first");
    if(!parameterInt(valueOf(target,"x"),-1,1,x)||!parameterInt(valueOf(target,"y"),-1,1,y)||!parameterInt(valueOf(target,"r"),-1,1,r)||!parameterInt(valueOf(target,"speed"),15,40,s)){fullStop();return fail(400,"Invalid movement");}
    speedLimit=s;driveX=x*s;driveY=y*s;driveR=r*s;lease(500);notice="Manual movement";guardedMotion(driveX,driveY,driveR);
  }else if(op=="heartbeat") {
    if(!armed)return fail(409,"Car is stopped");if(runMode==MANUAL)haltMotion();lease(runMode==MANUAL ? 750 : 1000);
  }else if(op=="halt"){haltMotion();lease(750);notice="Direction released: stopped";}
  else if(op=="mode") {
    String name=valueOf(target,"name");RunMode requested;
    if(name=="line")requested=LINE;else if(name=="light")requested=LIGHT;else if(name=="pilot")requested=PILOT;else if(name=="show")requested=SHOW;else return fail(400,"Unknown mode");
    if(requested==SHOW && valueOf(target,"lift")!="1")return fail(400,"Lift the car for the show");
    if(requested!=SHOW && matrixPresent)return fail(409,"Fit ultrasonic module for guarded automatic modes");
    if(!parameterInt(valueOf(target,"speed"),15,40,s))return fail(400,"Invalid speed");
    if(requested==LIGHT && !lightCalibrated)return fail(409,"Set ambient light baseline with flashlight off first");
    if(requested==LIGHT && !parameterInt(valueOf(target,"threshold"),1,80,lightThreshold))return fail(400,"Invalid sensitivity");
    if(requested==LINE && valueOf(target,"lineblack").length() && !parameterInt(valueOf(target,"lineblack"),0,1,lineBlack))return fail(400,"Invalid line polarity");
    int seconds=60,detached=0,leftAngle=scanLeftAngle;
    if(requested==PILOT) {
      if(valueOf(target,"seconds").length() && !parameterInt(valueOf(target,"seconds"),5,600,seconds))return fail(400,"Timer must be 5 to 600 seconds");
      if(valueOf(target,"autonomous").length() && !parameterInt(valueOf(target,"autonomous"),0,1,detached))return fail(400,"Invalid autonomous setting");
      if(valueOf(target,"scanleft").length() && (!parameterInt(valueOf(target,"scanleft"),60,120,leftAngle) || (leftAngle!=60 && leftAngle!=120)))return fail(400,"Scan left angle must be 60 or 120");
      int escape=1;if(valueOf(target,"escape").length() && !parameterInt(valueOf(target,"escape"),0,1,escape))return fail(400,"Invalid escape setting");
      scanLeftAngle=leftAngle;escapeEnabled=escape;
    }
    startRun(requested,who,1000);speedLimit=s;
    if(requested==PILOT){roamDeadline=millis()+uint32_t(seconds)*1000;hardRunDeadline=roamDeadline;autonomousRoam=detached;}
  }else if(op=="calibrate")calibrateLight();
  else if(op=="lineconfig"){int black;if(!parameterInt(valueOf(target,"black"),0,1,black))return fail(400,"Invalid line polarity");fullStop();lineBlack=black;notice="Line polarity updated; car stopped";}
  else if(op=="servo") {if(!parameterInt(valueOf(target,"angle"),30,150,angle))return fail(400,"Invalid head angle");fullStop();setHead(angle);notice="Head repositioned; movement stopped";}
  else if(op=="beep")playTone(userTone,180);
  else if(op=="sound") {int volume,pitch;if(!parameterInt(valueOf(target,"volume"),0,100,volume)||!parameterInt(valueOf(target,"pitch"),400,3000,pitch))return fail(400,"Sound: volume 0..100, pitch 400..3000 Hz");buzzerVolume=volume;buzzerDuty=uint32_t(volume)*32768/100;userTone=pitch;}
  else if(op=="melody"){fullStop();melodyStarted=millis();melodyActive=true;notice="Playing six-note chime; car remains stopped";}
  else if(op=="led") {String color=valueOf(target,"color");if(color!="auto" && color!="off" && color!="red" && color!="green" && color!="blue" && color!="yellow" && color!="cyan" && color!="purple" && color!="white" && color!="rainbow" && color!="chase" && color!="breathe" && color!="party")return fail(400,"Invalid light effect");ledStyle=color;effectsTick();}
  else if(op=="party"){int sound=0;if(!parameterInt(valueOf(target,"sound"),0,1,sound))return fail(400,"Invalid party sound setting");fullStop();ledStyle="party";partySound=sound;notice="Stationary party lights; Stop ends the party";}
  else if(op=="brightness"){if(!parameterInt(valueOf(target,"value"),5,50,s))return fail(400,"Brightness must be 5 to 50 percent");ledBrightness=s;chassisLeds.setBrightness(s*255/100);}
  else if(op=="alerts"){if(!parameterInt(valueOf(target,"value"),0,1,s))return fail(400,"Invalid alert setting");soundAlerts=s;if(!s){beepUntil=0;gpio_put(2,0);}}
  else return fail(400,"Unknown operation");
  return "{\"ok\":true}";
}
void processRemote() {
  if(!IrReceiver.decode())return;
  uint32_t raw=IrReceiver.decodedIRData.decodedRawData;bool repeat=IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  if(raw)lastIr=raw;else if(repeat)raw=lastIr;
  irCount++;IrReceiver.resume();
  if(raw==0xEA15FF00 || raw==0xB54AFF00){fullStop();return;}
  if(armed && controller!="IR")return;
  if(raw==0xBB44FF00){if(!repeat)playTone(userTone,80);return;}
  if(raw==0xBD42FF00){if(!repeat)ledStyle=ledStyle=="red" ? "green" : ledStyle=="green" ? "blue" : "red";return;}
  if(raw==0xAD52FF00){ledStyle="off";return;}
  if(raw==0xE916FF00 || raw==0xF30CFF00 || raw==0xF708FF00) {
    fullStop();setHead(raw==0xF708FF00 ? 90 : constrain(headAngle+(raw==0xE916FF00 ? 10 : -10),30,150));return;
  }
  RunMode selected=IDLE;int x=0,y=0,r=0;
  if(raw==0xBF40FF00){selected=REMOTE_MANUAL;y=25;}else if(raw==0xE619FF00){selected=REMOTE_MANUAL;y=-25;}
  else if(raw==0xF807FF00){selected=REMOTE_MANUAL;r=-25;}else if(raw==0xF609FF00){selected=REMOTE_MANUAL;r=25;}
  else if(raw==0xF20DFF00)selected=LIGHT;else if(raw==0xA15EFF00)selected=LINE;else if(raw==0xA55AFF00)selected=PILOT;
  if(selected==LIGHT && !lightCalibrated){notice="Set ambient light baseline on the dashboard first";return;}
  if(selected==IDLE || (matrixPresent && selected!=REMOTE_MANUAL))return;
  if(repeat && runMode==selected && controller=="IR"){lease(selected==REMOTE_MANUAL ? 350 : 3000);return;}
  startRun(selected,"IR",selected==REMOTE_MANUAL ? 350 : 3000);speedLimit=25;driveX=x;driveY=y;driveR=r;
}
String controlStatus() {
  float distance=(!matrixPresent && headAngle==90) ? filteredEcho() : -1;
  String extra=",\"mode\":\""+String(modeName())+"\",\"notice\":\""+notice+"\",\"front_cm\":"+(distance<0 ? String("null") : String(distance,1));
  extra+=",\"front_guard\":"+String(frontGuard ? "true" : "false")+",\"head_angle\":"+String(headAngle);
  extra+=",\"light_baseline\":["+String(lightBaseline[0])+","+String(lightBaseline[1])+"]";
  extra+=",\"wheels\":["+String(wheelOutput[0])+","+String(wheelOutput[1])+","+String(wheelOutput[2])+","+String(wheelOutput[3])+"]";
  extra+=",\"light_calibrated\":"+String(lightCalibrated ? "true" : "false")+",\"light_threshold\":"+String(lightThreshold);
  extra+=",\"light_delta\":["+String(lightAdc[0]-lightBaseline[0])+","+String(lightAdc[1]-lightBaseline[1])+"]";
  extra+=",\"range_quality\":\""+String(matrixPresent ? "Matrix fitted" : filteredEcho()<0 ? "Unreliable or settling" : "Valid recent echoes")+"\",\"head_cm\":"+(filteredEcho()<0 ? String("null") : String(filteredEcho(),1));
  extra+=",\"autonomous\":"+String(autonomousRoam ? "true" : "false")+",\"remaining_s\":"+String(roamDeadline ? max(0,int32_t(roamDeadline-millis()+999)/1000) : 0);
  extra+=",\"scan_cm\":["+(pilotLeft<0 ? String("null") : String(pilotLeft,1))+","+(pilotCenter<0 ? String("null") : String(pilotCenter,1))+","+(pilotRight<0 ? String("null") : String(pilotRight,1))+"]";
  extra+=",\"led_effect\":\""+ledStyle+"\",\"brightness\":"+String(ledBrightness)+",\"sound_alerts\":"+String(soundAlerts ? "true" : "false")+",\"alert\":\""+alertName+"\"";
  extra+=",\"roam_action\":\""+pilotAction+"\",\"escape_turns\":"+String(pilotTurns)+",\"escape_enabled\":"+String(escapeEnabled ? "true" : "false")+",\"pilot_stage\":"+String(pilotStage);
  extra+=",\"scan_left_angle\":"+String(scanLeftAngle);
  extra+=",\"test_echo_muted\":"+String(diagnosticEchoMuted ? "true" : "false");
  extra+=",\"line_black\":"+String(lineBlack)+",\"line_action\":\""+lineAction+"\",\"radar\":[";
  for(int i=0;i<9;i++){if(i)extra+=",";uint32_t age=millis()-radarTime[i];extra+="["+String(30+i*15)+","+(radarTime[i] && age<6000 && radarDistance[i]>=0 ? String(radarDistance[i],1) : String("null"))+","+String(age)+"]";}extra+="]";
  extra+=",\"sound_volume\":"+String(buzzerVolume)+",\"tone_hz\":"+String(userTone)+",\"playing_melody\":"+String(melodyActive ? "true" : "false");
  return extra;
}
void controlSetup() {
  char token[24];snprintf(token,sizeof(token),"%08lx%08lx",(unsigned long)get_rand_32(),(unsigned long)get_rand_32());controlToken=token;
  add_repeating_timer_ms(-10,safetyCallback,nullptr,&safetyTimer);
  buzzerDuty=uint32_t(buzzerVolume)*32768/100;
  add_repeating_timer_us(-50,buzzerCallback,nullptr,&buzzerTimer);
  sampleTelemetry();calibrateLight();lightCalibrated=false;
}
