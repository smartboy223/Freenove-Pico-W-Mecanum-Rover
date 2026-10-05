#pragma once
// Vendor wheel order: M1 front-left, M2 rear-left, M3 front-right, M4 rear-right.
// x positive = crab right, y positive = forward, rotation positive = turn right.
inline void mecanumMix(int x, int y, int rotation, int limit, int out[4]) {
  // Owner's floor test: reverse the longitudinal axis only; crab/turn stay unchanged.
  y=-y;
  out[0]=y+x+rotation; out[1]=y-x+rotation;
  out[2]=y+x-rotation; out[3]=y-x-rotation;
  int largest=0;
  for(int i=0;i<4;i++) {int a=out[i]<0 ? -out[i] : out[i]; if(a>largest) largest=a;}
  if(largest>limit) for(int i=0;i<4;i++) out[i]=out[i]*limit/largest;
}
