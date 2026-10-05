#include "../firmware/CarReady/LightPolicy.h"
static_assert(chooseLightMotion(-5,-2,3,25).intent==LightIntent::WAIT,"Below-baseline readings cannot move");
static_assert(chooseLightMotion(20,20,3,25).forward==25,"Balanced bright readings use the selected speed");
static_assert(chooseLightMotion(20,21,3,25).rotation==0,"Small imbalance stays straight");
static_assert(chooseLightMotion(40,4,3,25).rotation==-25,"Strong left target pivots left");
static_assert(chooseLightMotion(4,40,3,25).rotation==25,"Strong right target pivots right");
static_assert(chooseLightMotion(10,16,3,25).intent==LightIntent::BEAR_RIGHT,"Moderate imbalance curves toward light");
static_assert(chooseLightMotion(20,20,3,40).forward==25,"Light following respects the 25 percent ceiling");
static_assert(chooseLightMotion(20,20,3,15).forward==15,"Honor a lower selected speed");
static_assert(lightReleaseThreshold(9)==6 && lightReleaseThreshold(1)==1,"Bounded target release hysteresis");
namespace light_test {
using std::min;using std::max;using String=std::string;
uint32_t now=0,telemetryAt=0,lightSampleAt=0;
uint8_t lightConfirmSamples=0;bool lightTargetSeen=false,matrixPresent=false,moving=false;
int lightAdc[2]={20,20},lightBaseline[2]={20,20},lightThreshold=3,speedLimit=25,headAngle=90;
int outputY=0,outputR=0;float front=80,raw=80;
String lightAction,notice;
uint32_t millis(){return now;}
float filteredEcho(){return front;}
bool rawNear(float threshold){return raw>=2 && raw<threshold;}
void haltMotion(){outputY=outputR=0;moving=false;}
void rangeWarning(bool,float){}
bool guardedMotion(int,int y,int rotation) {
  if(y>0 && (front<35 || rawNear(35))){haltMotion();notice="Forward blocked";return false;}
  outputY=y;outputR=rotation;moving=bool(y || rotation);return true;
}
#include "../firmware/CarReady/Light.h"
void reset(){now=telemetryAt=lightSampleAt=0;lightTargetSeen=false;lightConfirmSamples=0;lightThreshold=3;lightBaseline[0]=lightBaseline[1]=20;speedLimit=25;front=raw=80;headAngle=90;matrixPresent=false;haltMotion();}
void sample(int left,int right){now+=100;telemetryAt=now;lightAdc[0]=left;lightAdc[1]=right;lightTick();}
void runTests() {
  reset();sample(60,20);assert(!moving);lightTick();assert(!moving);sample(60,20);assert(outputY==0 && outputR==-25);
  std::cout<<"PASS: light needs two distinct sensor samples; strong left target pivots left\n";
  sample(20,60);assert(outputY==0 && outputR==25);
  std::cout<<"PASS: right target changes steering to a right pivot\n";
  sample(60,60);assert(outputY==25 && outputR==0);
  sample(20,20);assert(!moving && !lightTargetSeen);
  std::cout<<"PASS: balanced light drives straight; losing light stops immediately\n";
  reset();lightBaseline[0]=40;sample(60,40);sample(60,40);assert(outputY==25 && outputR==0);
  std::cout<<"PASS: per-sensor baseline compensates unequal ambient readings\n";
  reset();front=raw=20;sample(60,20);sample(60,20);assert(!moving);sample(60,60);assert(!moving);
  front=raw=-1;sample(60,20);assert(!moving);
  std::cout<<"PASS: close or missing front echoes block light driving and turning\n";
  reset();sample(60,60);sample(60,60);now+=350;lightTick();assert(!moving);
  std::cout<<"PASS: stale light telemetry stops wheels\n";
  reset();lightThreshold=9;sample(40,40);sample(40,40);sample(26,26);assert(moving);sample(25,25);assert(!moving);
  std::cout<<"PASS: target hysteresis holds small fluctuations but releases below its floor\n";
  reset();lightBaseline[0]=lightBaseline[1]=80;sample(30,30);sample(30,30);assert(!moving && lightAction.find("Baseline too bright")!=String::npos);
  std::cout<<"PASS: stale bright baseline is explained and never authorizes movement\n";
}
}
