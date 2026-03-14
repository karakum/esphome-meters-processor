import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_DATETIME, DEVICE_CLASS_TIMESTAMP, ENTITY_CATEGORY_DIAGNOSTIC, DEVICE_CLASS_DATE

from . import PulsarMComponent
from .const import CONF_PULSARM_ID, CONF_DATE_INDICATION

DEPENDENCIES = ["pulsarm"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_PULSARM_ID): cv.use_id(PulsarMComponent),
            cv.Optional(CONF_DATETIME): text_sensor.text_sensor_schema(
                text_sensor.TextSensor,
                device_class=DEVICE_CLASS_TIMESTAMP,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_DATE_INDICATION): text_sensor.text_sensor_schema(
                text_sensor.TextSensor,
                device_class=DEVICE_CLASS_DATE,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PULSARM_ID])

    if CONF_DATETIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DATETIME])
        cg.add(parent.set_datetime_text_sensor(sens))

    if CONF_DATE_INDICATION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DATE_INDICATION])
        cg.add(parent.set_date_indication_text_sensor(sens))
