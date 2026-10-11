// EyeRenderer.h - turns a FaceState into OLED pixels.
//
// The iris is modelled as a flat disc sitting on a sphere (the eyeball).
// When the eye rotates, the disc is projected onto the screen, so it moves
// along a curve AND gets narrower towards the corners (foreshortening),
// exactly like a real iris seen from the front. Lids are curves that cut
// the white of the eye; blinking slides the upper lid down to a "closed"
// curve. All per-pixel work is integer (fixed point) because the ESP32-C3
// has no floating-point unit.
//
// Framebuffer: 128x64, 1 bit per pixel, SSD1306/U8g2 page layout:
//   byte index = (y / 8) * 128 + x, bit = y % 8   (1 = lit)
// That is the native layout of U8g2's full buffer and Adafruit_SSD1306,
// so you can render straight into the display library's buffer.
#pragma once
#include <stdint.h>
#include "EyeTypes.h"
#include "EyeGeometry.h"

namespace eye {

static const int kScreenW = 128;
static const int kScreenH = 64;
static const int kFrameBytes = kScreenW * kScreenH / 8;

inline void setPixel(uint8_t* fb, int x, int y) {
  if ((unsigned)x < (unsigned)kScreenW && (unsigned)y < (unsigned)kScreenH)
    fb[(y >> 3) * kScreenW + x] |= (uint8_t)(1u << (y & 7));
}
inline bool getPixel(const uint8_t* fb, int x, int y) {
  if ((unsigned)x >= (unsigned)kScreenW || (unsigned)y >= (unsigned)kScreenH) return false;
  return (fb[(y >> 3) * kScreenW + x] >> (y & 7)) & 1u;
}

// Where things ended up on screen for one eye (handy for tests/debugging).
struct EyeLayout {
  float cx, cy;          // eye centre (after head slide)
  float irisX, irisY;    // projected iris centre
  float irisW, irisH;    // projected iris width/height (foreshortened)
  float pupilR;          // pupil radius in px
  float glintX, glintY;  // catchlight centre
  float upperLidY, lowerLidY;  // lid heights at the iris column
};

class EyeRenderer {
 public:
  EyeRenderer();
  void setGeometry(const EyeGeometry& g);
  const EyeGeometry& geometry() const { return g_; }

  // Clears fb (kFrameBytes) and draws both eyes.
  void render(const FaceState& s, uint8_t* fb) const;
  // Test hook: draw only the white eye opening (no iris/pupil/highlights).
  bool openingOnly = false;
  EyeLayout layout(int idx, const FaceState& s) const;

 private:
  enum { kMaxHalf = 40, kCols = 2 * kMaxHalf + 1 };
  struct ColumnProfile { float archU, archL, archC, crease, scleraHalf, tn; uint8_t inside; };
  void buildProfiles();
  void renderEye(int idx, const FaceState& s, uint8_t* fb) const;
  void lidsAt(int idx, const EyePose& e, const ColumnProfile& c, float cy, float irisDy,
              float& yTop, float& yBot, float& yClosed) const;

  EyeGeometry g_;
  ColumnProfile prof_[2][kCols];  // per eye, per column offset from the eye centre
};

}  // namespace eye
