#pragma once
// Inspect a new heading by a bounded pivot, then permit guarded forward travel.
enum class RoamChoice {FORWARD,LEFT,RIGHT,ESCAPE_LEFT,ESCAPE_RIGHT,SURVEY_LEFT,SURVEY_RIGHT,UNKNOWN,TOO_CLOSE,EXHAUSTED};
constexpr int surveyDirection(float left,float right,int previous) {
  return previous ? previous : left>right ? -1 : 1;
}
constexpr RoamChoice chooseRoam(float left,float center,float right,int turns,int escapeDirection,bool escapeEnabled) {
  if(center>=55)return RoamChoice::FORWARD;
  if((left>=0 && left<18) || (center>=0 && center<28) || (right>=0 && right<18))return RoamChoice::TOO_CLOSE;
  if(turns>=8)return RoamChoice::EXHAUSTED;
  // Missing echoes are not a clear route. Inspect a different heading with a bounded turn.
  if(center<0 || (left<0 && right<0)) {
    if(!escapeEnabled)return RoamChoice::UNKNOWN;
    return surveyDirection(left,right,escapeDirection)<0 ? RoamChoice::SURVEY_LEFT : RoamChoice::SURVEY_RIGHT;
  }
  if(left>=55 || right>=55)return left>right ? RoamChoice::LEFT : RoamChoice::RIGHT;
  if(!escapeEnabled)return RoamChoice::EXHAUSTED;
  int direction=escapeDirection;
  if(left<0)direction=1;else if(right<0)direction=-1;else if(!direction)direction=left>right+8 ? -1 : 1;
  return direction<0 ? RoamChoice::ESCAPE_LEFT : RoamChoice::ESCAPE_RIGHT;
}
