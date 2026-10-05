#pragma once
enum class LightIntent {WAIT,STRAIGHT,BEAR_LEFT,BEAR_RIGHT,TURN_LEFT,TURN_RIGHT};
struct LightMotion {int forward,rotation;LightIntent intent;};
constexpr int lightReleaseThreshold(int threshold){return threshold*2/3>1 ? threshold*2/3 : 1;}
constexpr LightMotion chooseLightMotion(int left,int right,int threshold,int speed) {
  left=left>0 ? left : 0;right=right>0 ? right : 0;
  int peak=left>right ? left : right;
  if(peak<threshold)return {0,0,LightIntent::WAIT};
  int limit=speed<25 ? speed : 25,total=left+right,difference=right-left;
  int error=difference<0 ? -difference : difference;
  int deadband=total/10>2 ? total/10 : 2;
  if(error<=deadband)return {limit,0,LightIntent::STRAIGHT};
  int turnBoundary=total/3>3 ? total/3 : 3;
  if(error>=turnBoundary)return {0,difference<0 ? -limit : limit,difference<0 ? LightIntent::TURN_LEFT : LightIntent::TURN_RIGHT};
  int rotation=difference*8/total;
  if(!rotation)rotation=difference<0 ? -1 : 1;
  return {limit,rotation,difference<0 ? LightIntent::BEAR_LEFT : LightIntent::BEAR_RIGHT};
}
