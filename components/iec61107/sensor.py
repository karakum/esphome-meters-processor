import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import DEVICE_CLASS_DURATION, UNIT_SECOND, ENTITY_CATEGORY_DIAGNOSTIC, CONF_ID

from . import Iec61107Component
from .const import (
    CONF_METRICS,
    CONF_IEC61107_METRIC,
    CONF_IEC61107_ID,
    CONF_IEC61107_ZONE,
    CONF_TARIFF_ZONES,
    CONF_DATETIME_DIFF, CONF_TARIFF_ZONES_INDI, CONF_VOLUME_LAST_DAY, CONF_VOLUME_LAST_HOUR,
)

DEPENDENCIES = ["iec61107"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_IEC61107_ID): cv.use_id(Iec61107Component),
            cv.Optional(CONF_METRICS): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Required(CONF_IEC61107_METRIC): cv.string
                    })
                )
            ),
            cv.Required(CONF_TARIFF_ZONES): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Optional(CONF_IEC61107_ZONE, default=0): cv.int_range(0, 4)
                    })
                )
            ),
            cv.Required(CONF_TARIFF_ZONES_INDI): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Optional(CONF_IEC61107_ZONE, default=0): cv.int_range(0, 4)
                    })
                )
            ),
            cv.Required(CONF_VOLUME_LAST_DAY): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Optional(CONF_IEC61107_ZONE, default=0): cv.int_range(0, 4)
                    })
                )
            ),
            cv.Required(CONF_VOLUME_LAST_HOUR): cv.ensure_list(
                sensor.sensor_schema()
                .extend(
                    cv.Schema({
                        cv.Optional(CONF_IEC61107_ZONE, default=0): cv.int_range(0, 4)
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
    paren = await cg.get_variable(config[CONF_IEC61107_ID])

    if CONF_DATETIME_DIFF in config:
        sens = await sensor.new_sensor(config[CONF_DATETIME_DIFF])
        cg.add(paren.set_datetime_diff_sensor(sens))

    for z in config[CONF_TARIFF_ZONES]:
        sens = await sensor.new_sensor(z)
        cg.add(paren.add_tariff_zone(z[CONF_IEC61107_ZONE], sens))

    for z in config[CONF_TARIFF_ZONES_INDI]:
        sens = await sensor.new_sensor(z)
        cg.add(paren.add_tariff_zone_indi(z[CONF_IEC61107_ZONE], sens))

    for z in config[CONF_VOLUME_LAST_DAY]:
        sens = await sensor.new_sensor(z)
        cg.add(paren.add_volume_last_day(z[CONF_IEC61107_ZONE], sens))

    for z in config[CONF_VOLUME_LAST_HOUR]:
        sens = await sensor.new_sensor(z)
        cg.add(paren.add_volume_last_hour(z[CONF_IEC61107_ZONE], sens))

    if metrics_config := config.get(CONF_METRICS):
        for s in metrics_config:
            sens = await sensor.new_sensor(s)
            cg.add(paren.add_metric(s[CONF_IEC61107_METRIC], sens))
