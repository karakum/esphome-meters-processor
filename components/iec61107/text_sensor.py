import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.components.sun_gtil2.text_sensor import CONF_SERIAL_NUMBER
from esphome.const import CONF_DATETIME, DEVICE_CLASS_TIMESTAMP, ENTITY_CATEGORY_DIAGNOSTIC, DEVICE_CLASS_DATE

from . import Iec61107Component
from .const import CONF_STATE_FLAGS, CONF_IEC61107_ID, CONF_DATE_INDICATION

DEPENDENCIES = ["iec61107"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_IEC61107_ID): cv.use_id(Iec61107Component),
            cv.Optional(CONF_DATETIME): text_sensor.text_sensor_schema(
                text_sensor.TextSensor,
                device_class=DEVICE_CLASS_TIMESTAMP,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                text_sensor.TextSensor,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STATE_FLAGS): text_sensor.text_sensor_schema(
                text_sensor.TextSensor,
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
    parent = await cg.get_variable(config[CONF_IEC61107_ID])

    if CONF_DATETIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DATETIME])
        cg.add(parent.set_datetime_text_sensor(sens))
    if CONF_SERIAL_NUMBER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])
        cg.add(parent.set_serial_number_text_sensor(sens))
    if CONF_STATE_FLAGS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATE_FLAGS])
        cg.add(parent.set_flags_text_sensor(sens))

    if CONF_DATE_INDICATION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DATE_INDICATION])
        cg.add(parent.set_date_indication_text_sensor(sens))
