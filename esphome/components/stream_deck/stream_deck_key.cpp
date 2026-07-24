#include "stream_deck_key.h"
#include "stream_deck.h"

namespace esphome {
namespace stream_deck {

void StreamDeckKey::subscribe() {
  this->subscribe_homeassistant_state(&StreamDeckKey::on_state_changed_, this->entity_id_);
  this->subscribe_homeassistant_state(&StreamDeckKey::on_name_changed_, this->entity_id_, "friendly_name");
}

void StreamDeckKey::on_state_changed_(StringRef state) {
  this->state_ = state.str();
  if (this->parent_ != nullptr) {
    this->parent_->render_and_send_key_(this);
  }
}

void StreamDeckKey::on_name_changed_(StringRef name) {
  this->friendly_name_ = name.str();
  if (this->friendly_name_.empty()) {
    this->friendly_name_ = this->entity_id_;
  }
  if (this->parent_ != nullptr) {
    this->parent_->render_and_send_key_(this);
  }
}

void StreamDeckKey::toggle_entity() {
  this->call_homeassistant_service("homeassistant.toggle", {{"entity_id", this->entity_id_}});
}

}  // namespace stream_deck
}  // namespace esphome
