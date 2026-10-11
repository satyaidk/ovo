// secrets.example.h - template for the credentials MochiEyes needs.
//
// Copy this file to secrets.h and fill in your own values. secrets.h is listed
// in .gitignore so your Wi-Fi password never reaches GitHub. If secrets.h is
// missing, the firmware still builds with these placeholders: it fails to
// join Wi-Fi and opens its own hotspot (AP_SSID below), where you can enter
// your network from the web page.
#pragma once

#define WIFI_SSID_DEFAULT "YOUR_WIFI_NAME"      // 2.4 GHz network; the C3 has no 5 GHz radio
#define WIFI_PASS_DEFAULT "YOUR_WIFI_PASSWORD"

#define AP_SSID "Kitty"     // hotspot name used when Wi-Fi fails
#define AP_PASS "12345678"  // at least 8 characters, or the hotspot will not start
