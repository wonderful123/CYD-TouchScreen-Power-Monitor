import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = []
MULTI_CONF = False

pwr_ns = cg.esphome_ns.namespace("pwr")
PowerMonitorComponent = pwr_ns.class_("PowerMonitorComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PowerMonitorComponent),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_variable(config[CONF_ID], PowerMonitorComponent())
    await cg.register_component(var, config)