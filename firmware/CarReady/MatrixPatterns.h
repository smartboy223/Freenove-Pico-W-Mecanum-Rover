#pragma once
#include <cstdint>
#include <cstring>

enum class MatrixFace : uint8_t {
  AUTO,EYES,HAPPY,HEART,ANGRY,SAD,WINK,SURPRISED,SLEEPY,COOL,PARTY,
  FORWARD,REVERSE,CRAB_LEFT,CRAB_RIGHT,FORWARD_LEFT,FORWARD_RIGHT,
  REVERSE_LEFT,REVERSE_RIGHT,TURN_LEFT,TURN_RIGHT,OFF,COUNT
};
inline const char *matrixFaceName(MatrixFace face) {
  static const char *names[]={"auto","eyes","happy","heart","angry","sad","wink","surprised","sleepy","cool","party","forward","reverse","crab_left","crab_right","forward_left","forward_right","reverse_left","reverse_right","turn_left","turn_right","off"};
  return names[uint8_t(face)<uint8_t(MatrixFace::COUNT) ? uint8_t(face) : 0];
}
inline bool parseMatrixFace(const char *name,MatrixFace &face) {
  for(uint8_t i=0;i<uint8_t(MatrixFace::COUNT);i++)
    if(!std::strcmp(name,matrixFaceName(MatrixFace(i)))){face=MatrixFace(i);return true;}
  return false;
}
inline MatrixFace matrixDriveFace(const int wheels[4]) {
  int x=wheels[0]-wheels[1]+wheels[2]-wheels[3];
  int y=-(wheels[0]+wheels[1]+wheels[2]+wheels[3]);
  int r=wheels[0]+wheels[1]-wheels[2]-wheels[3];
  if(r && !x && !y)return r>0 ? MatrixFace::TURN_RIGHT : MatrixFace::TURN_LEFT;
  if(y>0)return x<0 ? MatrixFace::FORWARD_LEFT : x>0 ? MatrixFace::FORWARD_RIGHT : MatrixFace::FORWARD;
  if(y<0)return x<0 ? MatrixFace::REVERSE_LEFT : x>0 ? MatrixFace::REVERSE_RIGHT : MatrixFace::REVERSE;
  if(x)return x>0 ? MatrixFace::CRAB_RIGHT : MatrixFace::CRAB_LEFT;
  return MatrixFace::EYES;
}
inline void matrixPixel(uint16_t rows[8],int x,int y) {
  if(x>=0 && x<16 && y>=0 && y<8)rows[y]|=uint16_t(1u<<x);
}
// The module is two separately wired 8x8 panels. Rotate within each panel,
// preserving the left/right halves instead of clipping a 16x8 quarter-turn.
inline void matrixPanelRotate(const uint16_t rows[8],int rotation,uint16_t out[8]) {
  for(int y=0;y<8;y++)out[y]=0;
  for(int y=0;y<8;y++)for(int x=0;x<16;x++)if(rows[y]&(1u<<x)) {
    int local=x%8,rx=local,ry=y;
    if(rotation==90){rx=7-y;ry=local;}
    else if(rotation==180){rx=7-local;ry=7-y;}
    else if(rotation==270){rx=y;ry=7-local;}
    matrixPixel(out,(x/8)*8+rx,ry);
  }
}
inline void matrixLine(uint16_t rows[8],int x0,int y0,int x1,int y1) {
  int dx=x1>x0 ? x1-x0 : x0-x1,sx=x0<x1 ? 1 : -1;
  int dy=y1>y0 ? y0-y1 : y1-y0,sy=y0<y1 ? 1 : -1,error=dx+dy;
  for(;;){matrixPixel(rows,x0,y0);if(x0==x1 && y0==y1)break;int twice=2*error;if(twice>=dy){error+=dy;x0+=sx;}if(twice<=dx){error+=dx;y0+=sy;}}
}
inline void matrixArrow(uint16_t rows[8],int x,int y) {
  int tipX=x<0 ? 1 : x>0 ? 14 : 7,tipY=y<0 ? 0 : y>0 ? 7 : 3;
  int tailX=x ? 15-tipX : tipX,tailY=y ? 7-tipY : tipY;
  matrixLine(rows,tailX,tailY,tipX,tipY);
  if(!y){matrixLine(rows,tipX,tipY,tipX-x*4,tipY-2);matrixLine(rows,tipX,tipY,tipX-x*4,tipY+2);}
  else if(!x){matrixLine(rows,tipX,tipY,tipX-3,tipY-y*2);matrixLine(rows,tipX,tipY,tipX+3,tipY-y*2);}
  else {matrixLine(rows,tipX,tipY,tipX-x*4,tipY);matrixLine(rows,tipX,tipY,tipX,tipY-y*3);}
}
inline void matrixFrame(MatrixFace face,uint32_t time,uint16_t rows[8]) {
  for(int y=0;y<8;y++)rows[y]=0;
  if(face==MatrixFace::OFF)return;
  if(face==MatrixFace::AUTO)face=MatrixFace::EYES;
  if(face==MatrixFace::PARTY){static const MatrixFace frames[]={MatrixFace::HAPPY,MatrixFace::HEART,MatrixFace::WINK,MatrixFace::COOL};face=frames[(time/700)%4];}
  if(face>=MatrixFace::FORWARD && face<=MatrixFace::REVERSE_RIGHT) {
    int x=face==MatrixFace::CRAB_LEFT || face==MatrixFace::FORWARD_LEFT || face==MatrixFace::REVERSE_LEFT ? -1 : face==MatrixFace::CRAB_RIGHT || face==MatrixFace::FORWARD_RIGHT || face==MatrixFace::REVERSE_RIGHT ? 1 : 0;
    int y=face==MatrixFace::FORWARD || face==MatrixFace::FORWARD_LEFT || face==MatrixFace::FORWARD_RIGHT ? -1 : face==MatrixFace::REVERSE || face==MatrixFace::REVERSE_LEFT || face==MatrixFace::REVERSE_RIGHT ? 1 : 0;
    matrixArrow(rows,x,y);return;
  }
  if(face==MatrixFace::TURN_LEFT || face==MatrixFace::TURN_RIGHT){int sign=face==MatrixFace::TURN_LEFT ? -1 : 1;matrixArrow(rows,sign,0);matrixLine(rows,sign<0 ? 12 : 3,3,sign<0 ? 12 : 3,6);matrixLine(rows,3,6,12,6);return;}
  if(face==MatrixFace::HEART){const uint16_t heart[]={0x0660,0x0ff0,0x1ff8,0x1ff8,0x0ff0,0x07e0,0x03c0,0x0180};for(int y=0;y<8;y++)rows[y]=heart[y];if((time/450)%2){rows[0]=0x0240;rows[2]=rows[3]=0x0ff0;}return;}
  if(face==MatrixFace::EYES){bool blink=time%4000>=3750;if(blink){matrixLine(rows,2,3,5,3);matrixLine(rows,10,3,13,3);}else {for(int y=1;y<=5;y++){matrixPixel(rows,2,y);matrixPixel(rows,5,y);matrixPixel(rows,10,y);matrixPixel(rows,13,y);}matrixLine(rows,3,0,4,0);matrixLine(rows,3,6,4,6);matrixLine(rows,11,0,12,0);matrixLine(rows,11,6,12,6);}return;}
  if(face==MatrixFace::SLEEPY){matrixLine(rows,2,2,5,2);matrixLine(rows,10,2,13,2);matrixLine(rows,6,6,9,6);matrixPixel(rows,14,0);return;}
  if(face==MatrixFace::COOL){matrixLine(rows,1,1,14,1);for(int y=2;y<=3;y++){matrixLine(rows,2,y,5,y);matrixLine(rows,10,y,13,y);}matrixLine(rows,5,6,10,6);matrixPixel(rows,4,5);matrixPixel(rows,11,5);return;}
  if(face==MatrixFace::ANGRY){matrixLine(rows,2,0,5,2);matrixLine(rows,13,0,10,2);matrixPixel(rows,4,3);matrixPixel(rows,11,3);matrixLine(rows,6,5,9,5);matrixPixel(rows,5,6);matrixPixel(rows,10,6);return;}
  matrixPixel(rows,4,1);matrixPixel(rows,4,2);
  if(face==MatrixFace::WINK)matrixLine(rows,10,2,13,2);else {matrixPixel(rows,11,1);matrixPixel(rows,11,2);}
  if(face==MatrixFace::SURPRISED){matrixLine(rows,7,4,8,4);matrixLine(rows,7,7,8,7);matrixLine(rows,6,5,6,6);matrixLine(rows,9,5,9,6);}
  else if(face==MatrixFace::SAD){matrixLine(rows,6,5,9,5);matrixPixel(rows,5,6);matrixPixel(rows,10,6);matrixPixel(rows,12,4);}
  else {matrixLine(rows,6,6,9,6);matrixPixel(rows,5,5);matrixPixel(rows,10,5);matrixPixel(rows,4,4);matrixPixel(rows,11,4);}
}
