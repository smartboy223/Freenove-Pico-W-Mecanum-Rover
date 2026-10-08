#include "../firmware/CarReady/WifiSettings.h"
namespace wifi_test {
void runTests(){
  assert(wifiJsonString("Lab \"A\" \\ room")=="\"Lab \\\"A\\\" \\\\ room\"");
  assert(wifiJsonString("a\n\tb")=="\"a\\u000a\\u0009b\"");
  assert(wifiJsonString("\xd9\x85")=="\"\xd9\x85\"");
  assert(wifiCredentialsValid("classroom",9,"",0));
  assert(wifiCredentialsValid("lab",3,"eight123",8));
  assert(wifiCredentialsValid("lab",3,std::string(64,'a').c_str(),64));
  assert(!wifiCredentialsValid("",0,"eight123",8));
  assert(!wifiCredentialsValid(std::string(33,'x').c_str(),33,"eight123",8));
  assert(!wifiCredentialsValid("bad\nname",8,"eight123",8));
  assert(!wifiCredentialsValid("lab",3,"short",5));
  assert(!wifiCredentialsValid("lab",3,std::string(64,'z').c_str(),64));
  assert(!wifiCredentialsValid("lab",3,"key\0text",8));
  std::string decoded;
  assert(wifiFormDecode("Lab+2%2B%26%25%22%5C",decoded) && decoded=="Lab 2+&%\"\\");
  assert(wifiFormDecode("%D9%85",decoded) && decoded=="\xd9\x85");
  for(auto text:{"%","%1","%gg","a%00b"})assert(!wifiFormDecode(text,decoded));
  std::cout<<"PASS: Wi-Fi names, open/WPA keys, UTF-8 and special form characters validate; malformed/NUL fields reject\n";
}
}
