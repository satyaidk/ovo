// EyeGeometry.h - what the eyes LOOK like (pixels), separate from how they
// BEHAVE (EyeEngine). Two presets:
//
//   humanGeometry() - almond opening, lids covering the top/bottom of the
//                     iris like a real eye, gray (dithered) iris, black pupil.
//   mochiGeometry() - measured from your uploaded frame (tools/
//                     fit_eye_geometry.py): round eye cut flat by the upper
//                     lid, big black iris, gloss crescent, catchlight, rim arc.
//
// Coordinates: screen pixels, x right, y DOWN. Eye index 0 is the eye on the
// LEFT of the screen. "Nasal" = towards the screen centre (the nose).
#pragma once
#include <stdint.h>

namespace eye {

enum IrisStyle : uint8_t { IRIS_SOLID = 0, IRIS_GRAY = 1 };

struct EyeGeometry {
  // ---- placement ----------------------------------------------------------
  float centerX[2];       // eye centres
  float centerY;
  // ---- opening = (inside sclera ellipse) & (below upper lid) & (above lower lid)
  float halfWidth;        // corner-to-centre distance; lids are drawn over t in [-1,1]
  float scleraRx, scleraRy, scleraDy;   // ellipse bounding the visible eyeball
  float scleraPow;        // 2 = ellipse, >2 = squarer (superellipse)
  float upperBase;        // upper lid height at the corners (px, + = down)
  float upperArch;        // how far the lid arches up at its peak
  float upperPow;         // arch shape: 2 = elliptical, >2 boxier, <2 pointier
  float upperPeak;        // -1..1 where the arch peaks (+ = towards the nose)
  float upperTravel;      // px the whole upper lid moves down as upperOpen -> 0
  float upperTilt;        // px per unit tn the UPPER lid alone slopes (+ = lower at the nose)
  float lowerBase, lowerArch, lowerPow, lowerPeak;
  float cantTilt;         // px the OUTER corner sits higher than centre (canthal tilt)
  float closedBase, closedArch;  // the curve where the lids meet when blink = 1
  float slantPx;          // px at the corners per unit of upperSlant (anger/sadness)
  float lowerRaisePx;     // px the lower lid rises at lowerRaise = 1 (smile)
  float smileArch;        // px the lower lid bulges UP at lowerRaise = 1 (smiling-eyes crescent)
  // Lid-eye coupling: lids copy this fraction of the iris' vertical motion.
  // Upper lid follows the eye ~1:1, lower lid ~4x less [OxfordLid][IOVS-lid].
  float lidFollowUpper, lidFollowLower, lidFollowMaxUp, lidFollowMaxDown;
  // ---- eyeball & iris ---------------------------------------------------------
  float eyeRadius;        // R of the eyeball sphere (px)
  float irisRadius;       // px
  float pupilMin, pupilMax;  // px radius for pupil = 0 .. 1
  float limbalRing;       // px of dark ring at the iris edge
  uint8_t irisStyle;      // IRIS_SOLID / IRIS_GRAY
  // ---- highlights -------------------------------------------------------------
  float glintX, glintY, glintR;    // catchlight offset from iris centre (left eye; mirrored for right)
  float glint2X, glint2Y, glint2R; // optional 2nd catchlight (R = 0 -> off)
  float glintParallax;             // how much of the iris motion the catchlight copies (~0.54)
  uint8_t mirrorHighlights;        // 1 = mirror catchlight/gloss for the right eye (stylised),
                                   // 0 = same side in both eyes (one real light source)
  float glossX, glossY, glossR;    // white gloss disc clipped to the iris (R = 0 -> off)
  // ---- line accents -----------------------------------------------------------
  float creaseGap;        // px above the upper lid for a lid-crease line (0 = off)
  float creaseSpan;       // fraction of the eye width the crease covers
  uint8_t closedLine;     // 1 = draw the lid line while the eye is closed
  float rimR, rimA0, rimA1, rimW0, rimW1;  // rim arc around the eye (rimR = 0 -> off)
  // ---- head (whole-eye slide on screen) ---------------------------------------
  float headPxPerDegX, headPxPerDegY;
};

// Real-eye proportions scaled to the panel: iris 11.5 mm, eyeball 24 mm,
// MRD1 4-5 mm / MRD2 ~5 mm (lids cover the top ~1-1.5 mm of the iris),
// pupil 2-8 mm  [HumanEye-Wiki][Ptosis-UIowa].  ~1.9 px per mm here.
inline EyeGeometry humanGeometry() {
  EyeGeometry g{};
  g.centerX[0] = 31.5f; g.centerX[1] = 96.5f; g.centerY = 31.5f;
  g.halfWidth = 25.0f;
  g.scleraRx = 30; g.scleraRy = 40; g.scleraDy = 0; g.scleraPow = 2;  // not limiting
  g.upperBase = 1.0f; g.upperArch = 10.0f; g.upperPow = 2.0f; g.upperPeak = 0.18f; g.upperTravel = 6.0f;
  g.upperTilt = 0.0f;
  g.lowerBase = 1.0f; g.lowerArch = 8.6f;  g.lowerPow = 1.7f; g.lowerPeak = -0.15f;
  g.cantTilt = 2.0f;
  g.closedBase = 2.0f; g.closedArch = 5.5f;
  g.slantPx = 6.0f; g.lowerRaisePx = 3.5f; g.smileArch = 9.0f;
  g.lidFollowUpper = 0.85f; g.lidFollowLower = 0.25f; g.lidFollowMaxUp = 4.0f; g.lidFollowMaxDown = 7.0f;
  g.eyeRadius = 23.0f; g.irisRadius = 11.0f; g.pupilMin = 2.2f; g.pupilMax = 7.6f;
  g.limbalRing = 1.3f; g.irisStyle = IRIS_GRAY;
  g.glintX = -2.4f; g.glintY = -2.7f; g.glintR = 1.6f;   // light above-left, sits on the pupil edge
  g.glint2X = 3.2f; g.glint2Y = 3.4f; g.glint2R = 0.0f;
  g.glintParallax = 0.54f; g.mirrorHighlights = 0;
  g.glossX = 0; g.glossY = 0; g.glossR = 0;
  g.creaseGap = 4.0f; g.creaseSpan = 0.72f; g.closedLine = 1;
  g.rimR = 0;
  g.headPxPerDegX = 0.32f; g.headPxPerDegY = 0.30f;
  return g;
}

// Measured from your frame (128x64): eye circle r = 14.8 px centred at
// (20.0, 28.6) and (108.0, 27.9); flat upper lid at y = 20.5; iris circle
// r = 10.1 px, nearly concentric; catchlight ~3 px above the iris centre,
// ~1 px towards the nose; rim arc ~18 px from the eye centre on the
// lower-nasal side (10..40 deg); gloss disc r ~9 px on the lower-nasal part
// of the iris. Both eyes averaged and made mirror-symmetric about x = 64.
inline EyeGeometry mochiGeometry() {
  EyeGeometry g{};
  // Fitted by host/fit_preset.cpp (IoU 0.81 vs your frame, see README).
  g.centerX[0] = 20.5f; g.centerX[1] = 107.5f; g.centerY = 28.2f;
  g.halfWidth = 16.0f;
  g.scleraRx = 15.56f; g.scleraRy = 14.7f; g.scleraDy = 0; g.scleraPow = 1.97f;
  g.upperBase = -4.9f; g.upperArch = 0.0f; g.upperPow = 2; g.upperPeak = 0; g.upperTravel = 20.0f;
  g.upperTilt = 3.06f;   // lid line slopes down towards the nose (y 21 outside -> 25 inside)
  g.lowerBase = 17.0f; g.lowerArch = 0.0f; g.lowerPow = 2; g.lowerPeak = 0;   // just below the eye
  g.cantTilt = 0;
  g.closedBase = 9.5f; g.closedArch = 3.0f;
  g.slantPx = 4.5f; g.lowerRaisePx = 10.0f; g.smileArch = 6.0f;
  g.lidFollowUpper = 0.8f; g.lidFollowLower = 0.0f; g.lidFollowMaxUp = 3.0f; g.lidFollowMaxDown = 3.0f;
  g.eyeRadius = 22.0f; g.irisRadius = 9.1f; g.pupilMin = 0; g.pupilMax = 0;
  g.limbalRing = 0; g.irisStyle = IRIS_SOLID;
  g.glintX = 0.9f; g.glintY = -3.18f; g.glintR = 0.8f;
  g.glint2X = 0; g.glint2Y = 0; g.glint2R = 0;
  g.glintParallax = 0.54f; g.mirrorHighlights = 1;
  g.glossX = 12.9f; g.glossY = 6.04f; g.glossR = 8.0f;
  g.creaseGap = 0; g.creaseSpan = 0; g.closedLine = 1;
  g.rimR = 18.25f; g.rimA0 = 9.07f; g.rimA1 = 40.0f; g.rimW0 = 2.02f; g.rimW1 = 0.96f;
  g.headPxPerDegX = 0.32f; g.headPxPerDegY = 0.30f;
  return g;
}

}  // namespace eye
