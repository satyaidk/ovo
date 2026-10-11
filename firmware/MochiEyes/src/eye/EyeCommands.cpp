// EyeCommands.cpp - see EyeCommands.h for the protocol.
#include "EyeCommands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace eye {

namespace {
const int kMaxTok = 4;

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool eq(const char* a, const char* b) {
  while (*a && *b) {
    if (lower(*a) != lower(*b)) return false;
    ++a; ++b;
  }
  return *a == *b;
}

// Splits `buf` in place on spaces/tabs/commas. Returns token count.
int tokenize(char* buf, char* tok[kMaxTok]) {
  int n = 0;
  char* p = buf;
  while (*p && n < kMaxTok) {
    while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r' || *p == '\n') ++p;
    if (!*p) break;
    tok[n++] = p;
    while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\r' && *p != '\n') ++p;
    if (*p) *p++ = 0;
  }
  return n;
}

bool num(const char* s, float& v) {
  if (!s) return false;
  char* end = nullptr;
  v = strtof(s, &end);
  return end && end != s && *end == 0;
}

const char* kMoods[] = {"calm", "listening", "talking", "curious", "focused", "sleepy"};
const char* kExprs[] = {"neutral", "happy", "sad", "angry", "surprised", "sleepy", "suspicious"};
}  // namespace

bool EyeCommands::execute(const char* line, char* out, size_t outLen) {
  char buf[96];
  strncpy(buf, line ? line : "", sizeof buf - 1);
  buf[sizeof buf - 1] = 0;
  char* t[kMaxTok] = {nullptr, nullptr, nullptr, nullptr};
  const int n = tokenize(buf, t);
  if (n == 0) { snprintf(out, outLen, "?"); return false; }
  const char* c = t[0];
  float a = 0, b = 0;

  if (eq(c, "look") && n >= 3 && num(t[1], a) && num(t[2], b)) {
    e_.lookAt(a, b);
    snprintf(out, outLen, "ok look %.1f %.1f", (double)a, (double)b);
  } else if (eq(c, "lookn") && n >= 3 && num(t[1], a) && num(t[2], b)) {
    e_.lookAtNormalized(a, b);
    snprintf(out, outLen, "ok lookn %.2f %.2f", (double)a, (double)b);
  } else if (eq(c, "auto")) {
    e_.setMode(Mode::Auto);
    snprintf(out, outLen, "ok auto");
  } else if (eq(c, "mood") && n >= 2) {
    for (int i = 0; i < (int)Mood::Count; ++i)
      if (eq(t[1], kMoods[i])) { e_.setMood((Mood)i); snprintf(out, outLen, "ok mood %s", kMoods[i]); return true; }
    snprintf(out, outLen, "err mood: calm|listening|talking|curious|focused|sleepy");
    return false;
  } else if (eq(c, "expr") && n >= 2) {
    for (int i = 0; i < (int)Expression::Count; ++i)
      if (eq(t[1], kExprs[i])) { e_.setExpression((Expression)i); snprintf(out, outLen, "ok expr %s", kExprs[i]); return true; }
    snprintf(out, outLen, "err expr: neutral|happy|sad|angry|surprised|sleepy|suspicious");
    return false;
  } else if (eq(c, "blink")) {
    e_.blinkNow();
    snprintf(out, outLen, "ok blink");
  } else if (eq(c, "pause")) {
    e_.speechPause();
    snprintf(out, outLen, "ok pause");
  } else if (eq(c, "light") && n >= 2 && num(t[1], a)) {
    e_.setLight(a);
    snprintf(out, outLen, "ok light %.2f", (double)a);
  } else if (eq(c, "arousal") && n >= 2 && num(t[1], a)) {
    e_.setArousal(a);
    snprintf(out, outLen, "ok arousal %.2f", (double)a);
  } else if (eq(c, "near") && n >= 2 && num(t[1], a)) {
    e_.setTargetDistanceCm(a);
    snprintf(out, outLen, "ok near %.0f cm", (double)a);
  } else if (eq(c, "user") && n >= 3 && num(t[1], a) && num(t[2], b)) {
    e_.setUserDirection(a, b);
    snprintf(out, outLen, "ok user %.1f %.1f", (double)a, (double)b);
  } else if (eq(c, "startle") && n >= 3 && num(t[1], a) && num(t[2], b)) {
    e_.startle(a, b);
    snprintf(out, outLen, "ok startle");
  } else if (eq(c, "sleep")) {
    e_.setSleeping(true);
    snprintf(out, outLen, "ok sleep");
  } else if (eq(c, "wake")) {
    e_.setSleeping(false);
    snprintf(out, outLen, "ok wake");
  } else if (eq(c, "style") && n >= 2) {
    if (eq(t[1], "human")) r_.setGeometry(humanGeometry());
    else if (eq(t[1], "mochi")) r_.setGeometry(mochiGeometry());
    else { snprintf(out, outLen, "err style: human|mochi"); return false; }
    snprintf(out, outLen, "ok style %s", t[1]);
  } else if (eq(c, "feature") && n >= 3 && num(t[2], a)) {
    const bool on = a != 0;
    Features& f = e_.features;
    bool* p = nullptr;
    if (eq(t[1], "saccades")) p = &f.saccades;
    else if (eq(t[1], "micro")) p = &f.microsaccades;
    else if (eq(t[1], "blinks")) p = &f.blinks;
    else if (eq(t[1], "lids")) p = &f.lidCoupling;
    else if (eq(t[1], "pupil")) p = &f.pupil;
    else if (eq(t[1], "head")) p = &f.head;
    else if (eq(t[1], "vergence")) p = &f.vergence;
    else if (eq(t[1], "corrective")) p = &f.correctiveSaccades;
    else if (eq(t[1], "gazeblink")) p = &f.gazeEvokedBlinks;
    if (!p) { snprintf(out, outLen, "err feature: saccades|micro|blinks|lids|pupil|head|vergence|corrective|gazeblink"); return false; }
    *p = on;
    snprintf(out, outLen, "ok feature %s %d", t[1], on ? 1 : 0);
  } else if (eq(c, "status")) {
    const FaceState& s = e_.state();
    snprintf(out, outLen, "mode=%s mood=%s state=%s gaze=%.1f,%.1f head=%.1f pupil=%.2f blinks=%lu saccades=%lu",
             e_.mode() == Mode::Auto ? "auto" : "manual", kMoods[(int)e_.mood()], e_.attentionName(),
             (double)e_.gazeYaw(), (double)e_.gazePitch(), (double)s.headYawDeg, (double)s.eye[0].pupil,
             (unsigned long)e_.blinkCount(), (unsigned long)e_.saccadeCount());
  } else if (eq(c, "help")) {
    snprintf(out, outLen,
             "look y p | lookn x y | auto | mood m | expr e | blink | pause | light v | arousal v | near cm | "
             "user y p | startle y p | sleep | wake | style human|mochi | feature f 0|1 | status");
  } else {
    snprintf(out, outLen, "? unknown or incomplete command (try: help)");
    return false;
  }
  return true;
}

}  // namespace eye
