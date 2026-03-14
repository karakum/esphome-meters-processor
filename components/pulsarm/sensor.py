import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import DEVICE_CLASS_DURATION, UNIT_SECOND, ENTITY_CATEGORY_DIAGNOSTIC, CONF_CHANNELS

from . import PulsarMComponent
from .const import CONF_PULSARM_ID, CONF_PULSARM_CHANNEL, CONF_DATETIME_DIFF, CONF_CHANNELS_INDI, CONF_VOLUME_LAST_DAY, \
    CONF_VOLUME_LAST_HOUR

DEPENDENCIES = ["pulsarm"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_PULSARM_ID): cv.use_id(PulsarMComponent),
            cv.Required(CONF_CHANNELS): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Required(CONF_PULSARM_CHANNEL): cv.int_range(1, 10)
                    })
                )
            ),
            cv.Required(CONF_CHANNELS_INDI): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Required(CONF_PULSARM_CHANNEL): cv.int_range(1, 10)
                    })
                )
            ),
            cv.Required(CONF_VOLUME_LAST_DAY): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Required(CONF_PULSARM_CHANNEL): cv.int_range(1, 10)
                    })
                )
            ),
            cv.Required(CONF_VOLUME_LAST_HOUR): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Required(CONF_PULSARM_CHANNEL): cv.int_range(1, 10)
                    })
                )
            ),
            cv.Optional(CONF_DATETIME_DIFF): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                device_class=DEVICE_CLASS_DURATION,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PULSARM_ID])

    if CONF_DATETIME_DIFF in config:
        sens = await sensor.new_sensor(config[CONF_DATETIME_DIFF])
        cg.add(paren.set_datetime_diff_sensor(sens))

    for ch in config[CONF_CHANNELS]:
        sens = await sensor.new_sensor(ch)
        cg.add(paren.add_channel(ch[CONF_PULSARM_CHANNEL], sens))

    for ch in config[CONF_CHANNELS_INDI]:
        sens = await sensor.new_sensor(ch)
        cg.add(paren.add_channel_indi(ch[CONF_PULSARM_CHANNEL], sens))

    for ch in config[CONF_VOLUME_LAST_DAY]:
        sens = await sensor.new_sensor(ch)
        cg.add(paren.add_volume_last_day(ch[CONF_PULSARM_CHANNEL], sens))

    for ch in config[CONF_VOLUME_LAST_HOUR]:
        sens = await sensor.new_sensor(ch)
        cg.add(paren.add_volume_last_hour(ch[CONF_PULSARM_CHANNEL], sens))
