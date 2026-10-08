#include "../firmware/CarReady/NetworkPolicy.h"
static_assert(!hotspotOnBoot(false,false,false),"Normal restarts prefer saved home Wi-Fi");
static_assert(hotspotOnBoot(true,false,false),"An explicit one-shot hotspot request is honored");
static_assert(!hotspotOnBoot(true,true,false),"Pending home pairing overrides a stale hotspot marker");
static_assert(!hotspotOnBoot(true,false,true),"Offline diagnostic still exercises home fallback");
static_assert(!hotspotFallbackDue(false,false,29999,0),"Keep home association window open");
static_assert(hotspotFallbackDue(false,false,30000,0),"Missing home Wi-Fi starts fallback at deadline");
static_assert(!hotspotFallbackDue(false,true,90000,0),"Connected home does not switch modes");
static_assert(!hotspotFallbackDue(true,false,90000,0),"An existing hotspot remains stable");
static_assert(!hotspotFallbackDue(false,false,100,UINT32_MAX-100),"Wrap is a short wait");
static_assert(hotspotFallbackDue(false,false,30000,UINT32_MAX-100),"Fallback works across clock wrap");
