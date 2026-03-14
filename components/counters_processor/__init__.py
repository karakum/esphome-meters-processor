import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']

CONF_PROCESSOR_ID = "processor_id"

counters_processor_component_ns = cg.esphome_ns.namespace("counters_processor")
CountersProcessor = counters_processor_component_ns.class_("CountersProcessor", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CountersProcessor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def register_counter_device(var, config):
    parent = await cg.get_variable(config[CONF_PROCESSOR_ID])
    cg.add(parent.add_counter_device(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
