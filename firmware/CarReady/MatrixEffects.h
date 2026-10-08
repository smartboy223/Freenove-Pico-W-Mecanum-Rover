#pragma once
#include "MatrixPatterns.h"

// One timeline for the display, RGB LEDs and musical beat. No motor commands.
inline uint8_t partyBeat(uint32_t elapsed){return uint8_t((elapsed/400)%8);}
inline int partyPitch(uint32_t elapsed){static const int notes[]={523,659,784,1047,784,659,587,784};return notes[partyBeat(elapsed)];}
inline MatrixFace partyFace(uint32_t elapsed){static const MatrixFace faces[]={MatrixFace::HAPPY,MatrixFace::HEART,MatrixFace::WINK,MatrixFace::COOL,MatrixFace::SURPRISED,MatrixFace::HEART,MatrixFace::HAPPY,MatrixFace::WINK};return faces[partyBeat(elapsed)];}
inline uint8_t breatheLevel(uint32_t elapsed){int level=int((elapsed/8)%510);return uint8_t(level>255 ? 510-level : level);}
inline uint32_t expressionColor(MatrixFace face){
  return face==MatrixFace::ANGRY ? 0xff1800 : face==MatrixFace::SAD ? 0x0055ff : face==MatrixFace::HEART ? 0xff0080 : face==MatrixFace::SLEEPY ? 0x502080 : face==MatrixFace::COOL ? 0x00ffff : 0x30ff60;
}
inline int expressionPitch(MatrixFace face){return face==MatrixFace::ANGRY ? 440 : face==MatrixFace::SAD ? 587 : face==MatrixFace::HEART ? 1319 : face==MatrixFace::SLEEPY ? 523 : face==MatrixFace::COOL ? 988 : face==MatrixFace::SURPRISED ? 2349 : face==MatrixFace::WINK ? 2093 : 1760;}
struct MatrixEffectState {
  MatrixFace selected=MatrixFace::AUTO;
  const char *led="auto",*alert="";
  int wheels[4]={};
  uint32_t now=0,effectStarted=0,melodyStarted=0,alertStarted=0;
  bool warning=false,melody=false,tone=false,expression=false;
  int brightness=4;
};
struct MatrixEffectPlan {
  MatrixFace face=MatrixFace::EYES;
  const char *source="idle";
  uint32_t time=0;
  int brightness=4;
  enum Render {FACE,MUSIC,CHASE} render=FACE;
  bool blank=false;
};
inline MatrixEffectPlan matrixEffectPlan(const MatrixEffectState &s){
  MatrixEffectPlan p;p.time=s.now;p.brightness=s.brightness;
  uint32_t elapsed=s.now-s.effectStarted;
  if(s.selected==MatrixFace::OFF){p.face=MatrixFace::OFF;p.source="off";return p;}
  if(s.warning){
    p.source="alert";p.time=s.now-s.alertStarted;
    p.face=!std::strcmp(s.alert,"battery") ? MatrixFace::SAD : !std::strcmp(s.alert,"finished") ? MatrixFace::HAPPY : !std::strcmp(s.alert,"sensor") || !std::strcmp(s.alert,"timeout") ? MatrixFace::SURPRISED : MatrixFace::ANGRY;
    p.blank=p.time%240>=150;return p;
  }
  if(!std::strcmp(s.led,"party")){
    p.face=partyFace(elapsed);p.source="party";p.time=elapsed;
    p.brightness=elapsed%400<150 ? s.brightness : (s.brightness+1)/2;return p;
  }
  if(s.melody){p.face=MatrixFace::HAPPY;p.source="melody";p.time=s.now-s.melodyStarted;p.render=MatrixEffectPlan::MUSIC;return p;}
  if(s.expression){p.face=s.selected;p.source="expression";return p;}
  if(s.tone){p.face=MatrixFace::SURPRISED;p.source="tone";p.render=MatrixEffectPlan::MUSIC;return p;}
  if(s.selected!=MatrixFace::AUTO){p.face=s.selected;p.source="selected";return p;}
  p.face=matrixDriveFace(s.wheels);
  if(p.face!=MatrixFace::EYES){p.source="movement";return p;}
  p.time=elapsed;
  if(!std::strcmp(s.led,"off")){p.face=MatrixFace::OFF;p.source="lights off";}
  else if(!std::strcmp(s.led,"chase")){p.face=MatrixFace::WINK;p.source="running lights";p.render=MatrixEffectPlan::CHASE;}
  else if(!std::strcmp(s.led,"breathe")){p.face=MatrixFace::HEART;p.source="breathing";p.brightness=1+((s.brightness-1)*breatheLevel(elapsed)+127)/255;}
  else if(!std::strcmp(s.led,"rainbow")){p.face=partyFace(elapsed);p.source="rainbow";}
  else if(std::strcmp(s.led,"auto")){
    p.source="color";p.face=!std::strcmp(s.led,"red") ? MatrixFace::ANGRY : !std::strcmp(s.led,"blue") ? MatrixFace::COOL : !std::strcmp(s.led,"purple") ? MatrixFace::HEART : !std::strcmp(s.led,"cyan") ? MatrixFace::WINK : !std::strcmp(s.led,"white") ? MatrixFace::SURPRISED : MatrixFace::HAPPY;
  }
  return p;
}
inline void matrixEffectFrame(const MatrixEffectPlan &p,uint16_t rows[8]){
  matrixFrame(p.blank ? MatrixFace::OFF : p.face,p.time,rows);
  if(p.blank)return;
  if(p.render==MatrixEffectPlan::MUSIC){
    for(int y=0;y<8;y++)rows[y]=0;
    // Eight bars stay inside the full 16x8 area; the chime changes every note.
    for(int bar=0;bar<8;bar++){int height=1+int((p.time/100+bar*3)%7);for(int y=8-height;y<8;y++)matrixPixel(rows,bar*2,y);}
  }else if(p.render==MatrixEffectPlan::CHASE){
    for(int y=0;y<8;y++)rows[y]=0;
    int x=int((p.time/100)%8);
    for(int tile=0;tile<2;tile++){matrixLine(rows,tile*8+x,1,tile*8+x,6);matrixPixel(rows,tile*8+(x+7)%8,4);}
  }
}
