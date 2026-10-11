// EyeRenderer.cpp - see EyeRenderer.h for the model.
#include "EyeRenderer.h"

#include <string.h>

#include "EyeMath.h"

namespace eye {

namespace {
inline int32_t q8(float v) { return (int32_t)lrintf(v * 256.0f); }
inline int32_t q16(float v) { return (int32_t)lrintf(v * 65536.0f); }

// Lid arch profile over tn in [-1,1] (+1 = nasal corner). 0 at the corners,
// 1 at the peak, which can be moved towards the nose (real upper lids peak
// slightly nasal of centre, lower lids dip slightly temporal).
float archShape(float tn, float powv, float peak) {
  const float u = (tn >= peak) ? (tn - peak) / (1.0f - peak) : (tn - peak) / (1.0f + peak);
  const float au = absf(u);
  if (au >= 1.0f) return 0.0f;
  return powf(1.0f - powf(au, powv), 1.0f / powv);
}
}  // namespace

EyeRenderer::EyeRenderer() { setGeometry(humanGeometry()); }

void EyeRenderer::setGeometry(const EyeGeometry& g) {
  g_ = g;
  buildProfiles();
}

// Everything that depends only on the column (not on time) is computed once
// here, so the per-frame work has no powf/sqrtf per column.
void EyeRenderer::buildProfiles() {
  for (int idx = 0; idx < 2; ++idx) {
    const float nasal = (idx == 0) ? 1.0f : -1.0f;
    const float frac = g_.centerX[idx] - floorf(g_.centerX[idx]);
    for (int k = -kMaxHalf; k <= kMaxHalf; ++k) {
      ColumnProfile& c = prof_[idx][k + kMaxHalf];
      const float xr = (float)k + 0.5f - frac;  // pixel centre relative to eye centre
      const float t = xr / g_.halfWidth;
      c.tn = t * nasal;
      c.inside = absf(t) < 1.0f;
      c.archU = archShape(c.tn, g_.upperPow, g_.upperPeak);
      c.archL = archShape(c.tn, g_.lowerPow, g_.lowerPeak);
      c.archC = archShape(c.tn, 2.0f, 0.0f);
      c.crease = (g_.creaseSpan > 0 && absf(t) < g_.creaseSpan)
                     ? archShape(c.tn / g_.creaseSpan, 2.0f, g_.upperPeak) : -1.0f;
      const float sx = xr / g_.scleraRx;
      const float n = g_.scleraPow > 0.5f ? g_.scleraPow : 2.0f;
      c.scleraHalf = (absf(sx) < 1.0f) ? g_.scleraRy * powf(1.0f - powf(absf(sx), n), 1.0f / n) : -1.0f;
    }
  }
}

void EyeRenderer::lidsAt(int /*idx*/, const EyePose& e, const ColumnProfile& c, float cy, float irisDy,
                         float& yTop, float& yBot, float& yClosed) const {
  const EyeGeometry& g = g_;
  // Lid-eye coupling: lids copy the iris' vertical motion (upper ~1:1,
  // lower ~1/4), clamped to what real lids can do.
  const float followU = clampf(irisDy * g.lidFollowUpper * e.lidCoupling, -g.lidFollowMaxUp, g.lidFollowMaxDown);
  const float followL = clampf(irisDy * g.lidFollowLower * e.lidCoupling, -g.lidFollowMaxUp, g.lidFollowMaxDown);
  const float tilt = g.cantTilt * c.tn;  // + tn = nasal: inner corner lower, outer higher
  const float open = e.upperOpen;
  const float yU = cy + g.upperBase + g.upperTravel * (1.0f - open) - g.upperArch * open * c.archU +
                   tilt + (g.upperTilt + g.slantPx * e.upperSlant) * c.tn + followU;
  // Smiling: the cheek pushes the lower lid up and bends it into an upward bulge.
  const float yL = cy + g.lowerBase - g.lowerRaisePx * e.lowerRaise +
                   (g.lowerArch - g.smileArch * e.lowerRaise) * c.archL + tilt + followL;
  float yC = cy + g.closedBase + g.closedArch * c.archC + tilt + 0.5f * (followU + followL);
  if (yC > yL) yC = yL;
  const float b = clampf(e.blink, 0.0f, 1.0f);
  yTop = lerpf(yU, yC, b);
  yBot = lerpf(yL, yC, b);
  if (yTop > yBot) yTop = yBot = 0.5f * (yTop + yBot);
  yClosed = yC;
}

EyeLayout EyeRenderer::layout(int idx, const FaceState& s) const {
  const EyeGeometry& g = g_;
  const EyePose& e = s.eye[idx];
  EyeLayout L;
  const int hx = (int)lrintf(s.headYawDeg * g.headPxPerDegX);
  const int hy = (int)lrintf(-s.headPitchDeg * g.headPxPerDegY);
  L.cx = g.centerX[idx] + (float)hx;
  L.cy = g.centerY + (float)hy;
  const float q = clampf(e.yawDeg, -60, 60) * kDegToRad, p = clampf(e.pitchDeg, -60, 60) * kDegToRad;
  float d2 = g.eyeRadius * g.eyeRadius - g.irisRadius * g.irisRadius;
  const float d = sqrtf(d2 < 1 ? 1 : d2);
  L.irisX = L.cx + d * cosf(p) * sinf(q);
  L.irisY = L.cy - d * sinf(p);
  L.irisW = 2.0f * g.irisRadius * cosf(q);
  L.irisH = 2.0f * g.irisRadius * cosf(p);
  L.pupilR = (g.pupilMax > 0) ? g.pupilMin + (g.pupilMax - g.pupilMin) * clampf(e.pupil, 0, 1) : 0;
  const float hm = (idx == 1 && g.mirrorHighlights) ? -1.0f : 1.0f;
  L.glintX = L.cx + hm * g.glintX + g.glintParallax * (L.irisX - L.cx);
  L.glintY = L.cy + g.glintY + g.glintParallax * (L.irisY - L.cy);
  // lids at the iris column
  int k = (int)floorf(L.irisX) - ((int)floorf(g.centerX[idx]) + hx);
  if (k < -kMaxHalf) k = -kMaxHalf;
  if (k > kMaxHalf) k = kMaxHalf;
  float yT, yB, yC;
  lidsAt(idx, e, prof_[idx][k + kMaxHalf], L.cy, L.irisY - L.cy, yT, yB, yC);
  L.upperLidY = yT;
  L.lowerLidY = yB;
  return L;
}

void EyeRenderer::render(const FaceState& s, uint8_t* fb) const {
  memset(fb, 0, kFrameBytes);
  renderEye(0, s, fb);
  renderEye(1, s, fb);
}

void EyeRenderer::renderEye(int idx, const FaceState& s, uint8_t* fb) const {
  const EyeGeometry& g = g_;
  const EyePose& e = s.eye[idx];
  const float nasal = (idx == 0) ? 1.0f : -1.0f;

  // ---- head: the whole eye slides on screen (integer px keeps lid tables valid)
  const int hx = (int)lrintf(s.headYawDeg * g.headPxPerDegX);
  const int hy = (int)lrintf(-s.headPitchDeg * g.headPxPerDegY);
  const int baseX = (int)floorf(g.centerX[idx]) + hx;
  const float cx = g.centerX[idx] + (float)hx;
  const float cy = g.centerY + (float)hy;

  // ---- iris disc on the eyeball sphere -------------------------------------
  // gaze g = (cos p sin q, -sin p, cos p cos q); disc axes e1 = (cos q, 0, -sin q),
  // e2 = (sin p sin q, cos p, sin p cos q). Disc centre sits d = sqrt(R^2 - r^2)
  // in front of the eyeball centre. Screen offset (dx,dy) = M (a,b) with
  // M = [[cos q, sin p sin q],[0, cos p]]; we need the inverse per pixel.
  const float q = clampf(e.yawDeg, -60, 60) * kDegToRad;
  const float p = clampf(e.pitchDeg, -60, 60) * kDegToRad;
  const float cq = cosf(q), sq = sinf(q), cp = cosf(p), sp = sinf(p);
  float d2 = g.eyeRadius * g.eyeRadius - g.irisRadius * g.irisRadius;
  const float d = sqrtf(d2 < 1 ? 1 : d2);
  const float icx = cx + d * cp * sq;
  const float icy = cy - d * sp;
  const int32_t ICX = q8(icx), ICY = q8(icy);
  const int32_t I11 = (int32_t)lrintf(4096.0f / cq);
  const int32_t I12 = (int32_t)lrintf(-4096.0f * (sp / cp) * (sq / cq));
  const int32_t I22 = (int32_t)lrintf(4096.0f / cp);

  const float rI = g.irisRadius;
  const float rP = (g.pupilMax > 0) ? g.pupilMin + (g.pupilMax - g.pupilMin) * clampf(e.pupil, 0, 1) : 0.0f;
  const float rL = rI - g.limbalRing;
  const float rG = rP + 0.72f * (rL - rP);  // inner 72% of the band 50% gray, outer part darker
  const int32_t RI2 = q16(rI * rI), RP2 = q16(rP * rP), RL2 = q16(rL * rL), RG2 = q16(rG * rG);

  // ---- highlights (corneal reflections copy ~54% of the iris motion) ---------
  const float hm = (idx == 1 && g.mirrorHighlights) ? -1.0f : 1.0f;
  const float par = g.glintParallax;
  const int32_t G1X = q8(cx + hm * g.glintX + par * (icx - cx)), G1Y = q8(cy + g.glintY + par * (icy - cy));
  const int32_t G1R2 = q16(g.glintR * g.glintR);
  const int32_t G2X = q8(cx + hm * g.glint2X + par * (icx - cx)), G2Y = q8(cy + g.glint2Y + par * (icy - cy));
  const int32_t G2R2 = q16(g.glint2R * g.glint2R);
  const int32_t GLX = q8(cx + hm * g.glossX + par * (icx - cx)), GLY = q8(cy + g.glossY + par * (icy - cy));
  const int32_t GLR2 = q16(g.glossR * g.glossR);

  const float irisDy = icy - cy;
  const bool drawClosed = g.closedLine && e.blink > 0.5f;

  for (int k = -kMaxHalf; k <= kMaxHalf; ++k) {
    const int x = baseX + k;
    if (x < 0 || x >= kScreenW) continue;
    const ColumnProfile& c = prof_[idx][k + kMaxHalf];
    if (!c.inside || c.scleraHalf < 0) continue;

    float yTop, yBot, yClosed;
    lidsAt(idx, e, c, cy, irisDy, yTop, yBot, yClosed);
    const float sTop = cy + g.scleraDy - c.scleraHalf, sBot = cy + g.scleraDy + c.scleraHalf;
    const float oTop = yTop > sTop ? yTop : sTop;
    const float oBot = yBot < sBot ? yBot : sBot;

    // eyelid crease: a thin line on the lid skin above the opening
    if (c.crease >= 0.0f) {
      const float open = e.upperOpen;
      const float followU = clampf(irisDy * g.lidFollowUpper * e.lidCoupling, -g.lidFollowMaxUp, g.lidFollowMaxDown);
      const float yCr = cy + g.upperBase + g.upperTravel * (1.0f - open) - (g.upperArch * open + g.creaseGap) * c.crease +
                        (g.cantTilt + g.upperTilt + g.slantPx * e.upperSlant) * c.tn + 0.7f * followU + 2.5f * e.blink;
      if (yCr < oTop - 1.5f || oBot - oTop < 1.0f) setPixel(fb, x, (int)floorf(yCr));
    }

    if (oBot - oTop < 1.0f) {  // this column is closed
      if (drawClosed) setPixel(fb, x, (int)floorf(0.5f * (yTop + yBot)));
      continue;
    }
    int y0 = (int)ceilf(oTop - 0.5f), y1 = (int)floorf(oBot - 0.5f);
    if (y0 < 0) y0 = 0;
    if (y1 > kScreenH - 1) y1 = kScreenH - 1;

    const int32_t dxq = ((int32_t)x << 8) + 128 - ICX;
    const int32_t g1dx = ((int32_t)x << 8) + 128 - G1X, g2dx = ((int32_t)x << 8) + 128 - G2X;
    const int32_t gldx = ((int32_t)x << 8) + 128 - GLX;
    for (int y = y0; y <= y1; ++y) {
      const int32_t dyq = ((int32_t)y << 8) + 128 - ICY;
      const int32_t a = (I11 * dxq + I12 * dyq) >> 12;
      const int32_t b = (I22 * dyq) >> 12;
      const int32_t rho2 = a * a + b * b;  // Q16, distance on the iris disc
      bool lit;
      if (rho2 < RI2 && !openingOnly) {
        if (g.irisStyle == IRIS_GRAY) {
          if (rho2 < RP2 || rho2 >= RL2) lit = false;              // pupil / limbal ring
          else if (rho2 < RG2) lit = ((x + y) & 1) == 0;           // 50% gray iris
          else lit = ((x & 1) == 0) && ((y & 1) == 0);             // 25%: darker rim
        } else {
          lit = false;                                             // solid dark iris
        }
        const int32_t yy = ((int32_t)y << 8) + 128;
        if (GLR2 > 0) {
          const int32_t ex = gldx, ey = yy - GLY;
          if (ex * ex + ey * ey <= GLR2) lit = true;
        }
        {
          const int32_t ex = g1dx, ey = yy - G1Y;
          if (G1R2 > 0 && ex * ex + ey * ey <= G1R2) lit = true;
        }
        {
          const int32_t ex = g2dx, ey = yy - G2Y;
          if (G2R2 > 0 && ex * ex + ey * ey <= G2R2) lit = true;
        }
      } else {
        lit = true;  // sclera
      }
      if (lit) setPixel(fb, x, y);
    }
  }

  // ---- rim arc accent (outside the eye, fixed to the face) -------------------
  if (g.rimR > 0) {
    const float s0 = sinf(g.rimA0 * kDegToRad), c0 = cosf(g.rimA0 * kDegToRad);
    const float s1 = sinf(g.rimA1 * kDegToRad), c1 = cosf(g.rimA1 * kDegToRad);
    const float rMax = g.rimR + 0.5f * (g.rimW0 > g.rimW1 ? g.rimW0 : g.rimW1) + 1.0f;
    const int xa = (int)floorf(cx - rMax), xb = (int)ceilf(cx + rMax);
    const int ya = (int)floorf(cy - rMax), yb = (int)ceilf(cy + rMax);
    for (int y = ya; y <= yb; ++y) {
      for (int x = xa; x <= xb; ++x) {
        const float dxn = ((float)x + 0.5f - cx) * nasal, dy = (float)y + 0.5f - cy;
        if (dxn <= 0 || dy <= 0) continue;
        // angle between A0 and A1 (measured from the nasal horizontal, downwards)
        if (dy * c0 < dxn * s0 || dy * c1 > dxn * s1) continue;
        const float r = sqrtf(dxn * dxn + dy * dy);
        const float f = clampf((dy / r - s0) / (s1 - s0), 0, 1);
        const float w = lerpf(g.rimW0, g.rimW1, f);
        if (absf(r - g.rimR) <= 0.5f * w) setPixel(fb, x, y);
      }
    }
  }
}

}  // namespace eye
