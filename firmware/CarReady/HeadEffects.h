#pragma once
#include "MatrixEffects.h"
struct MatrixHeadPlan {int angle=90;const char *source="idle";};
inline MatrixHeadPlan matrixHeadPlan(const MatrixEffectState &s,int leftAngle){
  MatrixHeadPlan p;
  MatrixFace movement=matrixDriveFace(s.wheels);
  int side=leftAngle<90 ? -1 : 1;
  if(movement!=MatrixFace::EYES){
    p.source="movement";
    if(movement==MatrixFace::CRAB_LEFT||movement==MatrixFace::TURN_LEFT)p.angle=90+side*35;
    else if(movement==MatrixFace::CRAB_RIGHT||movement==MatrixFace::TURN_RIGHT)p.angle=90-side*35;
    else if(movement==MatrixFace::FORWARD_LEFT||movement==MatrixFace::REVERSE_LEFT)p.angle=90+side*22;
    else if(movement==MatrixFace::FORWARD_RIGHT||movement==MatrixFace::REVERSE_RIGHT)p.angle=90-side*22;
    return p;
  }
  if(s.selected==MatrixFace::OFF)return p;
  if(s.warning){p.source="alert";return p;}
  uint32_t elapsed=s.now-s.effectStarted;
  if(!std::strcmp(s.led,"party")){
    static const int poses[]={-20,-10,0,10,20,10,0,-10};p.angle=90+side*poses[partyBeat(elapsed)];p.source="party";
  }else if(s.melody){p.angle=90+side*(int((s.now-s.melodyStarted)/200)%3-1)*12;p.source="melody";}
  else if(s.expression){p.source="expression";p.angle=90+side*(s.selected==MatrixFace::WINK ? 18 : s.selected==MatrixFace::SAD||s.selected==MatrixFace::SLEEPY ? -12 : s.selected==MatrixFace::ANGRY ? 12 : 0);}
  else if(!std::strcmp(s.led,"chase")||!std::strcmp(s.led,"rainbow")||!std::strcmp(s.led,"breathe")){
    int sweep=int((elapsed/50)%48);p.angle=90+side*(sweep<=24?sweep-12:36-sweep);p.source="lights";
  }
  return p;
}
