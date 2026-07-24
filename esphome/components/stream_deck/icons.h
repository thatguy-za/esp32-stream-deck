#pragma once

#include <string>

#include "esphome/components/display/display.h"
#include "esphome/core/color.h"

namespace esphome {
namespace stream_deck {

// Simple vector-drawn icon shapes, keyed by Home Assistant entity domain.
// Deliberately not using ESPHome's font:/glyphs: (Material Design Icons)
// mechanism here: that needs specific glyph code points baked in at compile
// time, which either means fetching+verifying the MDI webfont and its
// codepoints (a real extra compile-time network dependency), or asking the
// user to hand-declare a font: block per icon - neither is as simple or as
// automatic as just drawing the handful of shapes we need directly with
// the canvas's own primitives (filled_rectangle/circle/line, inherited from
// display::Display). Trade-off: less visually polished than real MDI
// icons, but zero extra config and no external assets.
enum class IconType {
  LIGHTBULB,
  TOGGLE_SWITCH,
  FAN,
  SCRIPT,
  SCENE,
  GENERIC,
};

IconType icon_for_domain(const std::string &domain);

// Draws `icon` centered at (cx, cy), sized to fit within a `size`x`size`
// box, in the given color.
void draw_icon(display::Display &display, IconType icon, int cx, int cy, int size, Color color);

}  // namespace stream_deck
}  // namespace esphome
