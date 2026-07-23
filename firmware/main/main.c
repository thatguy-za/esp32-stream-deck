/*
 * Milestone 1: enumerate a Stream Deck (Mini, Original, or Original V2/MK.2)
 * as a USB host and log key presses. No image writing yet (that's M2) — see
 * docs/protocol.md and docs/hardware.md.
 *
 * Driver setup/event pattern follows espressif/esp-idf's own
 * examples/peripherals/usb/host/hid example, since the usb_host_hid
 * component's high-level flow (usb_lib_task + hid_host_install +
 * queued driver/interface events) isn't obvious to get right from the
 * header alone.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"

#include "usb/usb_host.h"
#include "usb/hid_host.h"

static const char *TAG = "streamdeck";

#define STREAMDECK_VID 0x0FD9

/* One row per supported device family. Key-press report layout is
 * "header_len bytes, then one boolean byte per key" for all three families
 * (just with different header_len/key_count) — see docs/protocol.md. Image
 * upload isn't in this table yet since M1 doesn't write images; M2 will need
 * a richer per-family profile (image format, page header layout) built on
 * top of this same PID lookup. */
typedef struct {
    uint16_t pid;
    const char *name;
    uint8_t key_count;
    uint8_t key_report_header_len;
} streamdeck_profile_t;

static const streamdeck_profile_t kStreamdeckProfiles[] = {
    {0x0063, "Stream Deck Mini", 6, 1},
    {0x0090, "Stream Deck Mini Mk2", 6, 1},
    {0x0060, "Stream Deck Original", 15, 1},
    {0x006D, "Stream Deck Original V2", 15, 4},
    {0x0080, "Stream Deck MK.2", 15, 4},
    {0x00A5, "Stream Deck MK.2 Scissor", 15, 4},
    {0x00B9, "Stream Deck MK.2 Module", 15, 4},
};
#define STREAMDECK_PROFILE_COUNT (sizeof(kStreamdeckProfiles) / sizeof(kStreamdeckProfiles[0]))

static const streamdeck_profile_t *streamdeck_find_profile(uint16_t vid, uint16_t pid)
{
    if (vid != STREAMDECK_VID) {
        return NULL;
    }
    for (size_t i = 0; i < STREAMDECK_PROFILE_COUNT; i++) {
        if (kStreamdeckProfiles[i].pid == pid) {
            return &kStreamdeckProfiles[i];
        }
    }
    return NULL;
}

/* Set once a recognized device connects, so the interface callback (which
 * only gets a device handle, not the VID/PID) knows how to decode reports. */
static const streamdeck_profile_t *s_active_profile = NULL;

typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST,
} app_event_group_t;

typedef struct {
    app_event_group_t event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

static QueueHandle_t app_event_queue = NULL;

/* Runs the USB Host Library's own event-handling loop on its own task, as
 * required by the library (see usb_host_lib_handle_events docs). */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive((TaskHandle_t)arg);

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

    vTaskDelete(NULL);
}

static void log_key_report(const uint8_t *data, size_t length)
{
    const streamdeck_profile_t *profile = s_active_profile;
    size_t header_len = profile ? profile->key_report_header_len : 1;
    size_t key_count = profile ? profile->key_count : 0;

    if (!profile || length < header_len + key_count) {
        /* Either no profile matched yet, or this doesn't match the layout
         * docs/protocol.md assumes for the matched profile — dump it raw so
         * the doc can be corrected from what real hardware actually sends. */
        char hex[3 * 64 + 1] = {0};
        for (size_t i = 0; i < length && i < 64; i++) {
            snprintf(&hex[i * 3], 4, "%02X ", data[i]);
        }
        ESP_LOGW(TAG, "Unexpected report length %u, raw bytes: %s", (unsigned)length, hex);
        return;
    }

    char keys[5 * 16 + 1] = {0};
    for (size_t i = 0; i < key_count && i < 16; i++) {
        snprintf(&keys[i * 5], 5, "%3u ", (unsigned)data[header_len + i]);
    }
    ESP_LOGI(TAG, "%s key states: [ %s]", profile->name, keys);
}

/* Handles reports/events for an already-open HID interface. */
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                         const hid_host_interface_event_t event,
                                         void *arg)
{
    uint8_t data[64] = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
            hid_device_handle, data, sizeof(data), &data_length));
        log_key_report(data, data_length);
        break;

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Stream Deck disconnected (iface %d)", dev_params.iface_num);
        s_active_profile = NULL;
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

/* Handles driver-level events (device connected) — opens the interface and
 * logs identification so we can confirm this is really a Stream Deck and
 * which family it belongs to. */
static void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                                   const hid_host_driver_event_t event,
                                   void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
        return;
    }

    hid_host_dev_info_t dev_info;
    ESP_ERROR_CHECK(hid_host_get_device_info(hid_device_handle, &dev_info));

    ESP_LOGI(TAG, "HID device connected: addr=%d iface=%d sub_class=%d proto=%d",
             dev_params.addr, dev_params.iface_num, dev_params.sub_class, dev_params.proto);
    ESP_LOGI(TAG, "  VID=0x%04X PID=0x%04X", dev_info.VID, dev_info.PID);
    ESP_LOGI(TAG, "  Manufacturer: %ls", dev_info.iManufacturer);
    ESP_LOGI(TAG, "  Product:      %ls", dev_info.iProduct);
    ESP_LOGI(TAG, "  Serial:       %ls", dev_info.iSerialNumber);

    const streamdeck_profile_t *profile = streamdeck_find_profile(dev_info.VID, dev_info.PID);
    if (profile) {
        ESP_LOGI(TAG, "  -> matches %s (PID 0x%04X, %d keys)", profile->name, profile->pid,
                 profile->key_count);
        s_active_profile = profile;
    } else {
        ESP_LOGW(TAG, "  -> VID/PID 0x%04X/0x%04X doesn't match any known Stream Deck "
                       "profile — add it to kStreamdeckProfiles / docs/protocol.md if this "
                       "is a real Stream Deck of a different revision",
                 dev_info.VID, dev_info.PID);
        s_active_profile = NULL;
    }

    const hid_host_device_config_t dev_config = {
        .callback = hid_host_interface_callback,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
}

/* Runs in the USB Host Library's context — keep it short, just forward to
 * our own event queue so the real handling happens in app_main's task. */
static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                                      const hid_host_driver_event_t event,
                                      void *arg)
{
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT_HID_HOST,
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg,
    };

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "esp32-stream-deck: Milestone 1 (enumerate + log key presses)");

    BaseType_t task_created = xTaskCreatePinnedToCore(
        usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);
    ulTaskNotifyTake(false, 1000);

    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
    ESP_LOGI(TAG, "Waiting for the Stream Deck to be connected...");

    app_event_queue_t evt_queue;
    while (true) {
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (evt_queue.event_group == APP_EVENT_HID_HOST) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                       evt_queue.hid_host_device.event,
                                       evt_queue.hid_host_device.arg);
            }
        }
    }
}
