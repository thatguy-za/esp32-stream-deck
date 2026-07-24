#pragma once

#include <vector>

#include "esphome/core/component.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/usb_helpers.h"
#include "usb/usb_host.h"

#include "stream_deck_canvas.h"
#include "stream_deck_key.h"

namespace esphome {
namespace stream_deck {

// Selectable in YAML via `stream_deck: model:`. One entry per protocol
// family, not per PID - "Mini"/"Mini Mk2" share a protocol, as do "Original
// V2"/"MK.2" (and its Scissor/Module variants) - see docs/protocol.md.
enum class StreamDeckModel {
  MODEL_MINI,
  MODEL_ORIGINAL,
  MODEL_ORIGINAL_V2,
};

// One row per supported PID. Key-press report layout is "header_len bytes,
// then one boolean byte per key" for all three families (just with
// different header_len/key_count) - see docs/protocol.md.
//
// Image dimensions/format are only filled in for the Mini family so far -
// that's the only one write_key_image_() knows how to encode for (BMP).
// Original/Original V2/MK.2 use JPEG, which needs a software encoder this
// project doesn't have yet; key_pixel_size is 0 for those rows as a marker.
struct StreamDeckProfile {
  uint16_t pid;
  const char *name;
  StreamDeckModel model;
  uint8_t key_count;
  uint8_t key_report_header_len;
  uint8_t key_pixel_size;
};

class StreamDeckComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_model(StreamDeckModel model) { this->configured_model_ = model; }
  void set_font(display::BaseFont *font) { this->font_ = font; }
  void add_key(StreamDeckKey *key);

  // Called by a StreamDeckKey whenever its state/name changes. Renders that
  // key's tile into the shared canvas and writes it to the device, if one is
  // connected and the active profile supports image writes (Mini family
  // only for now - see StreamDeckProfile's comment).
  void render_and_send_key_(StreamDeckKey *key);

 protected:
  enum AppEventGroup {
    APP_EVENT_HID_HOST = 0,
  };

  struct AppEvent {
    AppEventGroup event_group;
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
  };

  static void usb_lib_task(void *arg);
  static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                                        hid_host_driver_event_t event, void *arg);
  static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                           hid_host_interface_event_t event, void *arg);

  // Actually switches the shared USB-OTG PHY into host mode. Deferred a few
  // seconds after boot (see setup()) rather than done immediately, so a
  // board still tethered to a PC over the same USB-C port (e.g. right after
  // flashing) has a window to be unplugged first - starting USB Host mode
  // while a PC is still actively driving that same port as its own USB host
  // contends for the bus/VBUS and reliably brownout-reboots the board in a
  // loop. See docs/hardware.md.
  void start_usb_host_();

  void handle_device_event_(hid_host_device_handle_t hid_device_handle, hid_host_driver_event_t event);
  void log_key_report_(const uint8_t *data, size_t length);
  void handle_key_press_(uint8_t key_index);
  static const StreamDeckProfile *find_profile_(uint16_t vid, uint16_t pid);
  static const char *model_to_string_(StreamDeckModel model);

  // Sends one already-encoded BMP image to a key, chunked into the Mini
  // family's 1024-byte/16-byte-header pages (see docs/protocol.md), over
  // SET_REPORT control transfers on EP0 - the same mechanism already
  // confirmed working for enumeration, so it doesn't touch the interrupt IN
  // endpoint that failed to claim on some units (see the notes on
  // dump_full_descriptor_ below).
  void write_key_image_(uint8_t key_index, const uint8_t *bmp, size_t len);

  // Diagnostic only: dumps every interface/alternate-setting/endpoint on the
  // device to the log via a second, read-only USB Host client running
  // alongside hid_host's own. hid_host_device_open() only ever tries the
  // *first* IN endpoint of the interface it matched (see
  // hid_host_interface_claim_and_prepare_transfer() in espressif/esp-usb) -
  // if that's the one exceeding the ESP32-S3's ~408 byte pipe limit, this
  // tells us whether a different alternate setting exposes a smaller one we
  // could claim instead. See docs/protocol.md.
  void dump_full_descriptor_(uint8_t dev_addr);
  static void diag_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg);

  QueueHandle_t event_queue_{nullptr};
  TaskHandle_t usb_task_handle_{nullptr};
  StreamDeckModel configured_model_{StreamDeckModel::MODEL_MINI};
  usb_host_client_handle_t diag_client_hdl_{nullptr};
  hid_host_device_handle_t active_device_handle_{nullptr};
  std::vector<bool> last_key_states_;
  std::vector<StreamDeckKey *> keys_;
  StreamDeckCanvas canvas_;
  display::BaseFont *font_{nullptr};
};

// Set once a recognized device connects, so the (static) interface callback -
// which only gets a device handle, not the VID/PID - knows how to decode
// reports. Only one Stream Deck is supported per ESP32 at a time, which
// matches the hardware (one USB-OTG host port).
extern const StreamDeckProfile *g_active_profile;

}  // namespace stream_deck
}  // namespace esphome
