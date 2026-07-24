import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, font
from esphome.const import CONF_ENTITY_ID, CONF_ICON, CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["esp32", "api"]
CODEOWNERS = ["@thatguy-za"]
# StreamDeckCanvas subclasses display::DisplayBuffer internally (see
# stream_deck_canvas.h) even though nothing in our own YAML config declares
# a display: platform - without AUTO_LOAD, ESPHome won't bundle that
# component's headers into the build at all.
AUTO_LOAD = ["display"]

CONF_MODEL = "model"
CONF_FONT_ID = "font_id"
CONF_KEYS = "keys"
CONF_KEY_INDEX = "key_index"
CONF_ON_COLOR = "on_color"
CONF_OFF_COLOR = "off_color"

stream_deck_ns = cg.esphome_ns.namespace("stream_deck")
StreamDeckComponent = stream_deck_ns.class_("StreamDeckComponent", cg.Component)
StreamDeckKey = stream_deck_ns.class_("StreamDeckKey")
StreamDeckModel = stream_deck_ns.enum("StreamDeckModel", is_class=True)
IconType = stream_deck_ns.enum("IconType", is_class=True)

# One entry per protocol family (not per PID - e.g. "Mini" and "Mini Mk2" are
# the same protocol/key layout, just different PIDs; likewise MK.2 and
# Original V2 - see docs/protocol.md). Which one actually connects is still
# auto-detected by PID at runtime; this tells the firmware what to *expect*
# (key count/layout) since that has to be known at compile time to validate
# key_index below and size the render canvas.
MODELS = {
    "mini": StreamDeckModel.MODEL_MINI,
    "original": StreamDeckModel.MODEL_ORIGINAL,
    "original_v2": StreamDeckModel.MODEL_ORIGINAL_V2,
    "mk2": StreamDeckModel.MODEL_ORIGINAL_V2,  # alias - MK.2 is the Original V2 protocol
}

# Mirrors kStreamdeckProfiles in stream_deck.cpp - kept here too since
# key_index range validation has to happen at config-validate time, before
# any C++ exists to ask. Plain equality checks rather than a dict, since the
# resolved model value is a MockObj (codegen enum placeholder) and isn't
# hashable.
def _model_key_count(model_value):
    if model_value == StreamDeckModel.MODEL_MINI:
        return 6
    return 15  # ORIGINAL and ORIGINAL_V2 both have 15 keys

# Vector-drawn icon shapes (see icons.h/.cpp) - not real Material Design
# Icons glyphs, deliberately: those need specific codepoints baked in via
# ESPHome's font:/glyphs: mechanism at compile time, which means either an
# extra network fetch of the MDI webfont at build time or asking the user
# to hand-declare a font: block per icon. Drawing a handful of simple
# shapes ourselves needs neither.
ICONS = {
    "lightbulb": IconType.LIGHTBULB,
    "toggle_switch": IconType.TOGGLE_SWITCH,
    "fan": IconType.FAN,
    "script": IconType.SCRIPT,
    "scene": IconType.SCENE,
    "generic": IconType.GENERIC,
}

# Home Assistant entity domain -> default icon (used when a key doesn't set
# an explicit `icon:` override). Mirrors icon_for_domain() in icons.cpp -
# kept in sync manually; both are small.
DOMAIN_ICONS = {
    "light": "lightbulb",
    "switch": "toggle_switch",
    "input_boolean": "toggle_switch",
    "fan": "fan",
    "script": "script",
    "scene": "scene",
}


def _icon_for_key(key_conf):
    if CONF_ICON in key_conf:
        return ICONS[key_conf[CONF_ICON]]
    domain = key_conf[CONF_ENTITY_ID].split(".")[0]
    return ICONS[DOMAIN_ICONS.get(domain, "generic")]


KEY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StreamDeckKey),
        cv.Required(CONF_KEY_INDEX): cv.positive_int,
        cv.Required(CONF_ENTITY_ID): cv.string,
        cv.Optional(CONF_ICON): cv.one_of(*ICONS, lower=True),
        cv.Optional(CONF_ON_COLOR, default=0x00FF00): cv.int_range(min=0, max=0xFFFFFF),
        cv.Optional(CONF_OFF_COLOR, default=0x808080): cv.int_range(min=0, max=0xFFFFFF),
    }
)


def _validate_stream_deck(config):
    max_keys = _model_key_count(config[CONF_MODEL])
    for key_conf in config.get(CONF_KEYS, []):
        if key_conf[CONF_KEY_INDEX] >= max_keys:
            raise cv.Invalid(
                f"key_index {key_conf[CONF_KEY_INDEX]} is out of range for this model "
                f"({max_keys} keys, valid range 0-{max_keys - 1})",
                path=[CONF_KEYS],
            )
    if config.get(CONF_KEYS) and CONF_FONT_ID not in config:
        raise cv.Invalid("font_id is required when keys: is set (used to draw the entity name label)")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StreamDeckComponent),
            cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
            cv.Optional(CONF_FONT_ID): cv.use_id(font.Font),
            cv.Optional(CONF_KEYS, default=[]): cv.ensure_list(KEY_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_stream_deck,
)


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

    if config[CONF_KEYS]:
        # CustomAPIDevice's subscribe_homeassistant_state()/
        # call_homeassistant_service() are static_assert-guarded on these -
        # check here instead, so a missing flag is a clear config error
        # rather than a cryptic C++ build failure.
        api_conf = CORE.config.get("api") or {}
        if not api_conf.get("homeassistant_states"):
            raise cv.Invalid("stream_deck: keys: requires 'api: homeassistant_states: true'")
        if not api_conf.get("homeassistant_services"):
            raise cv.Invalid("stream_deck: keys: requires 'api: homeassistant_services: true'")
        cg.add_define("USE_API_HOMEASSISTANT_STATES")
        cg.add_define("USE_API_HOMEASSISTANT_SERVICES")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))

    if CONF_FONT_ID in config:
        font_var = await cg.get_variable(config[CONF_FONT_ID])
        cg.add(var.set_font(font_var))

    for key_conf in config[CONF_KEYS]:
        key_var = cg.new_Pvariable(key_conf[CONF_ID])
        cg.add(key_var.set_key_index(key_conf[CONF_KEY_INDEX]))
        cg.add(key_var.set_entity_id(key_conf[CONF_ENTITY_ID]))
        cg.add(key_var.set_icon(_icon_for_key(key_conf)))
        cg.add(key_var.set_on_color(key_conf[CONF_ON_COLOR]))
        cg.add(key_var.set_off_color(key_conf[CONF_OFF_COLOR]))
        cg.add(var.add_key(key_var))
