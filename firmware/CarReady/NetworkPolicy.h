#pragma once
#include <cstdint>
constexpr uint32_t HOME_WIFI_WAIT_MS=30000;
constexpr bool hotspotFallbackDue(bool hotspot,bool homeConnected,uint32_t now,uint32_t since) {
  return !hotspot && !homeConnected && uint32_t(now-since)>=HOME_WIFI_WAIT_MS;
}
