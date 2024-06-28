import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY

AUTO_LOAD = ["json"]

neptune_r900_sensor_ns = cg.esphome_ns.namespace("neptune_r900_sensor")
NeptuneR900Sensor = neptune_r900_sensor_ns.class_(
    "NeptuneR900Sensor", cg.PollingComponent
)

CONF_CONSuMPTION = "consumption"
CONF_NOUSE = "nouse"
CONF_BACKFLOW = "backflow"
CONF_METERID = "meter_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NeptuneR900Sensor),
        cv.Optional(CONF_CONSuMPTION): sensor.sensor_schema(
            unit_of_measurement="gal", icon=ICON_EMPTY, accuracy_decimals=1
        ),
        cv.Optional(CONF_NOUSE): sensor.sensor_schema(
            unit_of_measurement="Days", icon=ICON_EMPTY, accuracy_decimals=0
        ),
        cv.Optional(CONF_BACKFLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY, icon=ICON_EMPTY, accuracy_decimals=0
        ),
        cv.Optional(CONF_METERID, default=0): cv.uint32_t,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_library('rtl_433_ESP', None, 'https://github.com/Entropy512/rtl_433_ESP#neptune_r900')
    #cg.add_library('rtl_433_ESP', None, 'file:///home/adodd/gitrepos/rtl_433_ESP')
    cg.add_library('RadioLib', None)


    cg.add(var.set_meterid(int(config[CONF_METERID])))
    if consumption_config := config.get(CONF_CONSuMPTION):
        sens = await sensor.new_sensor(consumption_config)
        cg.add(var.set_consumption(sens))

    if nouse_config := config.get(CONF_NOUSE):
        sens = await sensor.new_sensor(nouse_config)
        cg.add(var.set_nouse(sens))

    if backflow_config := config.get(CONF_BACKFLOW):
        sens = await sensor.new_sensor(backflow_config)
        cg.add(var.set_backflow(sens))
