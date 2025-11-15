import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

DEPENDENCIES = []
AUTO_LOAD = []
CODEOWNERS = ["@yourusername"]

sd_card_esp32p4_ns = cg.esphome_ns.namespace("sd_card_esp32p4")
SDCardComponent = sd_card_esp32p4_ns.class_("SDCardComponent", cg.Component)

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MOUNT_POINT = "mount_point"
CONF_BUS_WIDTH = "bus_width"
CONF_MAX_FREQ_KHZ = "max_freq_khz"

BusWidth = sd_card_esp32p4_ns.enum("BusWidth")
BUS_WIDTHS = {
    1: BusWidth.BUS_WIDTH_1,
    4: BusWidth.BUS_WIDTH_4,
}

def validate_bus_width_pins(config):
    """Валидация пинов в зависимости от ширины шины"""
    bus_width = config[CONF_BUS_WIDTH]
    
    if bus_width == 4:
        # Для 4-bit режима требуются все data пины
        if CONF_DATA1_PIN not in config:
            raise cv.Invalid(f"{CONF_DATA1_PIN} is required for 4-bit bus width")
        if CONF_DATA2_PIN not in config:
            raise cv.Invalid(f"{CONF_DATA2_PIN} is required for 4-bit bus width")
        if CONF_DATA3_PIN not in config:
            raise cv.Invalid(f"{CONF_DATA3_PIN} is required for 4-bit bus width")
    elif bus_width == 1:
        # Для 1-bit режима data1-3 не должны быть указаны
        if CONF_DATA1_PIN in config:
            raise cv.Invalid(f"{CONF_DATA1_PIN} should not be specified for 1-bit bus width")
        if CONF_DATA2_PIN in config:
            raise cv.Invalid(f"{CONF_DATA2_PIN} should not be specified for 1-bit bus width")
        if CONF_DATA3_PIN in config:
            raise cv.Invalid(f"{CONF_DATA3_PIN} should not be specified for 1-bit bus width")
    
    return config

def set_default_bus_width(config):
    """Устанавливает значение по умолчанию для bus_width если не указано"""
    if CONF_BUS_WIDTH not in config:
        config = config.copy()
        config[CONF_BUS_WIDTH] = 4
    return config

def set_default_mount_point(config):
    """Устанавливает значение по умолчанию для mount_point если не указано"""
    if CONF_MOUNT_POINT not in config:
        config = config.copy()
        config[CONF_MOUNT_POINT] = "/sdcard"
    return config

def set_default_max_freq(config):
    """Устанавливает значение по умолчанию для max_freq_khz если не указано"""
    if CONF_MAX_FREQ_KHZ not in config:
        config = config.copy()
        config[CONF_MAX_FREQ_KHZ] = 20000
    return config

# ЕДИНАЯ СХЕМА С ВАЛИДАЦИЕЙ
CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(SDCardComponent),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CMD_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA0_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DATA1_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DATA2_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DATA3_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BUS_WIDTH): cv.enum(BUS_WIDTHS, int=True),
        cv.Optional(CONF_MOUNT_POINT): cv.string,
        cv.Optional(CONF_MAX_FREQ_KHZ): cv.int_range(min=400, max=40000),
    }).extend(cv.COMPONENT_SCHEMA),
    set_default_bus_width,      # Устанавливаем default=4
    set_default_mount_point,    # Устанавливаем default="/sdcard"
    set_default_max_freq,       # Устанавливаем default=20000
    validate_bus_width_pins,    # Проверка соответствия пинов
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Обязательные пины
    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    
    cmd_pin = await cg.gpio_pin_expression(config[CONF_CMD_PIN])
    cg.add(var.set_cmd_pin(cmd_pin))
    
    data0_pin = await cg.gpio_pin_expression(config[CONF_DATA0_PIN])
    cg.add(var.set_data0_pin(data0_pin))
    
    # Опциональные data пины (для 4-bit режима)
    if CONF_DATA1_PIN in config:
        data1_pin = await cg.gpio_pin_expression(config[CONF_DATA1_PIN])
        cg.add(var.set_data1_pin(data1_pin))
    
    if CONF_DATA2_PIN in config:
        data2_pin = await cg.gpio_pin_expression(config[CONF_DATA2_PIN])
        cg.add(var.set_data2_pin(data2_pin))
    
    if CONF_DATA3_PIN in config:
        data3_pin = await cg.gpio_pin_expression(config[CONF_DATA3_PIN])
        cg.add(var.set_data3_pin(data3_pin))
    
    # Настройки (теперь всегда присутствуют благодаря set_default_*)
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_bus_width(config[CONF_BUS_WIDTH]))
    cg.add(var.set_max_freq_khz(config[CONF_MAX_FREQ_KHZ]))
    
    # Добавляем необходимые флаги сборки
    cg.add_build_flag("-DCONFIG_VFS_SUPPORT_DIR=1")
    cg.add_build_flag("-DCONFIG_FATFS_LFN_HEAP=1")
