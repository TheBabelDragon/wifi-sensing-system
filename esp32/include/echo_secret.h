#pragma once
// Baked-in swarm credentials — firmware + host must match.
// Override only if you rotate: -DECHO_SECRET=\"...\" -DECHO_PORTAL_PASS=\"...\"

#ifndef ECHO_SECRET
#define ECHO_SECRET "Eg7$kQ2mN9pR4vX8wL3hJ6cF1bA5yU0zT"
#endif

// WiFiManager setup AP password (WPA2, 8+ chars)
#ifndef ECHO_PORTAL_PASS
#define ECHO_PORTAL_PASS "Eg7kQ2mN9p"
#endif
