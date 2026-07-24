#pragma once

#include <string>

#include "esphome/components/api/custom_api_device.h"
#include "esphome/core/color.h"
#include "esphome/core/string_ref.h"

#include "icons.h"

namespace esphome {
namespace stream_deck {

class StreamDeckComponent;

// One per key configured in YAML (`stream_deck: keys:`). Not itself an
// ESPHome Component - a plain object owned by StreamDeckComponent, which
// calls subscribe() from its own setup() and looks these up by key_index_
// when a physical key press is detected.
class StreamDeckKey : public api::CustomAPIDevice {
 public:
  void set_parent(StreamDeckComponent *parent) { this->parent_ = parent; }
  void set_key_index(uint8_t index) { this->key_index_ = index; }
  uint8_t get_key_index() const { return this->key_index_; }
  void set_entity_id(const std::string &entity_id) { this->entity_id_ = entity_id; }
  const std::string &get_entity_id() const { return this->entity_id_; }
  void set_icon(IconType icon) { this->icon_ = icon; }
  IconType get_icon() const { return this->icon_; }
  // Takes a raw 0xRRGGBB code (rather than a Color) so Python codegen can
  // just pass the plain int straight through from YAML.
  void set_on_color(uint32_t colorcode) { this->on_color_ = Color(colorcode); }
  void set_off_color(uint32_t colorcode) { this->off_color_ = Color(colorcode); }

  const std::string &get_friendly_name() const { return this->friendly_name_; }
  bool is_on() const { return this->state_ == "on"; }
  Color get_color() const { return this->is_on() ? this->on_color_ : this->off_color_; }

  // Subscribes to this key's entity state + friendly name over the HA API.
  void subscribe();

  void toggle_entity();

 protected:
  void on_state_changed_(StringRef state);
  void on_name_changed_(StringRef name);

  StreamDeckComponent *parent_{nullptr};
  uint8_t key_index_{0};
  std::string entity_id_;
  std::string state_{"unknown"};
  std::string friendly_name_;
  IconType icon_{IconType::GENERIC};
  Color on_color_{0, 255, 0};
  Color off_color_{128, 128, 128};
};

}  // namespace stream_deck
}  // namespace esphome
