#include <fstream>
#include "../firmware/CarReady/MatrixPatterns.h"
#include "../firmware/CarReady/MatrixEffects.h"
#include "../firmware/CarReady/Motion.h"
namespace matrix_test {
void runTests() {
  struct Direction {int x,y,r;MatrixFace face;};
  Direction directions[]={
    {0,25,0,MatrixFace::FORWARD},{0,-25,0,MatrixFace::REVERSE},
    {-25,0,0,MatrixFace::CRAB_LEFT},{25,0,0,MatrixFace::CRAB_RIGHT},
    {0,0,-25,MatrixFace::TURN_LEFT},{0,0,25,MatrixFace::TURN_RIGHT},
    {-25,25,0,MatrixFace::FORWARD_LEFT},{25,25,0,MatrixFace::FORWARD_RIGHT},
    {-25,-25,0,MatrixFace::REVERSE_LEFT},{25,-25,0,MatrixFace::REVERSE_RIGHT}
  };
  for(auto d:directions){int wheels[4];mecanumMix(d.x,d.y,d.r,25,wheels);assert(matrixDriveFace(wheels)==d.face);}
  int stopped[4]={};assert(matrixDriveFace(stopped)==MatrixFace::EYES);
  std::cout<<"PASS: matrix movement signs match all ten actual mecanum patterns and stopped state\n";
  MatrixFace face=MatrixFace::HAPPY;
  assert(!parseMatrixFace("invalid",face) && face==MatrixFace::HAPPY);
  for(uint8_t i=0;i<uint8_t(MatrixFace::COUNT);i++){assert(parseMatrixFace(matrixFaceName(MatrixFace(i)),face));assert(face==MatrixFace(i));}
  std::cout<<"PASS: every matrix selection parses; invalid expression cannot change selection\n";
  uint16_t happy[8],sad[8],angry[8],open[8],closed[8],off[8];
  matrixFrame(MatrixFace::HAPPY,0,happy);matrixFrame(MatrixFace::SAD,0,sad);matrixFrame(MatrixFace::ANGRY,0,angry);
  assert(std::memcmp(happy,sad,sizeof(happy)) && std::memcmp(happy,angry,sizeof(happy)));
  matrixFrame(MatrixFace::EYES,0,open);matrixFrame(MatrixFace::EYES,3800,closed);
  assert(std::memcmp(open,closed,sizeof(open)));
  matrixFrame(MatrixFace::OFF,0,off);for(auto row:off)assert(row==0);
  uint16_t party0[8],party1[8];matrixFrame(MatrixFace::PARTY,0,party0);matrixFrame(MatrixFace::PARTY,700,party1);
  assert(std::memcmp(party0,party1,sizeof(party0)));
  std::cout<<"PASS: emotions differ, eyes blink, party animates and Off clears pixels\n";
  for(int rotation:{0,90,180,270}) {
    bool seen[128]={};
    for(int y=0;y<8;y++)for(int x=0;x<16;x++) {
      uint16_t single[8]={},rotated[8],restored[8];matrixPixel(single,x,y);
      matrixPanelRotate(single,rotation,rotated);
      int count=0;
      for(int yy=0;yy<8;yy++)for(int xx=0;xx<16;xx++)if(rotated[yy]&(1u<<xx)) {
        assert(xx/8==x/8 && !seen[yy*16+xx]);seen[yy*16+xx]=true;count++;
        if(rotation==270)assert(xx%8==y && yy==7-x%8);
      }
      assert(count==1);matrixPanelRotate(rotated,(360-rotation)%360,restored);
      assert(!std::memcmp(single,restored,sizeof(single)));
      // Independent reference: Freenove setRow(y+tile*8, 1<<(7-localX))
      // stores the pixel at buffer[localX], bit(y+tile*8).
      if(rotation==270)assert(rotated[7-x%8]==uint16_t(1u<<(y+(x/8)*8)));
    }
  }
  std::cout<<"PASS: all four panel alignments preserve every pixel, panel order and inverse mapping\n";
  MatrixEffectState state;state.effectStarted=0xfffffff0u;state.led="party";
  uint16_t previousParty[8]={};
  for(int beat=0;beat<8;beat++){
    state.now=state.effectStarted+beat*400+20;
    auto plan=matrixEffectPlan(state);assert(plan.face==partyFace(beat*400+20));
    assert(partyBeat(state.now-state.effectStarted)==beat);
    assert(partyPitch(state.now-state.effectStarted)>=523 && partyPitch(state.now-state.effectStarted)<=1047);
    uint16_t frame[8];matrixEffectFrame(plan,frame);
    if(beat)assert(std::memcmp(frame,previousParty,sizeof(frame)));
    std::memcpy(previousParty,frame,sizeof(frame));
    state.now+=200;assert(matrixEffectPlan(state).brightness==2);
  }
  state.warning=true;state.alert="battery";state.alertStarted=state.now;
  assert(matrixEffectPlan(state).face==MatrixFace::SAD);
  state.now+=160;assert(matrixEffectPlan(state).blank);
  state.warning=false;assert(!std::strcmp(matrixEffectPlan(state).source,"party"));
  state.selected=MatrixFace::OFF;assert(matrixEffectPlan(state).face==MatrixFace::OFF);
  std::cout<<"PASS: eight party beats share notes/faces/brightness across clock wrap; alerts interrupt and resume; Off stays dark\n";
  state=MatrixEffectState{};state.selected=MatrixFace::HEART;state.led="chase";
  assert(matrixEffectPlan(state).face==MatrixFace::HEART);
  state.melody=true;assert(matrixEffectPlan(state).render==MatrixEffectPlan::MUSIC);
  state.melody=false;assert(matrixEffectPlan(state).face==MatrixFace::HEART);
  state.expression=true;state.tone=true;assert(!std::strcmp(matrixEffectPlan(state).source,"expression"));
  state.expression=false;assert(!std::strcmp(matrixEffectPlan(state).source,"tone"));
  std::cout<<"PASS: melody and tones temporarily animate; selected expression returns without changing selection\n";
  state=MatrixEffectState{};state.led="chase";uint16_t chase0[8],chase1[8];
  matrixEffectFrame(matrixEffectPlan(state),chase0);state.now=100;matrixEffectFrame(matrixEffectPlan(state),chase1);
  assert(std::memcmp(chase0,chase1,sizeof(chase0)));
  state.led="breathe";
  for(int brightness:{1,4,15})for(int t=0;t<4080;t+=8){state.brightness=brightness;state.now=t;auto p=matrixEffectPlan(state);assert(p.brightness>=1 && p.brightness<=brightness);}
  for(const char *style:{"red","green","blue","yellow","cyan","purple","white","rainbow","off"}){
    state.led=style;assert(std::strcmp(matrixEffectPlan(state).source,"idle"));
    state.wheels[0]=state.wheels[1]=state.wheels[2]=state.wheels[3]=-25;
    assert(matrixEffectPlan(state).face==MatrixFace::FORWARD);for(int &wheel:state.wheels)wheel=0;
  }
  std::cout<<"PASS: all RGB effects reach Interactive matrix; chase animates, breathe respects brightness and movement signs win\n";
  std::ofstream output("build/matrix-patterns.json");output<<"{";
  for(uint8_t i=0;i<uint8_t(MatrixFace::COUNT);i++){uint16_t rows[8];matrixFrame(MatrixFace(i),0,rows);if(i)output<<",";output<<"\""<<matrixFaceName(MatrixFace(i))<<"\":[";for(int y=0;y<8;y++){if(y)output<<",";output<<rows[y];}output<<"]";}
  output<<"}";
}
}
