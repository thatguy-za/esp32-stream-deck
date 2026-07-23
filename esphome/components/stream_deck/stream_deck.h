#pragma once

#include "esphome/core/component.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"

namespace esphome {
namespace stream_deck {

// One row per supported device family. Key-press report layout is "header_len
// bytes, then one boolean byte per key" for all three families (just with
// different header_len/key_count) - see docs/protocol.md. Image upload isn't
// in this table yet since M1 doesn't write images; M2 will need a richer
// per-family profile (image format, page header layout) built on this same
// PID lookup.
struct StreamDeckProfile {
  uint16_t pid;
  const char *name;
  uint8_t key_count;
  uint8_t key_report_header_len;
};

class StreamDeckComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

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

  void handle_device_event_(hid_host_device_handle_t hid_device_handle, hid_host_driver_event_t event);
  static void log_key_report_(const uint8_t *data, size_t length);
  static const StreamDeckProfile *find_profile_(uint16_t vid, uint16_t pid);

  QueueHandle_t event_queue_{nullptr};
  TaskHandle_t usb_task_handle_{nullptr};
};

// Set once a recognized device connects, so the (static) interface callback -
// which only gets a device handle, not the VID/PID - knows how to decode
// reports. Only one Stream Deck is supported per ESP32 at a time, which
// matches the hardware (one USB-OTG host port).
extern const StreamDeckProfile *g_active_profile;

}  // namespace stream_deck
}  // namespace esphome
