#pragma once
// Shared swarm secret — change this before production flash.
// Must match host:  export ECHO_SECRET='...'  or  --secret
#ifndef ECHO_SECRET
#define ECHO_SECRET "echogrid-change-me"
#endif

// WiFiManager setup AP password (WPA2, min 8 chars)
#ifndef ECHO_PORTAL_PASS
#define ECHO_PORTAL_PASS "echogrid1"
#endif
