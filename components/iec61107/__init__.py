import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.uart import CONF_STOP_BITS, CONF_DATA_BITS, CONF_PARITY, UART_PARITY_OPTIONS
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS, CONF_BAUD_RATE, CONF_DAY, CONF_PASSWORD
)

from .const import CONF_IEC61107_ID, CONF_IS_ENABLE
from ..counters_processor import CONF_PROCESSOR_ID, CountersProcessor, register_counter_device

DEPENDENCIES = ["counters_processor"]
MULTI_CONF = True

iec61107_component_ns = cg.esphome_ns.namespace("iec61107")
Iec61107Component = iec61107_component_ns.class_("Iec61107Component", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Iec61107Component),
            cv.GenerateID(CONF_PROCESSOR_ID): cv.use_id(CountersProcessor),
            cv.Required(CONF_ADDRESS): cv.string,
            cv.Required(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_IS_ENABLE): cv.returning_lambda,
            cv.Optional(CONF_DAY, default=25): cv.int_range(min=1, max=28),
            cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
            cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                UART_PARITY_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_counter_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_password(config[CONF_PASSWORD]))

    if CONF_IS_ENABLE in config:
        template_ = await cg.process_lambda(
            config[CONF_IS_ENABLE],
            [],
            return_type=cg.optional.template(bool),
        )
        cg.add(var.set_is_enable_lambda(template_))

    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))

    cg.add(var.set_indication_day(config[CONF_DAY]))
