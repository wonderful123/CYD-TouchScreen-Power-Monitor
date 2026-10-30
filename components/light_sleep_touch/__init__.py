from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INVERTED, CONF_TIMEOUT

DEPENDENCIES = ["esp32"]

light_sleep_touch_ns = cg.esphome_ns.namespace("light_sleep_touch")

LightSleepTouchComponent = light_sleep_touch_ns.class_(
    "LightSleepTouchComponent",
    cg.Component,
)

CONF_TOUCH_IRQ_PIN = "touch_irq_pin"
CONF_SLEEP_DURATION = "sleep_duration"
CONF_ON_BEFORE_SLEEP = "on_before_sleep"
CONF_ON_WAKE = "on_wake"


def validate_touch_irq_pin(config):
    # Wake hardware uses the physical low level; inverted reads would disagree.
    if config[CONF_INVERTED]:
        raise cv.Invalid("touch_irq_pin must not be inverted (PENIRQ is active low)")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LightSleepTouchComponent),
        cv.Required(CONF_TOUCH_IRQ_PIN): cv.All(
            pins.internal_gpio_input_pullup_pin_schema, validate_touch_irq_pin
        ),
        cv.Optional(CONF_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SLEEP_DURATION, default="30s"): cv.All(
            cv.positive_not_null_time_period, cv.positive_time_period_milliseconds
        ),
        cv.Optional(CONF_ON_BEFORE_SLEEP): automation.validate_automation({}),
        cv.Optional(CONF_ON_WAKE): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    irq_pin = await cg.gpio_pin_expression(config[CONF_TOUCH_IRQ_PIN])

    cg.add(var.set_touch_irq_pin(irq_pin))
    cg.add(var.set_inactivity_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))

    for conf in config.get(CONF_ON_BEFORE_SLEEP, []):
        await automation.build_callback_automation(
            var,
            "add_on_before_sleep_callback",
            [],
            conf,
        )

    for conf in config.get(CONF_ON_WAKE, []):
        await automation.build_callback_automation(
            var,
            "add_on_wake_callback",
            [],
            conf,
        )
