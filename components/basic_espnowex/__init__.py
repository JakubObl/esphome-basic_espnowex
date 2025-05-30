import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS, CONF_TRIGGER_ID, CONF_NUM_ATTEMPTS, CONF_TIMEOUT
from esphome import automation

# Dodanie definicji std_array, której brakuje w codegen
std_array = cg.std_ns.class_("array")
cg.std_array = std_array

basic_espnowex_ns = cg.esphome_ns.namespace("espnow")
BasicESPNowEx = basic_espnowex_ns.class_("BasicESPNowEx", cg.Component)

#RecvCmdTrigger = automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16)
#RecvAckTrigger = automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_array.template(cg.uint8, 3))
#RecvDataTrigger = automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_vector.template(cg.uint8))
#MessageTrigger = automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_string)

# ✨ Definicje klas triggerów dla ESPHome (tylko typy!)
MessageTrigger = basic_espnowex_ns.class_(
    "OnMessageTrigger",
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_string),
    cg.Component,
)

RecvCmdTrigger = basic_espnowex_ns.class_(
    "OnRecvCmdTrigger",
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16),
    cg.Component,
)

RecvAckTrigger = basic_espnowex_ns.class_(
    "OnRecvAckTrigger",
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_array.template(cg.uint8, 3)),
    cg.Component,
)

RecvDataTrigger = basic_espnowex_ns.class_(
    "OnRecvDataTrigger",
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_vector.template(cg.uint8)),
    cg.Component,
)

#OnMessageTrigger = basic_espnowex_ns.class_(
#    "OnMessageTrigger", 
#    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_string),
#    cg.Component,
#)

#OnRecvAckTrigger = basic_espnowex_ns.class_(
#    "OnRecvAckTrigger", 
#    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_array.template(cg.uint8, 3)),
#    cg.Component,
#)
#OnRecvCmdTrigger = basic_espnowex_ns.class_(
#    "OnRecvCmdTrigger", 
#    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16),
#    cg.Component,
#)
#OnRecvDataTrigger = basic_espnowex_ns.class_(
#    "OnRecvDataTrigger", 
#    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_vector.template(cg.uint8)),
#    cg.Component,
#)

CONF_PEER_MAC = "peer_mac"
CONF_MAX_RETRIES = "max_retries"
CONF_TIMEOUT_US = "timeout_us"
CONF_ON_MESSAGE = "on_message"
CONF_ON_RECV_DATA = "on_recv_data"
CONF_ON_RECV_ACK = "on_recv_ack"
CONF_ON_RECV_CMD = "on_recv_cmd"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BasicESPNowEx),
    cv.Optional(CONF_PEER_MAC): cv.mac_address,
    cv.Optional(CONF_MAX_RETRIES): cv.positive_int,
    cv.Optional(CONF_TIMEOUT_US): cv.positive_int,
    cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MessageTrigger)
    }),
    cv.Optional(CONF_ON_RECV_ACK): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RecvAckTrigger)
    }),
    cv.Optional(CONF_ON_RECV_CMD): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RecvCmdTrigger)
    }),
    cv.Optional(CONF_ON_RECV_DATA): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RecvDataTrigger)
    }),
    #cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_string)),
    #cv.Optional(CONF_ON_RECV_ACK): automation.validate_automation(automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_array.template(cg.uint8, 3))),
    #cv.Optional(CONF_ON_RECV_CMD): automation.validate_automation(automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16)),
    #cv.Optional(CONF_ON_RECV_DATA): automation.validate_automation(automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.std_vector.template(cg.uint8))),
    #cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnMessageTrigger)}),
    #cv.Optional(CONF_ON_RECV_ACK): automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvAckTrigger)}),
    #cv.Optional(CONF_ON_RECV_DATA): automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvDataTrigger)}),
    #cv.Optional(CONF_ON_RECV_CMD): automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnRecvCmdTrigger)}),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_PEER_MAC in config:
        mac_str = config[CONF_PEER_MAC].to_string()
        mac_ints = [int(x, 16) for x in mac_str.split(":")]
        mac_expr = cg.RawExpression(f"std::array<uint8_t, 6>{{{', '.join(map(str, mac_ints))}}}")
        cg.add(var.set_peer_mac(mac_expr))

    if CONF_MAX_RETRIES in config:
        max_retries_int = config[CONF_MAX_RETRIES].to_int()
        cg.add(var.set_max_retries(max_retries_int))

    if CONF_TIMEOUT_US in config:
        timeout_us_int = config[CONF_TIMEOUT_US].to_int()
        cg.add(var.set_timeout_us(timeout_us_int))

#    if CONF_ON_MESSAGE in config:
#        for conf in config[CONF_ON_MESSAGE]:
#            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
#          #  await cg.register_component(trigger, conf)
#            await automation.build_automation(
#                trigger, 
#                [
#                    (cg.std_array.template(cg.uint8, 6), "mac"),
#                    (cg.std_string, "message")
#                ], 
#                conf,
#            )
#            cg.add(var.add_on_message_trigger(trigger))
#    if CONF_ON_RECV_ACK in config:
#        for conf in config[CONF_ON_RECV_ACK]:
#            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
#          #  await cg.register_component(trigger, conf)
#            await automation.build_automation(
#                trigger, 
#                [
#                    (cg.std_array.template(cg.uint8, 6), "mac"),
#                    (cg.std_array.template(cg.uint8, 3), "msg_id")
#                ], 
#                conf,
#            )
#            cg.add(var.add_on_recv_ack_trigger(trigger))
#    if CONF_ON_RECV_CMD in config:
#        for conf in config.get(CONF_ON_RECV_CMD, []):
#            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
#           # await cg.register_component(trigger, conf)
#            await automation.build_automation(
#                trigger, 
#                [
#                    (cg.std_array.template(cg.uint8, 6), "mac"),
#                    (cg.int16, "cmd")
#                ], 
#                conf,
#            )
#            cg.add(var.add_on_recv_cmd_trigger(trigger))
#    if CONF_ON_RECV_DATA in config:
#        for conf in config.get(CONF_ON_RECV_DATA, []):
#            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
#           # await cg.register_component(trigger, conf)
#            await automation.build_automation(
#                trigger, 
#                [
#                    (cg.std_array.template(cg.uint8, 6), "mac"),
#                    (cg.std_vector.template(cg.uint8), "data")
#                ], 
#                conf,
#            )
#            cg.add(var.add_on_recv_data_trigger(trigger))

    if CONF_ON_MESSAGE in config:
        for conf in config[CONF_ON_MESSAGE]:
            await automation.build_automation(
                var.on_message_callback_,
                [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_string, "message")],
                conf,
            )

    if CONF_ON_RECV_ACK in config:
        for conf in config[CONF_ON_RECV_ACK]:
            await automation.build_automation(
                var.on_recv_ack_callback_,
                [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_array.template(cg.uint8, 3), "msg_id")],
                conf,
            )

    if CONF_ON_RECV_CMD in config:
        for conf in config[CONF_ON_RECV_CMD]:
            await automation.build_automation(
                var.on_recv_cmd_callback_,
                [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.int16, "cmd")],
                conf,
            )

    if CONF_ON_RECV_DATA in config:
        for conf in config[CONF_ON_RECV_DATA]:
            await automation.build_automation(
                var.on_recv_data_callback_,
                [(cg.std_array.template(cg.uint8, 6), "mac"), (cg.std_vector.template(cg.uint8), "data")],
                conf,
            )

    return var
