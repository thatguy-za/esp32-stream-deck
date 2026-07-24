#include "icons.h"

#include <cmath>

namespace esphome {
namespace stream_deck {

IconType icon_for_domain(const std::string &domain) {
  if (domain == "light") {
    return IconType::LIGHTBULB;
  }
  if (domain == "switch" || domain == "input_boolean") {
    return IconType::TOGGLE_SWITCH;
  }
  if (domain == "fan") {
    return IconType::FAN;
  }
  if (domain == "script") {
    return IconType::SCRIPT;
  }
  if (domain == "scene") {
    return IconType::SCENE;
  }
  return IconType::GENERIC;
}

namespace {

void draw_lightbulb(display::Display &display, int cx, int cy, int size, Color color) {
  int bulb_r = size / 3;
  display.filled_circle(cx, cy - size / 8, bulb_r, color);
  display.filled_rectangle(cx - bulb_r / 2, cy + bulb_r / 2, bulb_r, size / 8, color);
}

void draw_toggle_switch(display::Display &display, int cx, int cy, int size, Color color) {
  int w = static_cast<int>(size * 0.7f);
  int h = static_cast<int>(size * 0.4f);
  display.rectangle(cx - w / 2, cy - h / 2, w, h, color);
  display.filled_circle(cx, cy, h / 3, color);
}

void draw_fan(display::Display &display, int cx, int cy, int size, Color color) {
  int blade_len = size / 2;
  display.filled_circle(cx, cy, size / 8, color);
  for (int i = 0; i < 3; i++) {
    float angle = static_cast<float>(i) * (2.0f * static_cast<float>(M_PI) / 3.0f);
    int ex = cx + static_cast<int>(std::sin(angle) * blade_len);
    int ey = cy - static_cast<int>(std::cos(angle) * blade_len);
    display.line(cx, cy, ex, ey, color);
  }
}

void draw_script(display::Display &display, int cx, int cy, int size, Color color) {
  int w = static_cast<int>(size * 0.6f);
  int h = static_cast<int>(size * 0.8f);
  display.rectangle(cx - w / 2, cy - h / 2, w, h, color);
  for (int i = 1; i <= 3; i++) {
    int y = cy - h / 2 + (h * i) / 4;
    display.horizontal_line(cx - w / 2 + 2, y, w - 4, color);
  }
}

void draw_scene(display::Display &display, int cx, int cy, int size, Color color) {
  int r = size / 6;
  display.filled_circle(cx, cy - size / 4, r, color);
  display.filled_circle(cx - size / 4, cy + size / 6, r, color);
  display.filled_circle(cx + size / 4, cy + size / 6, r, color);
}

void draw_generic(display::Display &display, int cx, int cy, int size, Color color) {
  display.circle(cx, cy, size / 3, color);
  display.line(cx, cy - size / 2, cx, cy - size / 6, color);
}

}  // namespace

void draw_icon(display::Display &display, IconType icon, int cx, int cy, int size, Color color) {
  switch (icon) {
    case IconType::LIGHTBULB:
      draw_lightbulb(display, cx, cy, size, color);
      break;
    case IconType::TOGGLE_SWITCH:
      draw_toggle_switch(display, cx, cy, size, color);
      break;
    case IconType::FAN:
      draw_fan(display, cx, cy, size, color);
      break;
    case IconType::SCRIPT:
      draw_script(display, cx, cy, size, color);
      break;
    case IconType::SCENE:
      draw_scene(display, cx, cy, size, color);
      break;
    case IconType::GENERIC:
    default:
      draw_generic(display, cx, cy, size, color);
      break;
  }
}

}  // namespace stream_deck
}  // namespace esphome
