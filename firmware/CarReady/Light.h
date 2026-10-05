#pragma once
// Shared controller: sensor confirmation, target steering and front clearance.
void lightTick() {
  int left=max(0,lightAdc[0]-lightBaseline[0]),right=max(0,lightAdc[1]-lightBaseline[1]);
  int threshold=lightTargetSeen ? lightReleaseThreshold(lightThreshold) : lightThreshold;
  if(uint32_t(millis()-telemetryAt)>300){haltMotion();lightAction="Waiting for fresh light readings";notice=lightAction;return;}
  if(lightSampleAt!=telemetryAt) {
    lightSampleAt=telemetryAt;
    if(max(left,right)>=threshold){lightConfirmSamples=min(2,int(lightConfirmSamples)+1);if(lightConfirmSamples>=2)lightTargetSeen=true;}
    else {lightConfirmSamples=0;lightTargetSeen=false;}
  }
  if(!lightTargetSeen) {
    haltMotion();
    lightAction=lightAdc[0]+lightThreshold<lightBaseline[0] && lightAdc[1]+lightThreshold<lightBaseline[1] ? "Baseline too bright: set it with flashlight off" : lightConfirmSamples ? "Checking light target" : "Waiting for a brighter light target";
    notice=lightAction;return;
  }
  LightMotion motion=chooseLightMotion(left,right,lightReleaseThreshold(lightThreshold),speedLimit);
  if(motion.intent==LightIntent::WAIT){haltMotion();lightAction="Waiting for a brighter light target";notice=lightAction;return;}
  if(!motion.forward && motion.rotation) {
    float front=filteredEcho();
    if(matrixPresent || headAngle!=90 || front<28 || rawNear(18)) {
      haltMotion();lightAction=front<0 ? "Light found: waiting for reliable front clearance" : "Light found: too close to turn";notice=lightAction;
      if(headAngle==90 && !matrixPresent)rangeWarning(front>=0,28);return;
    }
  }
  if(guardedMotion(0,motion.forward,motion.rotation)) {
    lightAction=motion.intent==LightIntent::TURN_LEFT ? "Turning left toward light" : motion.intent==LightIntent::TURN_RIGHT ? "Turning right toward light" : motion.intent==LightIntent::BEAR_LEFT ? "Following light: bearing left" : motion.intent==LightIntent::BEAR_RIGHT ? "Following light: bearing right" : "Light aligned: driving forward";
    notice=lightAction;
  }else lightAction=notice;
}
