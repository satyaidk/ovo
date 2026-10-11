# Security policy

OvoEyes is a hobby device for a home network. It is not hardened for
untrusted networks, and this page explains what that means in practice.

## Supported versions

Only the latest release on `main` receives fixes.

| Version | Supported |
| ------- | --------- |
| 1.0.x   | Yes       |
| < 1.0   | No        |

## Reporting a vulnerability

Please do not open a public issue for a security problem. Use GitHub's
private vulnerability reporting instead (repository **Security** tab,
**Report a vulnerability**). Include the firmware version or commit, what an
attacker could do, and steps to reproduce.

You can expect an acknowledgement within 7 days. Fixes are released as a new
patch version and credited in the changelog unless you ask otherwise.

## Known limitations (by design)

These are documented trade-offs, not bugs. Reports that show a concrete
impact beyond what is described here are still welcome.

- **The web controller has no login.** Anyone on the same Wi-Fi network can
  open `http://ovo.local`, change the eyes, write the note, or replace the
  saved Wi-Fi credentials (which restarts the device). Keep the device on a
  network you trust.
- **The fallback hotspot uses a shared password** (`AP_PASS` in `secrets.h`,
  default `12345678`). Change it before using the device anywhere public.
- **HTTPS requests do not verify certificates** (`WiFiClientSecure::setInsecure()`).
  The device only reads public, non-sensitive data (weather and coin prices);
  a network attacker could feed it false numbers but cannot obtain secrets.
- **Wi-Fi credentials are stored unencrypted** in the ESP32's NVS flash.
  Anyone with physical access and a USB cable can read them.
- **The location lookup uses plain HTTP** (`ip-api.com`), so the approximate
  city is visible to the network.

## Keeping your own secrets out of git

Credentials live in `secrets.h`, which `.gitignore` excludes. Only the
template `secrets.example.h` is committed. If you ever commit a real password
by accident, change the password: deleting the commit does not remove it from
clones or forks.
