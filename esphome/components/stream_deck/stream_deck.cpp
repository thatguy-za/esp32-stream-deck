// Milestone 1: enumerate a Stream Deck (Mini, Original, or Original V2/MK.2)
// as a USB host and log key presses. No image writing yet (that's M2) - see
// docs/protocol.md and docs/hardware.md.
//
// Driver setup/event pattern follows espressif/esp-idf's own
// examples/peripherals/usb/host/hid example, since the usb_host_hid
// component's high-level flow (usb_lib_task + hid_host_install + queued
// driver/interface events) isn't obvious to get right from the header
// alone. Adapted here to ESPHome's setup()/loop() lifecycle instead of a
// blocking app_main loop, since blocking forever in setup() would hang the
// rest of ESPHome (WiFi, API, etc).
#include "stream_deck.h"

#include <cstdio>

#include "esphome/core/log.h"

namespace esphome {
namespace stream_deck {

static const char *const TAG = "stream_deck";

#define STREAMDECK_VID 0x0FD9

static const StreamDeckProfile kStreamdeckProfiles[] = {
    {0x0063, "Stream Deck Mini", StreamDeckModel::MODEL_MINI, 6, 1},
    {0x0090, "Stream Deck Mini Mk2", StreamDeckModel::MODEL_MINI, 6, 1},
    {0x0060, "Stream Deck Original", StreamDeckModel::MODEL_ORIGINAL, 15, 1},
    {0x006D, "Stream Deck Original V2", StreamDeckModel::MODEL_ORIGINAL_V2, 15, 4},
    {0x0080, "Stream Deck MK.2", StreamDeckModel::MODEL_ORIGINAL_V2, 15, 4},
    {0x00A5, "Stream Deck MK.2 Scissor", StreamDeckModel::MODEL_ORIGINAL_V2, 15, 4},
    {0x00B9, "Stream Deck MK.2 Module", StreamDeckModel::MODEL_ORIGINAL_V2, 15, 4},
};
#define STREAMDECK_PROFILE_COUNT (sizeof(kStreamdeckProfiles) / sizeof(kStreamdeckProfiles[0]))

const StreamDeckProfile *g_active_profile = nullptr;

const char *StreamDeckComponent::model_to_string_(StreamDeckModel model) {
  switch (model) {
    case StreamDeckModel::MODEL_MINI:
      return "mini";
    case StreamDeckModel::MODEL_ORIGINAL:
      return "original";
    case StreamDeckModel::MODEL_ORIGINAL_V2:
      return "original_v2 (or mk2)";
    default:
      return "unknown";
  }
}

const StreamDeckProfile *StreamDeckComponent::find_profile_(uint16_t vid, uint16_t pid) {
  if (vid != STREAMDECK_VID) {
    return nullptr;
  }
  for (size_t i = 0; i < STREAMDECK_PROFILE_COUNT; i++) {
    if (kStreamdeckProfiles[i].pid == pid) {
      return &kStreamdeckProfiles[i];
    }
  }
  return nullptr;
}

// Runs the USB Host Library's own event-handling loop on its own task, as
// required by the library (see usb_host_lib_handle_events docs).
void StreamDeckComponent::usb_lib_task(void *arg) {
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LOWMED,
  };

  ESP_ERROR_CHECK(usb_host_install(&host_config));
  xTaskNotifyGive(static_cast<TaskHandle_t>(arg));

  while (true) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_ERROR_CHECK(usb_host_device_free_all());
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      break;
    }
  }

  vTaskDelete(nullptr);
}

void StreamDeckComponent::log_key_report_(const uint8_t *data, size_t length) {
  const StreamDeckProfile *profile = g_active_profile;
  size_t header_len = profile ? profile->key_report_header_len : 1;
  size_t key_count = profile ? profile->key_count : 0;

  if (!profile || length < header_len + key_count) {
    // Either no profile matched yet, or this doesn't match the layout
    // docs/protocol.md assumes for the matched profile - dump it raw so the
    // doc can be corrected from what real hardware actually sends.
    char hex[3 * 64 + 1] = {0};
    for (size_t i = 0; i < length && i < 64; i++) {
      snprintf(&hex[i * 3], 4, "%02X ", data[i]);
    }
    ESP_LOGW(TAG, "Unexpected report length %u, raw bytes: %s", (unsigned) length, hex);
    return;
  }

  char keys[5 * 16 + 1] = {0};
  for (size_t i = 0; i < key_count && i < 16; i++) {
    snprintf(&keys[i * 5], 5, "%3u ", (unsigned) data[header_len + i]);
  }
  ESP_LOGI(TAG, "%s key states: [ %s]", profile->name, keys);
}

// Handles reports/events for an already-open HID interface.
void StreamDeckComponent::hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                                        hid_host_interface_event_t event, void *arg) {
  uint8_t data[64] = {0};
  size_t data_length = 0;
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
      ESP_ERROR_CHECK(
          hid_host_device_get_raw_input_report_data(hid_device_handle, data, sizeof(data), &data_length));
      log_key_report_(data, data_length);
      break;

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "Stream Deck disconnected (iface %d)", dev_params.iface_num);
      g_active_profile = nullptr;
      ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
      break;

    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      ESP_LOGW(TAG, "Transfer error on iface %d", dev_params.iface_num);
      break;

    default:
      ESP_LOGW(TAG, "Unhandled interface event %d on iface %d", event, dev_params.iface_num);
      break;
  }
}

// Handles driver-level events (device connected) - opens the interface and
// logs identification so we can confirm this is really a Stream Deck and
// which family it belongs to.
void StreamDeckComponent::handle_device_event_(hid_host_device_handle_t hid_device_handle,
                                                hid_host_driver_event_t event) {
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
    return;
  }

  hid_host_dev_info_t dev_info;
  ESP_ERROR_CHECK(hid_host_get_device_info(hid_device_handle, &dev_info));

  ESP_LOGI(TAG, "HID device connected: addr=%d iface=%d sub_class=%d proto=%d", dev_params.addr,
           dev_params.iface_num, dev_params.sub_class, dev_params.proto);
  ESP_LOGI(TAG, "  VID=0x%04X PID=0x%04X", dev_info.VID, dev_info.PID);
  ESP_LOGI(TAG, "  Manufacturer: %ls", dev_info.iManufacturer);
  ESP_LOGI(TAG, "  Product:      %ls", dev_info.iProduct);
  ESP_LOGI(TAG, "  Serial:       %ls", dev_info.iSerialNumber);

  const StreamDeckProfile *profile = find_profile_(dev_info.VID, dev_info.PID);
  if (profile) {
    ESP_LOGI(TAG, "  -> matches %s (PID 0x%04X, %d keys)", profile->name, profile->pid, profile->key_count);
    if (profile->model != this->configured_model_) {
      ESP_LOGW(TAG,
               "  -> but this build is configured for '%s' (stream_deck: model:) - key layout "
               "will be wrong until the YAML is updated to match the device actually connected",
               model_to_string_(this->configured_model_));
    }
    g_active_profile = profile;
  } else {
    ESP_LOGW(TAG,
             "  -> VID/PID 0x%04X/0x%04X doesn't match any known Stream Deck profile - "
             "add it to kStreamdeckProfiles / docs/protocol.md if this is a real Stream "
             "Deck of a different revision",
             dev_info.VID, dev_info.PID);
    g_active_profile = nullptr;
  }

  const hid_host_device_config_t dev_config = {
      .callback = hid_host_interface_callback,
      .callback_arg = nullptr,
  };
  ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
}

// Runs in the USB Host Library's context - keep it short, just forward to
// our own event queue so the real handling happens in loop().
void StreamDeckComponent::hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                                                    hid_host_driver_event_t event, void *arg) {
  auto *self = static_cast<StreamDeckComponent *>(arg);
  if (self == nullptr || self->event_queue_ == nullptr) {
    return;
  }
  const AppEvent evt = {
      .event_group = APP_EVENT_HID_HOST,
      .handle = hid_device_handle,
      .event = event,
  };
  xQueueSend(self->event_queue_, &evt, 0);
}

namespace {
// Grace period between boot and actually switching the USB-OTG PHY to host
// mode. A board freshly flashed over its native USB-C port is very likely
// still tethered to the flashing computer on that same port - and that
// computer is still actively driving the bus as *its* USB host. Slamming
// into host mode immediately contends with that and reliably brownout-
// reboots the board in a loop (see docs/hardware.md). This window gives
// time to physically unplug from the PC (and move to the documented 5V-pad
// power setup) before that happens.
constexpr uint32_t USB_HOST_START_DELAY_MS = 8000;
}  // namespace

void StreamDeckComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Stream Deck USB host...");
  this->event_queue_ = xQueueCreate(10, sizeof(AppEvent));

  ESP_LOGW(TAG,
           "Starting USB Host mode in %u seconds - if this board is still plugged into a "
           "computer over its native USB-C port, unplug it now (see docs/hardware.md). "
           "Leaving it connected will brownout-reboot the board.",
           (unsigned) (USB_HOST_START_DELAY_MS / 1000));
  this->set_timeout("start_usb_host", USB_HOST_START_DELAY_MS, [this]() { this->start_usb_host_(); });
}

void StreamDeckComponent::start_usb_host_() {
  bool task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2,
                                               &this->usb_task_handle_, 0);
  if (!task_created) {
    ESP_LOGE(TAG, "Failed to create USB host task");
    this->mark_failed();
    return;
  }
  ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));

  const hid_host_driver_config_t hid_host_driver_config = {
      .create_background_task = true,
      .task_priority = 5,
      .stack_size = 4096,
      .core_id = 0,
      .callback = hid_host_device_callback,
      .callback_arg = this,
  };
  esp_err_t err = hid_host_install(&hid_host_driver_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "hid_host_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "USB Host mode active. Waiting for the Stream Deck to be connected...");
}

void StreamDeckComponent::loop() {
  if (this->event_queue_ == nullptr) {
    return;
  }
  AppEvent evt;
  while (xQueueReceive(this->event_queue_, &evt, 0) == pdTRUE) {
    if (evt.event_group == APP_EVENT_HID_HOST) {
      this->handle_device_event_(evt.handle, evt.event);
    }
  }
}

void StreamDeckComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Stream Deck:");
  ESP_LOGCONFIG(TAG, "  Configured model: %s", model_to_string_(this->configured_model_));
  ESP_LOGCONFIG(TAG, "  USB Host mode starts %u s after boot", (unsigned) (USB_HOST_START_DELAY_MS / 1000));
}

}  // namespace stream_deck
}  // namespace esphome
