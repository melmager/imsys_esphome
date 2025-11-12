import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL
from esphome.components import spi
from esphome import pins

DEPENDENCIES = ['spi']
AUTO_LOAD = ['spi']

wiznet_ns = cg.esphome_ns.namespace('wiznet5k')
WIZNET5KComponent = wiznet_ns.class_('WIZNET5KComponent', cg.PollingComponent, spi.SPIDevice)

def _ipv4_validator(value):
    if not isinstance(value, str):
        raise cv.Invalid("IP address must be a string like '192.168.1.10'")
    parts = value.split('.')
    if len(parts) != 4:
        raise cv.Invalid("IP address must have 4 octets")
    try:
        vals = [int(x) for x in parts]
    except Exception:
        raise cv.Invalid("IP address octets must be integers")
    for v in vals:
        if v < 0 or v > 255:
            raise cv.Invalid("IP address octets must be between 0 and 255")
    return value

def _mac_validator(value):
    if not isinstance(value, str):
        raise cv.Invalid("MAC must be string like 'DE:AD:BE:EF:FE:ED'")
    sep = ':' if ':' in value else '-' if '-' in value else None
    if sep is None:
        raise cv.Invalid("MAC must use ':' or '-' as separator")
    parts = value.split(sep)
    if len(parts) != 6:
        raise cv.Invalid("MAC must have 6 octets")
    try:
        [int(x, 16) for x in parts]
    except Exception:
        raise cv.Invalid("MAC octets must be hex numbers")
    return value

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WIZNET5KComponent),
    cv.Optional('cs_pin'): cv.gpio_output_pin,
    cv.Optional('reset_pin'): cv.gpio_output_pin,
    cv.Optional('static_ip'): _ipv4_validator,
    cv.Optional('subnet_mask'): _ipv4_validator,
    cv.Optional('gateway'): _ipv4_validator,
    cv.Optional('mac_address'): _mac_validator,
    cv.Optional('debug', default=False): cv.boolean,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    # Register SPI device (will accept cs_pin in config)
    await spi.register_spi_device(var, config)

    if 'reset_pin' in config:
        reset = await cg.gpio_pin_expression(config['reset_pin'])
        cg.add(var.set_reset_pin(reset))

    if config.get('debug'):
        cg.add(var.set_debug(True))

    def add_ip_setter(cfg_key, setter_name):
        if cfg_key in config:
            s = config[cfg_key]
            parts = [int(x) for x in s.split('.')]
            cg.add(getattr(var, setter_name)(parts[0], parts[1], parts[2], parts[3]))

    add_ip_setter('static_ip', 'set_static_ip')
    add_ip_setter('subnet_mask', 'set_subnet_mask')
    add_ip_setter('gateway', 'set_gateway')

    if 'mac_address' in config:
        mac = config['mac_address']
        sep = ':' if ':' in mac else '-'
        parts = [int(x, 16) for x in mac.split(sep)]
        cg.add(var.set_mac(parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]))
