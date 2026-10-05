#pragma once
#include <stdint.h>
constexpr int FRONT_STOP_CM=35;
constexpr int ROAM_STOP_CM=45;
constexpr int ROAM_RESUME_CM=55;
constexpr bool warningDue(uint32_t now,uint32_t previous,bool seen,uint32_t cooldown) {
  return !seen || uint32_t(now-previous)>=cooldown;
}
constexpr int roamSpeed(float distance,int limit) {
  int ceiling=distance<70 ? 20 : 25;
  return limit<ceiling ? limit : ceiling;
}
constexpr uint32_t retreatBudget(uint32_t credit,uint32_t age,int attempts,bool enabled) {
  if(!enabled || age>8000 || attempts>=2 || credit<400)return 0;
  return credit/2<350 ? credit/2 : 350;
}
