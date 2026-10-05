#pragma once
// Shared by the live firmware and host state-machine tests.
void beginPilotScan(bool explore=false) {
  haltMotion();pilotHold=false;pilotSurvey=false;pilotExploring=explore;for(float &v:pilotScan)v=-1;
  setHead(scanAngle(0));pilotStage=1;pilotTime=millis();pilotAction="Finding a new route";
}
void finishPilotEscape() {
  int turns=pilotTurns,retreats=pilotRetreats;fullStop();pilotTurns=turns;pilotRetreats=retreats;
  notice="Roaming stopped: recovery limit reached; check sensor or reposition";
  pilotAction="Recovery limit reached";alert("blocked",3);
}
bool beginPilotRetreat() {
  uint32_t budget=retreatBudget(pilotForwardCredit,uint32_t(millis()-pilotForwardAt),pilotRetreats,backtrackEnabled && escapeEnabled);
  if(!budget || pilotCenter<2 || pilotCenter>=ROAM_RESUME_CM)return false;
  haltMotion();pilotRetreatMs=budget;pilotForwardCredit-=budget*2;pilotRetreats++;
  pilotRetreatFront=pilotCenter;pilotSurvey=false;pilotHold=false;setHead(90);
  pilotTime=millis();pilotStage=9;pilotAction="Backing along recent path";
  notice="Short retreat along recently traveled space, then rescan";alert("turn",1);return true;
}
void beginPilotTurn(RoamChoice choice) {
  haltMotion();pilotHold=false;pilotTurnStarted=false;
  pilotSurvey=choice==RoamChoice::SURVEY_LEFT || choice==RoamChoice::SURVEY_RIGHT;
  pilotEscape=choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::ESCAPE_RIGHT;
  pilotDirection=choice==RoamChoice::LEFT || choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::SURVEY_LEFT ? -1 : 1;
  int target=pilotDirection<0 ? (pilotScan[0]>pilotScan[1] ? 0 : 1) : (pilotScan[4]>pilotScan[3] ? 4 : 3);
  setHead(scanAngle(target));pilotForwardCredit=0;pilotForwardAt=0;
  pilotTurns++;pilotStage=6;pilotTime=millis();
  pilotAction=pilotSurvey ? "Turning to inspect unknown heading" : pilotEscape ? "Escape pivot" : "Changing route";
  notice=pilotSurvey ? "Inspect another heading, stop and check" : pilotEscape ? "Dead end: continuing turn to inspect new space" : "Turning toward measured space";
  if(!pilotSurvey)alert("turn",1);
}
void pilotTick() {
  uint32_t now=millis(),elapsed=now-pilotTime,dt=min(uint32_t(now-pilotLastTick),uint32_t(100));pilotLastTick=now;
  if(pilotForwardMoving()){pilotForwardCredit=min(uint32_t(1500),pilotForwardCredit+dt);pilotForwardAt=now;}
  if(pilotStage==0) {
    float front=filteredEcho();bool provisional=rawNear(ROAM_STOP_CM) && !confirmedNear(ROAM_STOP_CM);
    if(front>=ROAM_STOP_CM && !rawNear(ROAM_STOP_CM) && uint32_t(now-lastCruiseScan)<12000) {
      pilotHold=false;
      if(guardedMotion(0,roamSpeed(front,speedLimit),0)){notice="Roaming: moving forward, watching obstacles";pilotAction="Cruising";if(elapsed>=1200){pilotTurns=0;pilotDirection=0;pilotRetreats=0;}}
      return;
    }
    // Brake on the first questionable sample, but do not beep or rescan for an isolated glitch.
    if(front<0 || provisional) {
      haltMotion();if(!pilotHold){pilotHold=true;pilotHoldAt=now;}
      pilotAction="Pausing to verify echo";notice=pilotAction;
      if(front<0)rangeWarning(false);
      if(uint32_t(now-pilotHoldAt)<450)return;
    }else if(front<ROAM_STOP_CM)rangeWarning(true,ROAM_STOP_CM);
    beginPilotScan(front>=ROAM_STOP_CM);
  }else if(pilotStage>=1 && pilotStage<=5) {
    haltMotion();int index=pilotStage-1;notice="Quick route scan";
    if(elapsed<380 || (filteredEcho()<0 && elapsed<900))return;
    pilotScan[index]=filteredEcho();
    if(index<4){setHead(scanAngle(index+1));pilotStage++;pilotTime=now;return;}
    pilotLeft=max(pilotScan[0],pilotScan[1]);pilotCenter=pilotScan[2];pilotRight=max(pilotScan[3],pilotScan[4]);
    RoamChoice choice=chooseRoam(pilotLeft,pilotCenter,pilotRight,pilotTurns,pilotDirection,escapeEnabled);
    float closest=300;for(float v:pilotScan)if(v>=0)closest=min(closest,v);
    if(choice!=RoamChoice::FORWARD && closest<18)choice=RoamChoice::TOO_CLOSE;
    if(pilotExploring && choice==RoamChoice::FORWARD && closest>=28 && max(pilotLeft,pilotRight)>=max(80.0f,pilotCenter*.85f))choice=pilotLeft>pilotRight ? RoamChoice::LEFT : RoamChoice::RIGHT;
    if(choice==RoamChoice::TOO_CLOSE && beginPilotRetreat())return;
    setHead(90);pilotTime=now;
    if(choice==RoamChoice::FORWARD){pilotStage=7;pilotAction="Confirming front path";}
    else if(choice==RoamChoice::LEFT || choice==RoamChoice::RIGHT || choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::ESCAPE_RIGHT || choice==RoamChoice::SURVEY_LEFT || choice==RoamChoice::SURVEY_RIGHT){if(closest==300)alert("sensor",3);beginPilotTurn(choice);}
    else if(choice==RoamChoice::EXHAUSTED)finishPilotEscape();
    else {pilotStage=8;pilotAction=choice==RoamChoice::UNKNOWN ? "Echoes uncertain: retrying" : "Too close to turn; recent path unavailable";notice=pilotAction;if(choice!=RoamChoice::UNKNOWN)alert("blocked",3);}
  }else if(pilotStage==6) {
    float target=filteredEcho();
    if((target>=0 && target<18) || confirmedNear(18)){haltMotion();pilotTurnStarted=false;pilotSurvey=false;pilotStage=8;pilotTime=now;pilotAction="Close echo: turn stopped";notice=pilotAction;alert("blocked",3);return;}
    if(rawNear(18)){haltMotion();pilotAction="Checking isolated close echo";return;}
    if(!pilotTurnStarted && (elapsed<380 || (target<0 && elapsed<900))){haltMotion();return;}
    // Start duration from the end of servo verification, not from the start of the head movement.
    if(!pilotTurnStarted){pilotTurnStarted=true;pilotTurnAt=now;}
    int extra=min(3,max(0,pilotTurns-1))*150;
    uint32_t duration=pilotSurvey ? (pilotTurns>4 ? 600 : 450) : pilotEscape ? 350+extra : 500;
    if(uint32_t(now-pilotTurnAt)<duration)guardedMotion(0,0,pilotDirection*min(speedLimit,25));
    else {haltMotion();pilotTurnStarted=false;clearRadar();pilotLeft=pilotCenter=pilotRight=-1;setHead(90);pilotStage=7;pilotTime=now;pilotAction="Checking new heading";}
  }else if(pilotStage==7) {
    haltMotion();notice="Checking fresh front echoes";
    if(elapsed<380 || (filteredEcho()<0 && elapsed<900))return;
    pilotCenter=filteredEcho();
    if(pilotCenter>=ROAM_RESUME_CM && !rawNear(ROAM_STOP_CM)){pilotSurvey=false;pilotHold=false;pilotStage=0;pilotTime=now;lastCruiseScan=now;pilotAction="Cruising";guardedMotion(0,roamSpeed(pilotCenter,speedLimit),0);notice="Roaming: moving forward, watching obstacles";}
    else if(pilotSurvey && pilotCenter<0 && escapeEnabled) {
      if(pilotTurns>=8)finishPilotEscape();
      else if(pilotTurns%4==0)beginPilotScan();
      else beginPilotTurn(pilotDirection<0 ? RoamChoice::SURVEY_LEFT : RoamChoice::SURVEY_RIGHT);
    }else beginPilotScan();
  }else if(pilotStage==8) {
    haltMotion();if(elapsed>=600){setHead(90);pilotStage=7;pilotTime=now;pilotAction="Rechecking front";}
  }else if(pilotStage==9) {
    if(elapsed<180){haltMotion();return;}
    if(confirmedNear(pilotRetreatFront-3)){haltMotion();beginPilotScan();notice="Retreat stopped: front clearance decreased";alert("blocked",3);return;}
    if(elapsed<180+pilotRetreatMs)guardedMotion(0,-min(speedLimit,18),0);
    else {haltMotion();clearRadar();beginPilotScan();}
  }
}
