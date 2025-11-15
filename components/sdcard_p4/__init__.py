"""
SD Card component for ESP32-P4 with SDMMC support.

FEATURES:
- 4-bit and 1-bit SDIO bus modes
- Configurable frequency
- Custom mount point
- ESP-IDF framework support
"""

import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

# Component metadata
DOMAIN = "sdcard_p4"
CODEOWNERS = ["@alaltitov"]
DEPENDENCIES = ["esp32"]

# C++ namespace and classes
sdcard_p4_ns = cg.esphome_ns.namespace("sdcard_p4")
SDCardComponent = sdcard_p4_ns.class_("SDCardComponent", cg.Component)

# Configuration keys
CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_BUS_WIDTH = "bus_width"
CONF_MOUNT_POINT = "mount_point"
CONF_MAX_FREQ_KHZ = "max_freq_khz"

# ------------------ Validation Functions ------------------

def validate_bus_width(value):
    """Validate bus width is 1 or 4."""
    if value not in [1, 4]:
        raise cv.Invalid("bus_width must be 1 or 4")
    return value

def validate_data_pins(config):
    """Validate data pins based on bus width."""
    bus_width = config.get(CONF_BUS_WIDTH, 4)
    
    if bus_width == 4:
        required_pins = [CONF_DATA1_PIN, CONF_DATA2_PIN, CONF_DATA3_PIN]
        missing = [p for p in required_pins if p not in config]
        if missing:
            raise cv.Invalid(
                f"bus_width=4 requires pins: {', '.join(missing)}"
            )
    
    return config

# ------------------ Component Schema ------------------

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SDCardComponent),
            # CLK и CMD - output pins (возвращают номер пина)
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
            # DATA пины - bidirectional (возвращают номер пина)
            cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
            # Bus configuration
            cv.Optional(CONF_BUS_WIDTH, default=4): cv.All(
                cv.int_range(min=1, max=4), validate_bus_width
            ),
            cv.Optional(CONF_MOUNT_POINT, default="/sdcard"): cv.string,
            cv.Optional(CONF_MAX_FREQ_KHZ, default=20000): cv.int_range(
                min=400, max=40000
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_esp32_variant,
    validate_data_pins,
)

# ------------------ Code Generation ------------------

async def to_code(config):
    """Main code generation function."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add namespace to global scope
    cg.add_global(sdcard_p4_ns.using)
    cg.add_define("USE_SDCARD_P4")

    # Configure pins (internal pins - используем номера напрямую)
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    # Configure DATA1-3 pins if bus_width == 4
    if config[CONF_BUS_WIDTH] == 4:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    # Configure bus parameters
    cg.add(var.set_bus_width(config[CONF_BUS_WIDTH]))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_max_freq_khz(config[CONF_MAX_FREQ_KHZ]))

    # Log configuration
    _LOGGER.info(
        f"SD Card P4: {config[CONF_BUS_WIDTH]}-bit mode, "
        f"{config[CONF_MAX_FREQ_KHZ]} kHz, mount at {config[CONF_MOUNT_POINT]}"
    )

    # Add required ESP-IDF components
    cg.add_platformio_option("board_build.partitions", "partitions.csv")
    cg.add_build_flag("-DBOARD_HAS_PSRAM")
    
    # Add ESP-IDF libraries
    cg.add_library("ESP32", None, None)
