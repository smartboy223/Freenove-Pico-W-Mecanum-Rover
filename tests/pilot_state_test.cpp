#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "roam_policy_test.cpp"
using std::min;using std::max;
using String=std::string;
// Hardware boundaries are substituted; Pilot.h is the production state machine.
uint32_t now=0,pilotTime=0,lastCruiseScan=0,headMovedAt=0;
int pilotStage=7,pilotTurns=0,pilotDirection=0,headAngle=90,speedLimit=25;
bool pilotExploring=false,escapeEnabled=true,pilotEscape=false,pilotSurvey=false,armed=true;
float pilotLeft=-1,pilotCenter=-1,pilotRight=-1,pilotScan[5]={};
String pilotAction,notice;
int outputY=0,outputR=0,turnStarts=0,forwardTicks=0,radarClears=0,lastDirection=0;
uint32_t turnStarted=0;float samples[5]={-1,-1,-1,-1,-1};int count=0,position=0;
enum Scenario {MISSING,RECOVERS,DEAD_END,CLOSE_CORNER,CLOSE_DURING_TURN,OPEN};
Scenario scenario=MISSING;
bool exitFound=false;
uint32_t millis(){return now;}
void resetEchoes(){count=position=0;for(float &v:samples)v=-1;}
void setHead(int angle){headAngle=angle;headMovedAt=now;resetEchoes();}
int scanAngle(int index){return 30+index*30;}
float filteredEcho(){return count<3 ? -1 : stableEcho(samples,count,(position+4)%5);}
void haltMotion(){outputY=outputR=0;}
void clearRadar(){radarClears++;}
void fullStop(){armed=false;haltMotion();pilotTurns=pilotDirection=0;pilotSurvey=pilotEscape=false;}
void alert(const char*,int){}
bool guardedMotion(int,int y,int rotation){
  assert(!y || (headAngle==90 && filteredEcho()>=25));
  assert(abs(rotation)<=25 && y<=28);
  if(rotation && !outputR){turnStarts++;turnStarted=now;if(lastDirection)assert(lastDirection==(rotation<0 ? -1 : 1));lastDirection=rotation<0 ? -1 : 1;}
  if(y)forwardTicks++;
  outputY=y;outputR=rotation;return true;
}
#include "../firmware/CarReady/Pilot.h"
float environment(){
  if(scenario==OPEN)return 80;
  if((scenario==RECOVERS || scenario==DEAD_END) && pilotTurns>=2 && pilotStage!=6)exitFound=true;
  if(scenario==RECOVERS && exitFound)return 80;
  if(scenario==DEAD_END)return exitFound ? 80 : 30;
  if(scenario==CLOSE_CORNER && headAngle==30)return 10;
  if(scenario==CLOSE_DURING_TURN && turnStarts>0 && uint32_t(now-turnStarted)>=120)return 10;
  return -1;
}
void reset(Scenario s,bool enabled=true,uint32_t start=0){
  now=start;pilotTime=lastCruiseScan=headMovedAt=start;
  pilotStage=7;pilotTurns=pilotDirection=0;headAngle=90;pilotExploring=pilotEscape=pilotSurvey=false;
  escapeEnabled=enabled;armed=true;pilotLeft=pilotCenter=pilotRight=-1;
  turnStarts=forwardTicks=radarClears=lastDirection=0;turnStarted=0;haltMotion();resetEchoes();scenario=s;exitFound=false;
}
void run(uint32_t duration){
  for(uint32_t elapsed=0;elapsed<duration && armed;elapsed+=10){
    now+=10;
    if(elapsed%60==0 && uint32_t(now-headMovedAt)>=180){samples[position]=environment();position=(position+1)%5;count=min(count+1,5);}
    pilotTick();
    if(outputY)assert(filteredEcho()>=35);
  }
}
int main(){
  reset(MISSING);run(26000);
  assert(!armed && turnStarts==8 && forwardTicks==0 && outputR==0 && radarClears==8);
  std::cout<<"PASS: all missing echoes -> eight same-direction turns -> stopped; never advances\n";
  reset(RECOVERS);run(10000);
  assert(armed && turnStarts==2 && forwardTicks>0 && pilotStage==0 && !pilotSurvey);
  std::cout<<"PASS: missing echoes -> two turns -> fresh clear front -> forward travel\n";
  reset(DEAD_END);run(8000);
  assert(turnStarts==2 && forwardTicks>0);
  std::cout<<"PASS: measured dead end -> retained-direction pivots -> forward exit\n";
  reset(CLOSE_CORNER);run(13000);
  assert(armed && turnStarts==0 && forwardTicks==0);
  std::cout<<"PASS: close corner vetoes unknown-heading rotation\n";
  reset(CLOSE_DURING_TURN);run(10000);
  assert(turnStarts==1 && forwardTicks==0 && outputR==0);
  std::cout<<"PASS: a fresh close echo interrupts a turn\n";
  reset(MISSING,false);run(10000);
  assert(turnStarts==0 && forwardTicks==0);
  std::cout<<"PASS: disabled recovery remains stopped with missing echoes\n";
  reset(OPEN);run(4000);
  assert(forwardTicks>0 && turnStarts==0);
  std::cout<<"PASS: clear front advances without unnecessary scans\n";
  reset(RECOVERS,true,UINT32_MAX-200);run(10000);
  assert(turnStarts==2 && forwardTicks>0);
  std::cout<<"PASS: recovery also crosses the millis() wrap boundary\n";
}
