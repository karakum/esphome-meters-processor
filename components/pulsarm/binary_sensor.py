import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, DEVICE_CLASS_CONNECTIVITY

from . import PulsarMComponent
from .const import CONF_PULSARM_ID

DEPENDENCIES = ["pulsarm"]

CONF_CONNECTION = "connection"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_PULSARM_ID): cv.use_id(PulsarMComponent),
            cv.Required(CONF_CONNECTION): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PULSARM_ID])

    sens = await binary_sensor.new_binary_sensor(config[CONF_CONNECTION])
    cg.add(parent.set_connection_binary_sensor(sens))
