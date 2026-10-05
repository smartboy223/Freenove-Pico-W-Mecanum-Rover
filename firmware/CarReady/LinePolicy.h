#pragma once
// Correction sign follows the user's verified left/right rotation convention.
constexpr int lineSteering(int bits) {
  return bits==0 || bits==2 || bits==5 ? 0 : bits==6 ? -6 : bits==3 ? 6 : bits==4 ? -12 : bits==1 ? 12 : 99;
}
