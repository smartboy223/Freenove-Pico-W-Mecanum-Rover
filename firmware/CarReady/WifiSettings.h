#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
inline bool wifiCredentialsValid(const char *ssid,size_t nameLength,const char *key,size_t keyLength){
  if(!nameLength || nameLength>32 || keyLength>64)return false;
  for(size_t i=0;i<nameLength;i++)if(uint8_t(ssid[i])<32 || uint8_t(ssid[i])==127)return false;
  if(!keyLength)return true; // Explicit open-network choice is checked by the form handler.
  if(keyLength<8)return false;
  for(size_t i=0;i<keyLength;i++){
    unsigned char c=key[i];if(c<32 || c==127)return false;
    if(keyLength==64 && !((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
  }
  return true;
}
inline int wifiHex(char c){return c>='0'&&c<='9' ? c-'0' : c>='a'&&c<='f' ? c-'a'+10 : c>='A'&&c<='F' ? c-'A'+10 : -1;}
inline bool wifiFormDecode(const std::string &encoded,std::string &out){
  out.clear();
  for(size_t i=0;i<encoded.size();i++){
    char c=encoded[i];
    if(c=='+')c=' ';
    else if(c=='%'){
      if(i+2>=encoded.size())return false;
      int a=wifiHex(encoded[i+1]),b=wifiHex(encoded[i+2]);if(a<0||b<0)return false;
      c=char(a*16+b);i+=2;
    }
    if(!c)return false;out+=c;
  }
  return true;
}
