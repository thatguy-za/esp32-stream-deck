import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@thatguy-za"]

stream_deck_ns = cg.esphome_ns.namespace("stream_deck")
StreamDeckComponent = stream_deck_ns.class_("StreamDeckComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StreamDeckComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # USB Host mode (needed to talk to the Stream Deck) needs add_idf_component()
    # below, which relies on the esp-idf build path rather than the Arduino
    # framework's (arduino-esp32, itself built on ESP-IDF, doesn't set up the
    # managed-component/idf_component.yml machinery the same way).
    if CORE.using_arduino:
        raise cv.Invalid(
            "stream_deck requires 'esp32: framework: type: esp-idf' (not supported "
            "under the Arduino framework)"
        )

    # HID Host driver on top of the USB Host Library. Not on the ESP
    # Component Registry under a plain add_idf_component() call (that helper
    # is for git-sourced components), so pull it from its actual home,
    # espressif/esp-usb, by subdirectory path instead. See docs/protocol.md
    # for why this component (rather than raw usb_host.h calls) is the
    # right level to build on.
    esp32.add_idf_component(
        name="usb_host_hid",
        repo="https://github.com/espressif/esp-usb.git",
        path="host/class/hid/usb_host_hid",
        ref="master",
    )

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
