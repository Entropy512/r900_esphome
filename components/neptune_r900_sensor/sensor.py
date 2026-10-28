import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_INVERT,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_WATER,
    STATE_CLASS_TOTAL_INCREASING
)

DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

neptune_ns = cg.esphome_ns.namespace("neptune_r900_sensor")
NeptuneR900Sensor = neptune_ns.class_("NeptuneR900Sensor", cg.Component)

CONF_METER_ID = "meter_id"
CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_CS_PIN = "cs_pin"
CONF_GDO0_PIN = "gdo0_pin"
CONF_CONSUMPTION = "consumption"
CONF_NOUSE = "nouse"
CONF_BACKFLOW = "backflow"
CONF_LEAK = "leak"
CONF_LEAK_NOW = "leak_now"
CONF_RSSI = "rssi"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NeptuneR900Sensor),
        cv.Required(CONF_METER_ID): cv.uint32_t,
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_GDO0_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_SCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MISO_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_FREQUENCY, default=912071010.0): cv.frequency,
        cv.Optional(CONF_INVERT, default=False): cv.boolean,
        cv.Optional(CONF_CONSUMPTION): sensor.sensor_schema(
            unit_of_measurement="gal",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_NOUSE): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_BACKFLOW): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_LEAK): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_LEAK_NOW): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement="dBm", accuracy_decimals=0
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_library("SPI", None)
    await cg.register_component(var, config)
    cg.add(var.set_meter_id(config[CONF_METER_ID]))
    cg.add(var.set_pins(
        await cg.gpio_pin_expression(config[CONF_CS_PIN]),
        await cg.gpio_pin_expression(config[CONF_GDO0_PIN]),
        await cg.gpio_pin_expression(config[CONF_SCK_PIN]),
        await cg.gpio_pin_expression(config[CONF_MOSI_PIN]),
        await cg.gpio_pin_expression(config[CONF_MISO_PIN]),
    ))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_invert(config[CONF_INVERT]))

    for key, setter in (
        (CONF_CONSUMPTION, "set_consumption_sensor"),
        (CONF_NOUSE, "set_nouse_sensor"),
        (CONF_BACKFLOW, "set_backflow_sensor"),
        (CONF_LEAK, "set_leak_sensor"),
        (CONF_LEAK_NOW, "set_leak_now_sensor"),
        (CONF_RSSI, "set_rssi_sensor"),
    ):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))
