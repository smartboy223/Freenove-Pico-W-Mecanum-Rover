#include "../firmware/CarReady/HeadEffects.h"
namespace head_test {
void runTests(){
  MatrixEffectState s;int wheels[4];
  for(int x:{-25,0,25})for(int y:{-25,0,25})for(int r:{-25,0,25}){
    mecanumMix(x,y,r,25,wheels);std::memcpy(s.wheels,wheels,sizeof(wheels));
    auto a=matrixHeadPlan(s,60),b=matrixHeadPlan(s,120);assert(a.angle>=55&&a.angle<=125);assert(a.angle+b.angle==180);
  }
  mecanumMix(-25,0,0,25,s.wheels);assert(matrixHeadPlan(s,60).angle==55);
  mecanumMix(25,0,0,25,s.wheels);assert(matrixHeadPlan(s,60).angle==125);
  mecanumMix(0,25,0,25,s.wheels);assert(matrixHeadPlan(s,60).angle==90);
  mecanumMix(0,-25,0,25,s.wheels);assert(matrixHeadPlan(s,60).angle==90);
  std::cout<<"PASS: matrix head tracks crab, turn and diagonal directions; straight travel stays centered; reverse mounting swaps sides\n";
  std::memset(s.wheels,0,sizeof(s.wheels));s.led="party";
  bool different=false;int previous=-1;
  for(int i=0;i<8;i++){s.now=i*400;auto p=matrixHeadPlan(s,60);assert(p.angle>=70&&p.angle<=110);if(previous>=0&&p.angle!=previous)different=true;previous=p.angle;}
  assert(different);s.selected=MatrixFace::OFF;assert(matrixHeadPlan(s,60).angle==90);
  s.selected=MatrixFace::AUTO;s.warning=true;assert(matrixHeadPlan(s,60).angle==90);
  std::cout<<"PASS: stationary party uses bounded head poses on the shared beat; Off and alerts center the head\n";
}
}
