/*
 * Milestone 1: enumerate a Stream Deck Mini as a USB host and log key
 * presses. No image writing yet (that's M2) — see docs/protocol.md and
 * docs/hardware.md.
 *
 * Driver setup/event pattern follows espressif/esp-idf's own
 * examples/peripherals/usb/host/hid example, since the usb_host_hid
 * component's high-level flow (usb_lib_task + hid_host_install +
 * queued driver/interface events) isn't obvious to get right from the
 * header alone.
 */
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

/* Elgato Stream Deck Mini — see docs/protocol.md for sources. */
#define STREAMDECK_VID          0x0FD9
#define STREAMDECK_PID_MINI     0x0063
#define STREAMDECK_PID_MINI_MK2 0x0090

#define STREAMDECK_KEY_COUNT    6

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
    if (length < 1 + STREAMDECK_KEY_COUNT) {
        /* Doesn't match the 7-byte layout assumed in docs/protocol.md —
         * dump it raw so we can update the doc from what real hardware
         * actually sends. */
        char hex[3 * 64 + 1] = {0};
        for (size_t i = 0; i < length && i < 64; i++) {
            snprintf(&hex[i * 3], 4, "%02X ", data[i]);
        }
        ESP_LOGW(TAG, "Unexpected report length %u, raw bytes: %s", (unsigned)length, hex);
        return;
    }

    ESP_LOGI(TAG, "status=0x%02X keys=[%d %d %d %d %d %d]",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
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
 * logs identification so we can confirm this is really a Stream Deck Mini. */
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

    if (dev_info.VID == STREAMDECK_VID &&
        (dev_info.PID == STREAMDECK_PID_MINI || dev_info.PID == STREAMDECK_PID_MINI_MK2)) {
        ESP_LOGI(TAG, "  -> matches Stream Deck Mini (PID 0x%04X)", dev_info.PID);
    } else {
        ESP_LOGW(TAG, "  -> VID/PID does not match the expected Stream Deck Mini "
                       "(0x%04X/0x%04X or 0x%04X/0x%04X) — update docs/protocol.md "
                       "if this unit is a different revision",
                 STREAMDECK_VID, STREAMDECK_PID_MINI, STREAMDECK_VID, STREAMDECK_PID_MINI_MK2);
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
