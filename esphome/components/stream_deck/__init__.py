import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@thatguy-za"]

CONF_MODEL = "model"

stream_deck_ns = cg.esphome_ns.namespace("stream_deck")
StreamDeckComponent = stream_deck_ns.class_("StreamDeckComponent", cg.Component)
StreamDeckModel = stream_deck_ns.enum("StreamDeckModel", is_class=True)

# One entry per protocol family (not per PID - e.g. "Mini" and "Mini Mk2" are
# the same protocol/key layout, just different PIDs; likewise MK.2 and
# Original V2 - see docs/protocol.md). Which one actually connects is still
# auto-detected by PID at runtime; this tells the firmware what to *expect*
# (key count/layout, and eventually image format in M2) since that has to be
# known at compile time to generate the right number of key entities later.
MODELS = {
    "mini": StreamDeckModel.MODEL_MINI,
    "original": StreamDeckModel.MODEL_ORIGINAL,
    "original_v2": StreamDeckModel.MODEL_ORIGINAL_V2,
    "mk2": StreamDeckModel.MODEL_ORIGINAL_V2,  # alias - MK.2 is the Original V2 protocol
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StreamDeckComponent),
        cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
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
    cg.add(var.set_model(config[CONF_MODEL]))
