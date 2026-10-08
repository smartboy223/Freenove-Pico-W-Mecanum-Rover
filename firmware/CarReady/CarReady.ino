// FNK0089 Pico W mecanum base. Pin map and matrix patterns from Freenove.
#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>
#include <hardware/watchdog.h>
#include <WiFi.h>
#include <pico/cyw43_arch.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <SimpleMDNS.h>
#include "wifi_credentials.h"
#include <Adafruit_NeoPixel.h>
#define NO_LED_FEEDBACK_CODE
#define SEND_PWM_BY_TIMER
#include <IRremote.hpp>
Adafruit_NeoPixel chassisLeds(8,16,NEO_GRB+NEO_KHZ800);
uint32_t lastIr=0, irCount=0;
bool motorTestActive=false;
uint32_t motorTestDeadline=0;

WiFiServer lanServer(80);
DNSServer portalDns;
struct HttpSlot {WiFiClient client;String headers,body,response;size_t sent=0;uint32_t started=0;bool page=false,headersComplete=false;int bodyLength=0;};
HttpSlot httpSlots[4];
uint32_t lastWifiAttempt=0;
uint32_t lastLanStart=0;
bool lanStarted=false;
bool hotspotActive=false,mdnsStarted=false,networkFilesystem=false,homeWasConnected=false;
uint8_t networkPending=0;
uint32_t networkQueuedAt=0,homeDisconnectedAt=0;
#include "NetworkPolicy.h"
#include "WifiSettings.h"
String homeSsid=WIFI_SSID,homePassword=WIFI_PASSWORD,pendingSsid="",pendingPassword="";
String wifiJoinState="idle";
bool wifiPairing=false,homeSettingsSaved=false;
bool homeForgotten=false,wifiScanRunning=false;
bool homeLinkVerified=false;
uint32_t homeVerifyAt=0,homeMatchSince=0;
String homeAssociatedSsid="";
uint32_t wifiScanStarted=0;
String wifiLastResult="idle",homeLastIp="",wifiScanState="idle",wifiScanResults="[]";
String jsonText(const String &value){return String(wifiJsonString(value.c_str()).c_str());}
bool networkReady();
String radioHomeSsid(){
  if(WiFi.status()!=WL_CONNECTED || !WiFi.localIP().isSet())return "";
  struct {uint32_t length;char name[32];} reply={};
  cyw43_arch_lwip_begin();int error=cyw43_ioctl(&cyw43_state,CYW43_IOCTL_GET_SSID,sizeof(reply),reinterpret_cast<uint8_t*>(&reply),CYW43_ITF_STA);cyw43_arch_lwip_end();
  if(error || !reply.length || reply.length>32)return "";
  char name[33]={};memcpy(name,reply.name,reply.length);return String(name);
}
void networkTick();
void queueNetwork(uint8_t choice);
#include "Dashboard.h"

const uint8_t motorPins[4][2] = {{18,19},{21,20},{7,6},{9,8}};
const int motorSigns[4] = {1,1,1,1}; // Change individual signs after wheel calibration.
Servo head;
bool matrixPresent = false, armed = false, moving = false;
float distanceCm = -1;
uint32_t lastDrive = 0, lastSample = 0, lastEyes = 0;
char command[96];
size_t commandLength = 0;
bool overflowed = false;
#include "MatrixPatterns.h"
#include "MatrixEffects.h"
#include "HeadEffects.h"
MatrixFace matrixFace=MatrixFace::AUTO,matrixActive=MatrixFace::EYES;
const char *matrixSource="idle";
int matrixOutputBrightness=4;
uint32_t matrixWrites=0,matrixErrors=0;
uint16_t matrixRows[8]={};
uint16_t matrixWire[8]={};
int matrixBrightness=4,matrixRotation=270; // Correct each tile 90 degrees left for the kit wiring.
uint32_t otaUntil=0;
bool otaListening=false;
void matrixCommand(uint8_t value);
void matrixTick(bool force=false);
bool matrixHeadAuto=true;
uint32_t matrixHeadAt=0,matrixHeadManualUntil=0;
int matrixHeadTarget=90;
const char *matrixHeadSource="idle";
#include "Control.h"
void matrixHeadTick(){
  if(!matrixPresent)return;
  if(!matrixHeadAuto || (matrixHeadManualUntil && int32_t(millis()-matrixHeadManualUntil)<0)){matrixHeadTarget=headAngle;matrixHeadSource="manual";return;}
  MatrixEffectState state;state.selected=matrixFace;state.led=ledStyle.c_str();state.now=millis();state.effectStarted=effectStarted;
  state.melody=melodyActive;state.melodyStarted=melodyStarted;state.warning=alertPulses>0;
  state.expression=int32_t(state.now-expressionUntil)<0;memcpy(state.wheels,wheelOutput,sizeof(wheelOutput));
  MatrixHeadPlan plan=matrixHeadPlan(state,scanLeftAngle);matrixHeadTarget=plan.angle;matrixHeadSource=plan.source;
  if(uint32_t(millis()-matrixHeadAt)<40)return;matrixHeadAt=millis();
  int delta=constrain(matrixHeadTarget-headAngle,-3,3);if(delta){headAngle+=delta;head.write(headAngle);}
}

void stopMotors() {
  for (auto &pair : motorPins) for (uint8_t pin : pair) pwm_set_gpio_level(pin, 0);
  moving = false;
  motorTestActive=false;
}

void matrixCommand(uint8_t value) {
  Wire.beginTransmission(0x71); Wire.write(value); Wire.endTransmission();
}

void matrixTick(bool force) {
  if(!matrixPresent)return;
  MatrixEffectState state;state.selected=matrixFace;state.led=ledStyle.c_str();state.alert=alertName.c_str();
  memcpy(state.wheels,wheelOutput,sizeof(wheelOutput));state.now=millis();state.effectStarted=effectStarted;
  state.melodyStarted=melodyStarted;state.alertStarted=alertStarted;state.warning=alertPulses>0;
  state.melody=melodyActive;state.tone=buzzerVolume>0 && int32_t(state.now-beepUntil)<0;
  state.expression=int32_t(state.now-expressionUntil)<0;state.brightness=matrixBrightness;
  MatrixEffectPlan plan=matrixEffectPlan(state);matrixActive=plan.face;matrixSource=plan.source;
  if(force || matrixOutputBrightness!=plan.brightness){matrixOutputBrightness=plan.brightness;matrixCommand(0xe0|matrixOutputBrightness);}
  matrixEffectFrame(plan,matrixRows);
  uint16_t aligned[8];matrixPanelRotate(matrixRows,matrixRotation,aligned);
  for(int col=0;col<8;col++)matrixWire[col]=aligned[7-col];
  static uint16_t previous[8]={};
  if(!force && !memcmp(previous,matrixWire,sizeof(matrixWire)))return;
  Wire.beginTransmission(0x71);Wire.write(0);
  // The tile correction matches Freenove setRow(y+tile*8, rowBits).
  for(int col=0;col<8;col++){uint16_t value=matrixWire[col];Wire.write(uint8_t(value));Wire.write(uint8_t(value>>8));}
  if(Wire.endTransmission()==0){memcpy(previous,matrixWire,sizeof(matrixWire));matrixWrites++;}
  else matrixErrors++;
}

String statusJson() {
  if(safetyExpired)expireControl();
  String result="{\"firmware\":\"CarReady-2.11\",\"board\":\"Pico W\",\"module\":\"";
  result+=matrixPresent ? "matrix" : "ultrasonic";
  result+="\",\"armed\":"; result+=armed ? "true" : "false";
  result+=",\"moving\":"; result+=moving ? "true" : "false";
  result+=",\"distance_cm\":"; result+=(matrixPresent || distanceCm<0) ? String("null") : String(distanceCm,1);
  result+=",\"battery_adc\":"; result+=String(batteryAdcRaw);
  result+=",\"battery_volts\":"+String(batteryVolts(),2)+",\"battery_percent\":"+(batteryPercent()<0 ? String("null") : String(batteryPercent()));
  result+=",\"battery_state\":\""+String(batteryState())+"\"";
  result+=",\"wifi_connected\":"; result+=networkReady() ? "true" : "false";
  result+=",\"home_wifi_connected\":"+String(!hotspotActive && homeLinkVerified ? "true" : "false");
  result+=",\"http_listening\":"+String(lanStarted && lanServer.status()!=0 ? "true" : "false");
  result+=",\"hotspot_open\":"+String(strlen(HOTSPOT_PASSWORD)==0 ? "true" : "false")+",\"home_settings_saved\":"+String(homeSettingsSaved ? "true" : "false")+",\"wifi_join_state\":\""+wifiJoinState+"\"";
  String currentIp=hotspotActive ? WiFi.softAPIP().toString() : homeLinkVerified ? WiFi.localIP().toString() : String("");
  result+=",\"home_ssid\":"+jsonText(homeSsid)+",\"saved_ssid\":"+jsonText(homeSettingsSaved ? homeSsid : String(""));
  result+=",\"connected_ssid\":"+jsonText(hotspotActive ? String(HOTSPOT_SSID) : homeLinkVerified ? homeAssociatedSsid : String(""));
  result+=",\"home_settings_source\":\""+String(homeForgotten || !homeSsid.length() ? "none" : homeSettingsSaved ? "saved" : "build_default")+"\",\"wifi_last_result\":"+jsonText(wifiLastResult);
  result+=",\"dashboard_url\":"+jsonText(currentIp.length() ? "http://"+currentIp+"/" : String(""))+",\"home_url\":"+jsonText(homeLastIp.length() ? "http://"+homeLastIp+"/" : String("http://freenove-car.local/"));
  result+=",\"wifi_signal_dbm\":"+String(!hotspotActive && homeLinkVerified ? String(WiFi.RSSI()) : String("null"))+",\"wifi_scan_state\":"+jsonText(wifiScanState);
  result+=",\"matrix_head_auto\":"+String(matrixHeadAuto ? "true" : "false")+",\"matrix_head_target\":"+String(matrixHeadTarget)+",\"matrix_head_source\":"+jsonText(matrixHeadSource);
  result+=",\"network_mode\":\""+String(hotspotActive ? "hotspot" : homeLinkVerified ? "home" : "connecting")+"\",\"hotspot_ssid\":\""+String(HOTSPOT_SSID)+"\",\"hotspot_ip\":\"192.168.4.1\",\"hotspot_clients\":"+String(hotspotActive ? WiFi.softAPgetStationNum() : 0);
  result+=",\"ip\":\""; result+=hotspotActive ? WiFi.softAPIP().toString() : homeLinkVerified ? WiFi.localIP().toString() : String("");
  result+="\",\"line\":["+String(digitalRead(12))+","+String(digitalRead(11))+","+String(digitalRead(10))+"]";
  result+=",\"light\":["+String(lightAdc[0])+","+String(lightAdc[1])+"]";
  result+=",\"ir_count\":"+String(irCount)+",\"ir_raw\":"+String(lastIr)+controlStatus()+"}"; return result;
}
void status() {Serial.println(statusJson());}

bool networkReady(){return hotspotActive || homeLinkVerified;}
void closeNetworkServer() {
  portalDns.stop();
  // Release old TCP contexts before changing interfaces. Retained FIN_WAIT
  // clients can otherwise prevent a fresh listener binding to port 80.
  for(auto &slot:httpSlots){slot.client.stop(1);slot.client=WiFiClient();slot.headers="";slot.body="";slot.response="";slot.page=false;slot.headersComplete=false;slot.sent=0;}
  while(lanServer.hasClient()){WiFiClient pending=lanServer.accept();pending.stop(1);}
  if(lanStarted)lanServer.end();lanStarted=false;
  lastLanStart=millis()-250;
  if(mdnsStarted)MDNS.end();mdnsStarted=false;
}
void saveHotspotChoice(bool useHotspot) {
  if(!networkFilesystem)return;
  if(useHotspot){File file=LittleFS.open("/network-hotspot","w");if(file){file.print("1");file.close();}}
  else LittleFS.remove("/network-hotspot");
}
bool storeHomeSettings(const String &ssid,const String &password,bool pending=false){
  if(!networkFilesystem)return false;
  File file=LittleFS.open("/wifi-home.tmp","w");if(!file)return false;
  uint8_t lengths[2]={uint8_t(ssid.length()),uint8_t(password.length())};
  bool ok=file.write(lengths,2)==2 && file.write(reinterpret_cast<const uint8_t*>(ssid.c_str()),ssid.length())==ssid.length()
    && file.write(reinterpret_cast<const uint8_t*>(password.c_str()),password.length())==password.length();
  file.close();
  if(!ok){LittleFS.remove("/wifi-home.tmp");return false;}
  bool stored=LittleFS.rename("/wifi-home.tmp",pending ? "/wifi-pending" : "/wifi-home");
  if(stored && !pending){LittleFS.remove("/wifi-forgotten");homeForgotten=false;}
  return stored;
}
void storeWifiResult(const char *value){
  wifiLastResult=value;if(!networkFilesystem)return;
  File file=LittleFS.open("/wifi-result","w");if(file){file.print(value);file.close();}
}
void loadWifiMetadata(){
  if(!networkFilesystem)return;
  homeForgotten=LittleFS.exists("/wifi-forgotten");
  if(homeForgotten){homeSsid="";homePassword="";}
  File result=LittleFS.open("/wifi-result","r");if(result){String value=result.readString();result.close();if(value=="joined"||value=="failed"||value=="forgotten")wifiLastResult=value;}
  File ip=LittleFS.open("/wifi-last-ip","r");if(ip){String value=ip.readString();ip.close();IPAddress parsed;if(value.length()<=15 && parsed.fromString(value))homeLastIp=value;}
}
void loadHomeSettings(bool pending=false){
  if(!networkFilesystem)return;
  if(!pending && homeForgotten)return;
  File file=LittleFS.open(pending ? "/wifi-pending" : "/wifi-home","r");if(!file)return;
  uint8_t lengths[2];if(file.read(lengths,2)!=2 || lengths[0]>32 || lengths[1]>64 || file.size()!=2+lengths[0]+lengths[1]){file.close();return;}
  char ssid[33]={},key[65]={};
  bool ok=file.read(reinterpret_cast<uint8_t*>(ssid),lengths[0])==lengths[0] && file.read(reinterpret_cast<uint8_t*>(key),lengths[1])==lengths[1];file.close();
  if(ok && wifiCredentialsValid(ssid,lengths[0],key,lengths[1])){
    if(pending){pendingSsid=ssid;pendingPassword=key;wifiPairing=true;wifiJoinState="connecting";}
    else{homeSsid=ssid;homePassword=key;homeSettingsSaved=true;}
  }
}
void startHotspot() {
  watchdog_enable(8000,true);
  homeLinkVerified=false;homeAssociatedSsid="";
  fullStop();closeNetworkServer();WiFi.disconnect();WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  hotspotActive=(strlen(HOTSPOT_PASSWORD) ? WiFi.beginAP(HOTSPOT_SSID,HOTSPOT_PASSWORD) : WiFi.beginAP(HOTSPOT_SSID))==WL_CONNECTED;
  if(hotspotActive){portalDns.setTTL(0);portalDns.start(53,"*",IPAddress(192,168,4,1));notice="Car Wi-Fi ready: open dashboard at http://192.168.4.1/; wheels stopped";}
  else {notice="Hotspot start failed; wheels stopped";homeDisconnectedAt=millis();}
  watchdog_enable(2000,true);
}
void startHomeWifi(bool offlineTest=false) {
  if(!offlineTest && !wifiPairing && !homeSsid.length()){startHotspot();return;}
  watchdog_enable(8000,true);
  homeLinkVerified=false;homeMatchSince=0;homeVerifyAt=millis()-250;homeAssociatedSsid="";
  fullStop();closeNetworkServer();
  if(hotspotActive)WiFi.disconnectAP();WiFi.disconnect();hotspotActive=false;
  saveHotspotChoice(false);WiFi.mode(WIFI_STA);
  const char *ssid=offlineTest ? "Freenove-Rover-Unavailable-Test" : wifiPairing ? pendingSsid.c_str() : homeSsid.c_str();
  const char *key=wifiPairing ? pendingPassword.c_str() : homePassword.c_str();
  if(strlen(key))WiFi.beginNoBlock(ssid,key);else WiFi.beginNoBlock(ssid);
  lastWifiAttempt=homeDisconnectedAt=millis();homeWasConnected=false;
  notice="Joining home Wi-Fi; car Wi-Fi returns after 30 seconds if unavailable";
  watchdog_enable(2000,true);
}
void queueNetwork(uint8_t choice){
  fullStop();if(choice!=4){wifiPairing=false;pendingSsid="";pendingPassword="";wifiJoinState="idle";}
  networkPending=choice;networkQueuedAt=millis();notice="Switching network; car stopped and disarmed";
}
void networkTick() {
  if(wifiScanRunning){
    int count=WiFi.scanComplete();
    if(count>=0){
      struct FoundNetwork{String ssid;int rssi;bool open;};FoundNetwork found[20];int used=0;
      for(int i=0;i<count;i++){
        const char *name=WiFi.SSID(i);if(!name)continue;char bounded[33]={};memcpy(bounded,name,strnlen(name,32));String ssid=bounded;
        if(!ssid.length() || ssid==HOTSPOT_SSID)continue;
        int rssi=WiFi.RSSI(i),same=-1;for(int j=0;j<used;j++)if(found[j].ssid==ssid)same=j;
        if(same>=0){if(rssi>found[same].rssi)found[same]={ssid,rssi,WiFi.encryptionType(i)==ENC_TYPE_NONE};continue;}
        if(used<20)found[used++]={ssid,rssi,WiFi.encryptionType(i)==ENC_TYPE_NONE};
      }
      for(int i=0;i<used;i++)for(int j=i+1;j<used;j++)if(found[j].rssi>found[i].rssi){FoundNetwork swap=found[i];found[i]=found[j];found[j]=swap;}
      wifiScanResults="[";for(int i=0;i<used;i++){if(i)wifiScanResults+=",";wifiScanResults+="{\"ssid\":"+jsonText(found[i].ssid)+",\"rssi\":"+String(found[i].rssi)+",\"open\":"+String(found[i].open ? "true" : "false")+"}";}wifiScanResults+="]";
      WiFi.scanDelete();wifiScanRunning=false;wifiScanState="ready";
    }else if(uint32_t(millis()-wifiScanStarted)>12000){wifiScanRunning=false;wifiScanState="failed";}
  }
  if(networkPending && uint32_t(millis()-networkQueuedAt)>=300) {
    uint8_t choice=networkPending;networkPending=0;
    fullStop();
    if(choice==4 && !storeHomeSettings(pendingSsid,pendingPassword,true)){wifiPairing=false;wifiJoinState="storage_error";notice="Wi-Fi settings could not be saved; network unchanged";return;}
    if(choice!=4)LittleFS.remove("/wifi-pending");
    saveHotspotChoice(choice==2 || choice==5);
    if(choice==3){File file=LittleFS.open("/network-offline-test","w");if(file){file.print("1");file.close();}}
    else LittleFS.remove("/network-offline-test");
    // A watchdog resets the CPU, while the radio can retain the old association.
    // Disassociate before reboot so a candidate cannot inherit that old link/IP.
    watchdog_enable(8000,true);closeNetworkServer();
    if(hotspotActive)WiFi.disconnectAP();WiFi.disconnect();delay(150);
    watchdog_reboot(0,0,20);while(true)tight_loop_contents();
  }
  // During the response grace period, the old link belongs to the old settings.
  // Never confirm or save a newly queued candidate using that connection.
  if(networkPending)return;
  if(hotspotActive){portalDns.processNextRequest();return;}
  if(uint32_t(millis()-homeVerifyAt)>=250){
    homeVerifyAt=millis();String actual=radioHomeSsid(),expected=wifiPairing ? pendingSsid : homeSsid;
    if(actual.length() && actual==expected){
      homeAssociatedSsid=actual;if(!homeMatchSince)homeMatchSince=millis();
      homeLinkVerified=uint32_t(millis()-homeMatchSince)>=1000;
    }else{homeLinkVerified=false;homeMatchSince=0;homeAssociatedSsid="";}
  }
  bool connected=homeLinkVerified;
  if(connected){
    bool justPaired=wifiPairing;
    if(wifiPairing){
      bool saved=storeHomeSettings(pendingSsid,pendingPassword);
      LittleFS.remove("/wifi-pending");
      homeSsid=pendingSsid;homePassword=pendingPassword;pendingSsid="";pendingPassword="";wifiPairing=false;
      homeSettingsSaved=saved;wifiJoinState=saved ? "joined" : "storage_error";
      if(saved)storeWifiResult("joined");
      notice=saved ? "Home Wi-Fi joined and saved; dashboard at http://freenove-car.local/" : "Wi-Fi joined but settings could not be saved";
    }
    if(!homeWasConnected){
      homeLastIp=WiFi.localIP().toString();
      if(networkFilesystem){File file=LittleFS.open("/wifi-last-ip","w");if(file){file.print(homeLastIp);file.close();}}
      if(!justPaired)notice="Home Wi-Fi connected; dashboard ready at http://"+homeLastIp+"/";
    }
    homeWasConnected=true;homeDisconnectedAt=millis();return;
  }
  if(homeWasConnected){homeWasConnected=false;homeDisconnectedAt=millis();fullStop();notice="Home Wi-Fi lost; car stopped";alert("wifi",3);}
  if(hotspotFallbackDue(false,false,millis(),homeDisconnectedAt)){
    bool failed=wifiPairing;wifiPairing=false;pendingSsid="";pendingPassword="";
    if(failed){LittleFS.remove("/wifi-pending");storeWifiResult("failed");}
    startHotspot();if(failed){wifiJoinState="failed";notice="Could not join that Wi-Fi. Car hotspot restored; previous saved network kept. Check name/password and retry.";}
  }
}

String wifiJoinRequest(const String &body,const String &headers,int &code){
  auto fail=[&](int status,const char *message){code=status;return String("{\"error\":\"")+message+"\"}";};
  String who=headerOf(headers,"X-Car-Owner");
  if(headerOf(headers,"X-Car-Token")!=controlToken || who.length()<3 || who.length()>64)return fail(403,"Reload the dashboard before connecting Wi-Fi");
  if(otaUntil || networkPending || wifiScanRunning)return fail(409,"Finish the scan, update or network switch first");
  if(armed && controller!=who)return fail(409,"Another controller is active; Stop first");
  if(!headerOf(headers,"Content-Type").startsWith("application/x-www-form-urlencoded"))return fail(415,"Use the dashboard Wi-Fi form");
  std::string encoded(body.c_str()),ssid,key,open;bool haveSsid=false,haveKey=false,haveOpen=false;
  for(size_t at=0;at<encoded.size();){
    size_t end=encoded.find('&',at);if(end==std::string::npos)end=encoded.size();
    std::string item=encoded.substr(at,end-at);size_t eq=item.find('=');
    if(eq==std::string::npos)return fail(400,"Invalid Wi-Fi form");
    std::string name=item.substr(0,eq),value;
    if(!wifiFormDecode(item.substr(eq+1),value))return fail(400,"Invalid Wi-Fi text encoding");
    if(name=="ssid" && !haveSsid){ssid=value;haveSsid=true;}
    else if(name=="password" && !haveKey){key=value;haveKey=true;}
    else if(name=="open" && !haveOpen){open=value;haveOpen=true;}
    else return fail(400,"Invalid or repeated Wi-Fi field");
    at=end+1;
  }
  if(!haveSsid || !haveKey || !haveOpen || (open!="0" && open!="1"))return fail(400,"Enter a network name and choose its password setting");
  if((open=="1")!=key.empty() || !wifiCredentialsValid(ssid.c_str(),ssid.size(),key.c_str(),key.size()))return fail(400,"Use a 1-32 byte Wi-Fi name and an 8-63 byte password, or choose an open network");
  if(!networkFilesystem)return fail(503,"Wi-Fi settings storage unavailable");
  pendingSsid=ssid.c_str();pendingPassword=key.c_str();wifiPairing=true;wifiJoinState="connecting";queueNetwork(4);
  return "{\"ok\":true,\"stopped\":true,\"state\":\"connecting\",\"home_url\":\"http://freenove-car.local/\",\"fallback_seconds\":30}";
}
String wifiActionRequest(const String &action,const String &headers,int &code){
  auto fail=[&](int status,const char *message){code=status;return String("{\"error\":\"")+message+"\"}";};
  String who=headerOf(headers,"X-Car-Owner");
  if(headerOf(headers,"X-Car-Token")!=controlToken || who.length()<3 || who.length()>64)return fail(403,"Reload the dashboard first");
  if(otaUntil || networkPending || wifiScanRunning)return fail(409,"Finish the scan, update or network switch first");
  if(armed && controller!=who)return fail(409,"Another controller is active; Stop first");
  if(action=="scan"){
    fullStop();wifiScanState="scanning";wifiScanResults="[]";wifiScanStarted=millis();
    WiFi.scanDelete();int count=WiFi.scanNetworks(true);wifiScanRunning=count==-1;
    if(!wifiScanRunning)wifiScanState=count==0 ? "ready" : "failed";
    return "{\"ok\":true,\"stopped\":true,\"state\":"+jsonText(wifiScanState)+"}";
  }
  if(!networkFilesystem)return fail(503,"Wi-Fi settings storage unavailable");
  // The tombstone prevents compiled credentials returning after a reboot or update.
  fullStop();File marker=LittleFS.open("/wifi-forgotten","w");if(!marker)return fail(503,"Could not forget Wi-Fi; settings kept");
  bool stored=marker.print("1")==1;marker.close();if(!stored){LittleFS.remove("/wifi-forgotten");return fail(503,"Could not forget Wi-Fi; settings kept");}
  LittleFS.remove("/wifi-home");LittleFS.remove("/wifi-pending");LittleFS.remove("/wifi-last-ip");
  homeForgotten=true;homeSettingsSaved=false;homeSsid="";homePassword="";homeLastIp="";storeWifiResult("forgotten");queueNetwork(5);
  return "{\"ok\":true,\"stopped\":true,\"state\":\"forgotten\",\"dashboard_url\":\"http://192.168.4.1/\"}";
}
void finishHttp(HttpSlot &slot){
  String body;const char *type="application/json";int code=200;String extra;
  String &headers=slot.headers;
  if(headers.startsWith("GET / HTTP/") || headers.startsWith("GET /?")){slot.page=true;type="text/html; charset=utf-8";}
  else if(headers.startsWith("GET /api/status HTTP/"))body=statusJson();
  else if(headers.startsWith("POST /api/stop HTTP/")){fullStop();body="{\"stopped\":true}";}
  else if(headers.startsWith("POST /api/wifi HTTP/"))body=wifiJoinRequest(slot.body,headers,code);
  else if(headers.startsWith("POST /api/wifi/scan HTTP/"))body=wifiActionRequest("scan",headers,code);
  else if(headers.startsWith("POST /api/wifi/forget HTTP/"))body=wifiActionRequest("forget",headers,code);
  else if(headers.startsWith("GET /api/wifi/scan HTTP/"))body="{\"state\":"+jsonText(wifiScanState)+",\"networks\":"+wifiScanResults+"}";
  else if(headers.startsWith("POST /api/control?")){int end=headers.indexOf(' ',5);body=controlRequest(headers.substring(5,end),headers,code);}
  else if(headers.startsWith("GET /favicon.ico ")){code=204;body="";}
  else if(hotspotActive && headers.startsWith("GET ") && !headers.startsWith("GET /api/")){
    code=302;extra="Location: http://192.168.4.1/\r\n";type="text/plain";body="Open the rover dashboard";
  }else{body="{\"error\":\"not found\"}";code=404;}
  slot.response=String("HTTP/1.1 ")+code+(code==200 ? " OK\r\n" : code==302 ? " Found\r\n" : code==204 ? " No Content\r\n" : " Error\r\n");
  size_t length=slot.page ? sizeof(dashboard)-1-9+controlToken.length() : body.length();
  slot.response+=String("Content-Type: ")+type+"\r\nCache-Control: no-store\r\nX-Frame-Options: DENY\r\nConnection: close\r\n"+extra+"Content-Length: "+length+"\r\n\r\n";
  slot.response+=body;slot.sent=0;slot.started=millis();slot.body="";slot.headers="";
}
void serveLan() {
  if(motorTestActive) return; // Diagnostic pulses last <=400ms; do not block their deadline with network writes.
  bool connected=networkReady();
  if(!connected) {
    if(lanStarted) {closeNetworkServer();fullStop();}
    return;
  }
  if(lanStarted && lanServer.status()==0)lanStarted=false;
  if(!lanStarted) {
    if(uint32_t(millis()-lastLanStart)<250)return;lastLanStart=millis();
    lanServer.begin(80,8);lanServer.setNoDelay(true);lanStarted=lanServer.status()!=0;
    if(!lanStarted)return;
    if(!hotspotActive){MDNS.begin("freenove-car");MDNS.addService("http","tcp",80);mdnsStarted=true;}
  }
  if(mdnsStarted)MDNS.update();
  for(auto &slot:httpSlots) {
  WiFiClient &lanClient=slot.client;
  String &httpHeaders=slot.headers;
  if(!lanClient) {slot.response="";slot.sent=0;slot.page=false;slot.body="";slot.bodyLength=0;slot.headersComplete=false;lanClient=lanServer.accept();httpHeaders="";slot.started=millis();if(lanClient)lanClient.setTimeout(100);}
  if(!lanClient) continue;
  if(uint32_t(millis()-slot.started)>(slot.response.length() ? 4000 : 2000) || httpHeaders.length()>8192) {lanClient.stop();slot.response="";continue;}
  if(slot.response.length()) {
    // Never send more than TCP can accept. A slow/closed browser must not hold the control loop.
    int room=lanClient.availableForWrite();
    // Stream the larger dashboard directly from flash, keeping four clients within the heap budget.
    static const size_t tokenAt=strstr(dashboard,"__TOKEN__")-dashboard;
    size_t total=slot.response.length()+(slot.page ? sizeof(dashboard)-1-9+controlToken.length() : 0);
    const char *data;size_t remaining;
    if(slot.sent<slot.response.length()){data=slot.response.c_str()+slot.sent;remaining=slot.response.length()-slot.sent;}
    else {size_t position=slot.sent-slot.response.length();
      if(position<tokenAt){data=dashboard+position;remaining=tokenAt-position;}
      else if(position<tokenAt+controlToken.length()){size_t tokenPos=position-tokenAt;data=controlToken.c_str()+tokenPos;remaining=controlToken.length()-tokenPos;}
      else {size_t rawPos=position-controlToken.length()+9;data=dashboard+rawPos;remaining=sizeof(dashboard)-1-rawPos;}
    }
    size_t amount=min(size_t(1460),remaining);
    if(room>0){amount=min(amount,size_t(room));slot.sent+=lanClient.write(reinterpret_cast<const uint8_t*>(data),amount);}
    if(slot.sent>=total){lanClient.stop();slot.response="";}
    continue;
  }
  int budget=512;
  while(lanClient.available() && budget-->0) {
    char value=char(lanClient.read());
    if(slot.headersComplete){slot.body+=value;if(slot.body.length()==size_t(slot.bodyLength)){finishHttp(slot);break;}continue;}
    httpHeaders+=value;
    if(httpHeaders.endsWith("\r\n\r\n")) {
      String length=headerOf(httpHeaders,"Content-Length");int expected=0;
      if((length.length() && !parameterInt(length,0,512,expected)) || headerOf(httpHeaders,"Transfer-Encoding").length()){
        slot.response="HTTP/1.1 413 Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";slot.started=millis();break;
      }
      slot.bodyLength=expected;slot.headersComplete=true;
      if(!expected){finishHttp(slot);break;}
    }
  }
  }
}

void handleCommand() {
  if(wifiScanRunning && strcmp(command,"STATUS") && strcmp(command,"STOP")){Serial.println("ERR Wi-Fi scan running; movement disabled");return;}
  if(otaUntil && strcmp(command,"STATUS") && strcmp(command,"STOP")){Serial.println("ERR wireless update window open; movement disabled");return;}
  if(networkPending && strcmp(command,"STATUS") && strcmp(command,"STOP")){Serial.println("ERR network switching; movement disabled");return;}
  int a,b,c,d,angle; char extra;
  if (!strcmp(command,"STATUS")) status();
  else if(!strcmp(command,"WIFI HOTSPOT")){queueNetwork(2);Serial.println("OK switching to car hotspot; stopped");}
  else if(!strcmp(command,"WIFI HOME")){queueNetwork(1);Serial.println("OK retrying home Wi-Fi; stopped");}
  else if(!strcmp(command,"WIFI TESTOFFLINE")){queueNetwork(3);Serial.println("OK stopped offline test; automatic hotspot after 30 seconds");}
  else if(!strcmp(command,"TESTBLOCKED LIFTED")) {
    if(matrixPresent){fullStop();Serial.println("ERR fit ultrasonic for recovery test");return;}
    startRun(PILOT,"USB-blocked-test",1000);speedLimit=25;escapeEnabled=backtrackEnabled=true;
    diagnosticBlocked=true;diagnosticBlockedPhase=0;autonomousRun=true;
    runDeadline=millis()+18000;hardRunDeadline=runDeadline;
    Serial.println("OK lifted blocked-route test: real forward, simulated wall, short retreat, real sonar; 18 second stop");
  }
  else if(!strcmp(command,"TESTNOECHO LIFTED")) {
    if(matrixPresent){fullStop();Serial.println("ERR fit ultrasonic for recovery test");return;}
    startRun(PILOT,"USB-recovery-test",1000);speedLimit=25;
    diagnosticEchoMuted=true;resetEchoes();autonomousRun=true;
    runDeadline=millis()+15000;hardRunDeadline=runDeadline;
    Serial.println("OK lifted recovery test: echoes muted until two pivots; 15 second stop");
  }
  else if(!strcmp(command,"TESTWATCHDOG")) {
    fullStop();Serial.println("OK watchdog recovery test; restarting in 2 seconds");Serial.flush();
    while(true)tight_loop_contents(); // The hardware watchdog must recover without main-loop help.
  }
  else if (sscanf(command,"TESTMOTOR %d %d %d %c",&a,&b,&c,&extra)==3) {
    fullStop();
    if(a<1 || a>4 || b < -35 || b>35 || c<1 || c>400) {Serial.println("ERR test bounds: wheel 1..4, speed -35..35, duration 1..400ms");return;}
    int speed=b*motorSigns[a-1];
    pwm_set_gpio_level(motorPins[a-1][speed<0 ? 1 : 0],abs(speed)*100);
    moving=(speed!=0);motorTestActive=true;motorTestDeadline=millis()+c;lease(c);wheelOutput[a-1]=b;
    Serial.println("OK bounded wheel test");
  } else if(sscanf(command,"LED %d %d %d %c",&a,&b,&c,&extra)==3) {
    if(a<0 || a>255 || b<0 || b>255 || c<0 || c>255) {Serial.println("ERR LED range");return;}
    chassisLeds.fill(chassisLeds.Color(a,b,c));chassisLeds.show();Serial.println("OK LEDs");
  }
  else if (!strcmp(command,"STOP") || !strcmp(command,"DISARM")) {
    fullStop(); Serial.println("OK stopped and disarmed");
  } else if (!strcmp(command,"ARM")) {
    fullStop(); armed=true; runMode=USB_CONTROL; controller="USB"; lease(500); lastDrive=millis(); Serial.println("OK armed; DRIVE lease 500ms");
  } else if (sscanf(command,"SERVO %d %c",&angle,&extra)==1 && angle>=30 && angle<=150) {
    fullStop();setHead(angle);if(matrixPresent)matrixHeadManualUntil=millis()+5000; Serial.println("OK servo");
  } else if (!strcmp(command,"BEEP")) {
    // GPIO2 shares a PWM slice with motor GPIO18/19: use software pulse instead.
    for(int i=0;i<100;i++) {digitalWrite(2,HIGH);delayMicroseconds(250);digitalWrite(2,LOW);delayMicroseconds(250);}
    Serial.println("OK beep");
  } else if (sscanf(command,"DRIVE %d %d %d %d %c",&a,&b,&c,&d,&extra)==4) {
    if (!armed) {Serial.println("ERR disarmed");return;}
    if (a < -40 || a > 40 || b < -40 || b > 40 || c < -40 || c > 40 || d < -40 || d > 40) {fullStop();Serial.println("ERR speed range -40..40");return;}
    stopMotors(); int values[4]={a,b,c,d};
    for(int i=0;i<4;i++) {int value=values[i]*motorSigns[i];pwm_set_gpio_level(motorPins[i][value<0 ? 1 : 0],abs(value)*100);}
    moving=(a || b || c || d);lastDrive=millis();lease(500);for(int i=0;i<4;i++)wheelOutput[i]=values[i]; Serial.println("OK drive");
  } else if (!strcmp(command,"HELP")) Serial.println("STATUS | ARM | DISARM | STOP | DRIVE m1 m2 m3 m4 | TESTMOTOR wheel speed milliseconds | LED r g b | SERVO 30..150 | BEEP");
  else {fullStop();Serial.println("ERR command; stopped");}
}

void setup() {
  // Hold every motor input LOW before initializing peripherals.
  for(auto &pair:motorPins) for(uint8_t pin:pair) {pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  Serial.begin(115200); analogReadResolution(10);
  for(auto &pair:motorPins) for(uint8_t pin:pair) {
    gpio_set_function(pin,GPIO_FUNC_PWM);
    pwm_config config=pwm_get_default_config();
    pwm_config_set_clkdiv(&config,float(clock_get_hz(clk_sys))/5000000.0f);
    pwm_config_set_wrap(&config,9999); // 500Hz, independent of servo PWM.
    pwm_init(pwm_gpio_to_slice_num(pin),&config,true);
    pwm_set_gpio_level(pin,0);
  }
  stopMotors(); pinMode(2,OUTPUT); digitalWrite(2,LOW);
  for(uint8_t pin: {10,11,12}) pinMode(pin,INPUT);
  pinMode(28,INPUT);pinMode(27,INPUT);
  chassisLeds.begin();chassisLeds.setBrightness(32);chassisLeds.clear();chassisLeds.show();
  IrReceiver.begin(3,false);
  head.attach(13,500,2500); head.write(90);
  Wire.setSDA(4); Wire.setSCL(5); Wire.begin(); Wire.setClock(100000); Wire.setTimeout(25);
  delay(300); Wire.beginTransmission(0x71); matrixPresent=(Wire.endTransmission()==0);
  if(matrixPresent) {matrixCommand(0x21);matrixCommand(0x81);matrixCommand(0xe4);matrixTick(true);}
  else {Wire.end();pinMode(4,OUTPUT);digitalWrite(4,LOW);pinMode(5,INPUT);}
  controlSetup();networkFilesystem=LittleFS.begin();loadWifiMetadata();loadHomeSettings();loadHomeSettings(true);
  bool offline=networkFilesystem && LittleFS.exists("/network-offline-test");if(offline)LittleFS.remove("/network-offline-test");
  bool requestedHotspot=networkFilesystem && LittleFS.exists("/network-hotspot");
  // Consume explicit switches once; later power cycles retry the saved home network.
  saveHotspotChoice(false);
  if(hotspotOnBoot(requestedHotspot,wifiPairing,offline))startHotspot();else startHomeWifi(offline);
  ArduinoOTA.setHostname("freenove-rover");ArduinoOTA.setPort(2040);ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([](){fullStop();saveHotspotChoice(hotspotActive);notice="Wireless firmware update: motors stopped";watchdog_update();});
  ArduinoOTA.onProgress([](unsigned int,unsigned int){watchdog_update();});
  ArduinoOTA.onEnd([](){fullStop();notice="Wireless update received; rebooting stopped";watchdog_update();});
  ArduinoOTA.onError([](ota_error_t){fullStop();otaUntil=0;notice="Wireless update failed; car stopped";watchdog_update();});
  watchdog_enable(2000,true); // Hardware recovery if USB/network code ever wedges the main loop.
}

void loop() {
  watchdog_update();
  networkTick();
  if(otaUntil && (int32_t(millis()-otaUntil)>=0 || !networkReady())){otaUntil=0;notice="Wireless update window closed";}
  if(otaListening && !otaUntil){ArduinoOTA.end();otaListening=false;watchdog_enable(2000,true);}
  if(otaUntil)ArduinoOTA.handle();
  sampleTelemetry();
  if(motorTestActive && (int32_t(millis()-motorTestDeadline)>=0 || !Serial)) stopMotors();
  controlTick();
  matrixHeadTick();
  processRemote();
  serveLan();
  while(Serial.available()) {
    char ch=Serial.read();
    if(ch=='\r') continue;
    if(ch=='\n') {
      command[commandLength]=0;
      if(overflowed) {fullStop();Serial.println("ERR command too long");}
      else if(commandLength) handleCommand();
      commandLength=0;overflowed=false;
    } else if(commandLength<sizeof(command)-1 && !overflowed) command[commandLength++]=ch;
    else {overflowed=true;fullStop();}
  }
  if(!matrixPresent && uint32_t(millis()-lastSample)>=60) {
    lastSample=millis();digitalWrite(4,LOW);delayMicroseconds(2);
    digitalWrite(4,HIGH);delayMicroseconds(10);digitalWrite(4,LOW);
    unsigned long duration=pulseIn(5,HIGH,18000);
    recordEcho(duration);
  }
  if(matrixPresent && uint32_t(millis()-lastEyes)>=50) {
    lastEyes=millis();matrixTick();
  }
}
