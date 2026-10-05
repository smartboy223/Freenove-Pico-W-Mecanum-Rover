#pragma once
// Shared by the live firmware and host state-machine tests.
void beginPilotScan(bool explore=false) {
  haltMotion();pilotSurvey=false;pilotExploring=explore;for(float &v:pilotScan)v=-1;
  setHead(scanAngle(0));pilotStage=1;pilotTime=millis();pilotAction="Finding a new route";
}
void finishPilotEscape() {
  int turns=pilotTurns;fullStop();pilotTurns=turns;
  notice="Roaming stopped: eight headings tried; check sensor or reposition";
  pilotAction="Recovery limit reached";alert("blocked",3);
}
void beginPilotTurn(RoamChoice choice) {
  haltMotion();
  pilotSurvey=choice==RoamChoice::SURVEY_LEFT || choice==RoamChoice::SURVEY_RIGHT;
  pilotEscape=choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::ESCAPE_RIGHT;
  pilotDirection=choice==RoamChoice::LEFT || choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::SURVEY_LEFT ? -1 : 1;
  pilotTurns++;pilotStage=6;pilotTime=millis();
  pilotAction=pilotSurvey ? "Turning to inspect unknown heading" : pilotEscape ? "Escape pivot" : "Changing route";
  notice=pilotSurvey ? "No usable route echo: turn, stop and check a new heading" : pilotEscape ? "Dead end: pivoting to inspect a new heading" : "Turning toward measured space";
  alert("turn",1);
}
void pilotTick() {
  uint32_t elapsed=millis()-pilotTime;
  if(pilotStage==0) {
    float front=filteredEcho();
    if(front>=35 && uint32_t(millis()-lastCruiseScan)<12000) {
      if(guardedMotion(0,min(speedLimit,28),0)){notice="Roaming: moving forward, watching obstacles";pilotAction="Cruising";if(elapsed>=1200){pilotTurns=0;pilotDirection=0;}}
      return;
    }
    if(front<35)alert(front<0 ? "sensor" : "obstacle",front<0 ? 3 : 2);
    beginPilotScan(front>=35);
  }else if(pilotStage>=1 && pilotStage<=5) {
    haltMotion();int index=pilotStage-1;notice="Quick route scan";
    if(elapsed<380 || (filteredEcho()<0 && elapsed<900))return;
    pilotScan[index]=filteredEcho();
    if(index<4){setHead(scanAngle(index+1));pilotStage++;pilotTime=millis();return;}
    pilotLeft=max(pilotScan[0],pilotScan[1]);pilotCenter=pilotScan[2];pilotRight=max(pilotScan[3],pilotScan[4]);
    RoamChoice choice=chooseRoam(pilotLeft,pilotCenter,pilotRight,pilotTurns,pilotDirection,escapeEnabled);
    // A known close corner vetoes pivots, even when a farther angle looks clear.
    float closest=300;for(float v:pilotScan)if(v>=0)closest=min(closest,v);
    if(choice!=RoamChoice::FORWARD && closest<18)choice=RoamChoice::TOO_CLOSE;
    if(pilotExploring && choice==RoamChoice::FORWARD && closest>=18 && max(pilotLeft,pilotRight)>=max(80.0f,pilotCenter*.85f))choice=pilotLeft>pilotRight ? RoamChoice::LEFT : RoamChoice::RIGHT;
    setHead(90);pilotTime=millis();
    if(choice==RoamChoice::FORWARD){pilotStage=7;pilotAction="Confirming front path";}
    else if(choice==RoamChoice::LEFT || choice==RoamChoice::RIGHT || choice==RoamChoice::ESCAPE_LEFT || choice==RoamChoice::ESCAPE_RIGHT || choice==RoamChoice::SURVEY_LEFT || choice==RoamChoice::SURVEY_RIGHT)beginPilotTurn(choice);
    else if(choice==RoamChoice::EXHAUSTED)finishPilotEscape();
    else {pilotStage=8;pilotAction=choice==RoamChoice::UNKNOWN ? "Echoes uncertain: retrying" : "Too close to pivot";notice=pilotAction;alert(choice==RoamChoice::UNKNOWN ? "sensor" : "blocked",3);}
  }else if(pilotStage==6) {
    float front=filteredEcho();
    if(front>=0 && front<18){haltMotion();pilotSurvey=false;pilotStage=8;pilotTime=millis();pilotAction="Close echo: turn stopped";notice=pilotAction;alert("blocked",3);return;}
    if(elapsed<180){haltMotion();return;}
    if(elapsed<180+(pilotSurvey ? 450 : pilotEscape ? 350 : 500))guardedMotion(0,0,pilotDirection*min(speedLimit,25));
    else {haltMotion();clearRadar();pilotLeft=pilotCenter=pilotRight=-1;setHead(90);pilotStage=7;pilotTime=millis();pilotAction="Checking new heading";}
  }else if(pilotStage==7) {
    haltMotion();notice="Checking fresh front echoes";
    if(elapsed<380 || (filteredEcho()<0 && elapsed<900))return;
    pilotCenter=filteredEcho();
    if(pilotCenter>=35){pilotSurvey=false;pilotStage=0;pilotTime=millis();lastCruiseScan=millis();pilotAction="Cruising";guardedMotion(0,min(speedLimit,28),0);notice="Roaming: moving forward, watching obstacles";}
    else if(pilotSurvey && pilotCenter<0 && escapeEnabled) {
      // Keep inspecting the other side rather than repeating a full blind scan after every small turn.
      if(pilotTurns>=8)finishPilotEscape();
      else if(pilotTurns%4==0)beginPilotScan();
      else beginPilotTurn(pilotDirection<0 ? RoamChoice::SURVEY_LEFT : RoamChoice::SURVEY_RIGHT);
    }
    else beginPilotScan();
  }else if(pilotStage==8) {
    haltMotion();if(elapsed>=350){setHead(90);pilotStage=7;pilotTime=millis();pilotAction="Rechecking front";}
  }
}
