#pragma once

#include <vector>

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/color.h"

namespace esphome {
namespace stream_deck {

// A headless, in-RAM RGB888 canvas - no physical display attached. Not
// registered as an ESPHome Component (never goes through
// App.register_component()); StreamDeckComponent owns one instance
// directly and drives it explicitly (init() once, then draw calls +
// encode_bmp() per render), so its setup()/loop()/update() are never
// called by the framework - fine, since we don't need periodic polling.
//
// Confirmed viable pattern via the community ssd1306_virtual platform,
// which does the same "DisplayBuffer subclass with no real bus" trick to
// get a pure framebuffer. See esphome/components/display/display_buffer.h
// for the base class this relies on.
class StreamDeckCanvas : public display::DisplayBuffer {
 public:
  void init(int width, int height);

  void update() override {}
  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }

  // Encodes the current buffer as a 24-bit BMP matching the Mini family's
  // documented layout (docs/protocol.md): vertically flipped + rotated 90°.
  // Assumes a square canvas (true for every Stream Deck key size documented
  // so far - Mini is 80x80, Original/MK.2 are 72x72).
  //
  // The rotation direction/flip axis here is a best-effort implementation
  // of that description - docs/protocol.md doesn't specify clockwise vs.
  // counterclockwise, and this couldn't be verified without real hardware.
  // If the rendered key image comes out sideways or mirrored, this is the
  // first place to adjust.
  std::vector<uint8_t> encode_bmp() const;

 protected:
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  Color get_pixel_(int x, int y) const;

  int width_{0};
  int height_{0};
};

}  // namespace stream_deck
}  // namespace esphome
