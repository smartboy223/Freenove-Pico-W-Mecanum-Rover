#include "../firmware/CarReady/RoamPolicy.h"
#include "../firmware/CarReady/EchoPolicy.h"
constexpr float recovered[]={50,-1,51,52,50};
constexpr float latestMiss[]={50,51,50,52,-1};
constexpr float sparse[]={-1,-1,50,51,-1};
constexpr float uncertain[]={30,60,90,120,150};
constexpr float closeObstacle[]={60,61,62,60,10};
constexpr float highOutlier[]={60,61,62,60,280};
static_assert(stableEcho(recovered,5,4)==50,"Recover from one old miss using current stable evidence");
static_assert(stableEcho(latestMiss,5,4)<0,"A current miss stops immediately");
static_assert(stableEcho(sparse,4,3)<0,"Two echoes are insufficient");
static_assert(stableEcho(uncertain,5,4)<0,"Scattered echoes must not enable movement");
static_assert(stableEcho(closeObstacle,5,4)==10,"A new close obstacle overrides old clear echoes");
static_assert(stableEcho(highOutlier,5,4)==60,"A far outlier must not inflate clearance");
// Decision regressions for measured clearance, escape progress and uncertainty.
static_assert(chooseRoam(28,60,31,0,0,true)==RoamChoice::FORWARD,"Open front should advance");
static_assert(chooseRoam(60,30,28,0,0,true)==RoamChoice::LEFT,"Measured left exit");
static_assert(chooseRoam(28,30,60,0,0,true)==RoamChoice::RIGHT,"Measured right exit");
static_assert(chooseRoam(35,30,25,0,0,true)==RoamChoice::ESCAPE_LEFT,"Dead end should pivot to inspect a new heading");
static_assert(chooseRoam(25,30,35,0,0,true)==RoamChoice::ESCAPE_RIGHT,"Opposite escape direction");
static_assert(chooseRoam(35,30,25,2,1,true)==RoamChoice::ESCAPE_RIGHT,"Retain escape direction instead of oscillating");
static_assert(chooseRoam(25,30,35,2,-1,true)==RoamChoice::ESCAPE_LEFT,"Retain left escape direction");
static_assert(chooseRoam(-1,30,35,0,0,true)==RoamChoice::ESCAPE_RIGHT,"Inspect new heading toward a measured side, without treating the unknown side as clear");
static_assert(chooseRoam(25,-1,35,0,0,true)==RoamChoice::SURVEY_RIGHT,"Missing center echo triggers inspection of another heading");
static_assert(chooseRoam(25,30,-1,0,0,true)==RoamChoice::ESCAPE_LEFT,"Measured left clearance permits bounded inspection");
static_assert(chooseRoam(-1,30,-1,0,0,true)==RoamChoice::SURVEY_RIGHT,"Unknown sides trigger bounded inspection instead of scan lock");
static_assert(chooseRoam(60,30,-1,0,0,true)==RoamChoice::LEFT,"Do not ignore a measured exit because the opposite sector missed");
static_assert(chooseRoam(15,30,60,0,0,true)==RoamChoice::TOO_CLOSE,"Known near corner still vetoes a turn");
#include "../firmware/CarReady/LinePolicy.h"
static_assert(lineSteering(0)==0 && lineSteering(2)==0 && lineSteering(5)==0,"Vendor straight patterns");
static_assert(lineSteering(1)>0 && lineSteering(3)>0,"Right corrections");
static_assert(lineSteering(4)<0 && lineSteering(6)<0,"Left corrections");
static_assert(lineSteering(7)==99,"All black or no floor stops");
static_assert(chooseRoam(17.9f,30,60,0,0,true)==RoamChoice::TOO_CLOSE,"Close corner blocks rotation even with an exit");
static_assert(chooseRoam(25,17.9f,35,0,0,true)==RoamChoice::TOO_CLOSE,"Too close in front");
static_assert(chooseRoam(18,18,18,0,0,true)==RoamChoice::ESCAPE_RIGHT,"Clearance boundary");
static_assert(chooseRoam(35,30,25,7,-1,true)==RoamChoice::ESCAPE_LEFT,"Eighth turn allowed");
static_assert(chooseRoam(35,30,25,8,-1,true)==RoamChoice::EXHAUSTED,"No ninth turn");
static_assert(chooseRoam(60,30,25,8,-1,true)==RoamChoice::EXHAUSTED,"Bound failed clear-side turns too");
static_assert(chooseRoam(35,60,25,8,-1,true)==RoamChoice::FORWARD,"An actual forward exit ends the dead end");
static_assert(chooseRoam(35,30,25,0,0,false)==RoamChoice::EXHAUSTED,"Respect disabled escape option");
static_assert(chooseRoam(60,30,25,0,0,false)==RoamChoice::LEFT,"Normal measured exits remain available");
static_assert(chooseRoam(-1,-1,-1,0,0,true)==RoamChoice::SURVEY_RIGHT,"All missing echoes must not freeze the scan");
static_assert(chooseRoam(-1,-1,-1,2,-1,true)==RoamChoice::SURVEY_LEFT,"Keep inspecting the same direction toward the other side");
static_assert(chooseRoam(-1,-1,-1,8,1,true)==RoamChoice::EXHAUSTED,"Persistent missing echoes stop at the limit");
static_assert(chooseRoam(-1,-1,-1,0,0,false)==RoamChoice::UNKNOWN,"Disabled recovery never turns into unmeasured space");
static_assert(chooseRoam(10,-1,-1,0,0,true)==RoamChoice::TOO_CLOSE,"Known close echo vetoes unknown-heading inspection");
