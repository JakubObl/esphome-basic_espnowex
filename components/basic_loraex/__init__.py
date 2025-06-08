import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID, CONF_TRIGGER_ID, CONF_FREQUENCY, 
    CONF_CS_PIN, CONF_RST_PIN, CONF_SPI_ID
)
from esphome import automation, pins
from esphome.components import spi

# Dodanie definicji std_array, której brakuje w codegen
std_array = cg.std_ns.class_("array")
cg.std_array = std_array

DEPENDENCIES = ["spi"]

basic_loraex_ns = cg.esphome_ns.namespace("lora")
BasicLoRaEx = basic_loraex_ns.class_("BasicLoRaEx", cg.Component, spi.SPIDevice)

OnMessageTrigger = basic_loraex_ns.class_(
    "OnMessageTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_string),
    cg.Component,
)

OnRecvAckTrigger = basic_loraex_ns.class_(
    "OnRecvAckTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_array.template(cg.uint8, 3)),
    cg.Component,
)

OnRecvCmdTrigger = basic_loraex_ns.class_(
    "OnRecvCmdTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16),
    cg.Component,
)

OnRecvDataTrigger = basic_loraex_ns.class_(
    "OnRecvDataTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_vector.template(cg.uint8)),
    cg.Component,
)

# Konfiguracja pinów LoRa
CONF_DIO0_PIN = "dio0_pin"
CONF_DIO1_PIN = "dio1_pin"
CONF_DIO2_PIN = "dio2_pin"

# Parametry LoRa specyficzne dla SX1278
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_BANDWIDTH = "bandwidth"
CONF_CODING_RATE = "coding_rate"
CONF_TX_POWER = "tx_power"
CONF_SYNC_WORD = "sync_word"
CONF_PREAMBLE_LENGTH = "preamble_length"
CONF_ENABLE_CRC = "enable_crc"
CONF_IMPLICIT_HEADER = "implicit_header"

# Parametry komunikacji
CONF_PEER_MAC = "peer_mac"
CONF_MAX_RETRIES = "max_retries"
CONF_TIMEOUT_US = "timeout_us"
CONF_ON_MESSAGE = "on_message"
CONF_ON_RECV_DATA = "on_recv_data"
CONF_ON_RECV_ACK = "on_recv_ack"
CONF_ON_RECV_CMD = "on_recv_cmd"

def validate_frequency(value):
    """Walidacja częstotliwości LoRa"""
    freq = cv.frequency(value)
    # Sprawdź obsługiwane zakresy SX1278
    if not (137000000 <= freq <= 1020000000):
        raise cv.Invalid("Frequency must be between 137MHz and 1020MHz")
    return int(freq)

def validate_spreading_factor(value):
    """Walidacja spreading factor (6-12)"""
    sf = cv.int_range(min=6, max=12)(value)
    return sf

def validate_bandwidth(value):
    """Walidacja bandwidth"""
    valid_bw = [7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000]
    if value not in valid_bw:
        raise cv.Invalid(f"Bandwidth must be one of: {valid_bw}")
    return value

def validate_coding_rate(value):
    """Walidacja coding rate (5-8 dla 4/5-4/8)"""
    cr = cv.int_range(min=5, max=8)(value)
    return cr

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(BasicLoRaEx),
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DIO0_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DIO1_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DIO2_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_RST_PIN): pins.gpio_output_pin_schema,
        
        # Parametry RF LoRa
        cv.Optional(CONF_FREQUENCY, default=433000000): validate_frequency,
        cv.Optional(CONF_SPREADING_FACTOR, default=9): validate_spreading_factor,
        cv.Optional(CONF_BANDWIDTH, default=125000): validate_bandwidth,
        cv.Optional(CONF_CODING_RATE, default=7): validate_coding_rate,
        cv.Optional(CONF_TX_POWER, default=14): cv.int_range(min=2, max=20),
        cv.Optional(CONF_SYNC_WORD, default=0x12): cv.hex_uint8_t,
        cv.Optional(CONF_PREAMBLE_LENGTH, default=8): cv.positive_int,
        cv.Optional(CONF_ENABLE_CRC, default=True): cv.boolean,
        cv.Optional(CONF_IMPLICIT_HEADER, default=False): cv.boolean,
        
        # Parametry komunikacji (zgodność z ESP-NOW)
        cv.Optional(CONF_PEER_MAC): cv.mac_address,
        cv.Optional(CONF_MAX_RETRIES, default=5): cv.positive_int,
        cv.Optional(CONF_TIMEOUT_US, default=200000): cv.positive_int,
        
        # Triggery automatyzacji
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnMessageTrigger)
        }),
        cv.Optional(CONF_ON_RECV_ACK): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvAckTrigger)
        }),
        cv.Optional(CONF_ON_RECV_DATA): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvDataTrigger)
        }),
        cv.Optional(CONF_ON_RECV_CMD): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvCmdTrigger)
        }),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema())
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    # Konfiguracja pinów
    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))
    
    dio0_pin = await cg.gpio_pin_expression(config[CONF_DIO0_PIN])
    cg.add(var.set_dio0_pin(dio0_pin))
    
    if CONF_DIO1_PIN in config:
        dio1_pin = await cg.gpio_pin_expression(config[CONF_DIO1_PIN])
        cg.add(var.set_dio1_pin(dio1_pin))
        
    if CONF_DIO2_PIN in config:
        dio2_pin = await cg.gpio_pin_expression(config[CONF_DIO2_PIN])
        cg.add(var.set_dio2_pin(dio2_pin))
    
    if CONF_RST_PIN in config:
        rst_pin = await cg.gpio_pin_expression(config[CONF_RST_PIN])
        cg.add(var.set_rst_pin(rst_pin))

    # Parametry LoRa
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_sync_word(config[CONF_SYNC_WORD]))
    cg.add(var.set_preamble_length(config[CONF_PREAMBLE_LENGTH]))
    cg.add(var.set_enable_crc(config[CONF_ENABLE_CRC]))
    cg.add(var.set_implicit_header(config[CONF_IMPLICIT_HEADER]))

    # Parametry komunikacji
    if CONF_PEER_MAC in config:
        mac_str = config[CONF_PEER_MAC].to_string()
        mac_ints = [int(x, 16) for x in mac_str.split(":")]
        mac_expr = cg.RawExpression(f"std::array<uint8_t, 6>{{{', '.join(map(str, mac_ints))}}}")
        cg.add(var.set_peer_mac(mac_expr))

    cg.add(var.set_max_retries(config[CONF_MAX_RETRIES]))
    cg.add(var.set_timeout_us(config[CONF_TIMEOUT_US]))

    # Triggery automatyzacji
    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_string, "message")],
            conf,
        )

    for conf in config.get(CONF_ON_RECV_CMD, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.int16, "cmd")],
            conf,
        )

    for conf in config.get(CONF_ON_RECV_ACK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_array.template(cg.uint8, 3), "msg_id")],
            conf,
        )

    for conf in config.get(CONF_ON_RECV_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_vector.template(cg.uint8), "data")],
            conf,
        )

    return var
