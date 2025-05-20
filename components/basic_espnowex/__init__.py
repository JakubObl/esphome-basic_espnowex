import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS
from esphome import automation

# Dodanie definicji std_array, której brakuje w codegen
std_array = cg.std_ns.class_("array")
cg.std_array = std_array

basic_espnowex_ns = cg.esphome_ns.namespace("espnow")
BasicESPNowEx = basic_espnowex_ns.class_("BasicESPNowEx", cg.Component)
OnMessageTrigger = basic_espnowex_ns.class_(
    "OnMessageTrigger", 
    automation.Trigger.template(cg.std_vector.template(cg.uint8), cg.std_array.template(cg.uint8, 6))
)

OnRecvAckTrigger = basic_espnowex_ns.class_(
    "OnRecvAckTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6))
)
OnRecvCmdTrigger = basic_espnowex_ns.class_(
    "OnRecvCmdTrigger", 
    automation.Trigger.template(cg.std_array.template(cg.uint8, 6), cg.int16)
)

CONF_PEER_MAC = "peer_mac"
CONF_ON_MESSAGE = "on_message"
CONF_ON_RECV_ACK = "on_recv_ack"
CONF_ON_RECV_CMD = "on_recv_cmd"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BasicESPNowEx),
    cv.Optional(CONF_PEER_MAC): cv.mac_address,
    cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({cv.GenerateID(): cv.declare_id(OnMessageTrigger)}),
    cv.Optional(CONF_ON_RECV_ACK): automation.validate_automation({cv.GenerateID(): cv.declare_id(OnRecvAckTrigger)}),
    cv.Optional(CONF_ON_RECV_CMD): automation.validate_automation({cv.GenerateID(): cv.declare_id(OnRecvCmdTrigger)}),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_PEER_MAC in config:
        mac_str = config[CONF_PEER_MAC].to_string()
        mac_ints = [int(x, 16) for x in mac_str.split(":")]
        mac_expr = cg.RawExpression(f"std::array<uint8_t, 6>{{{', '.join(map(str, mac_ints))}}}")
        cg.add(var.set_peer_mac(mac_expr))
    
    #cg.register_method(
    #    var,
    #    "send_espnow_cmd",
    #    [
    #        (cg.int16, "cmd"),
    #        (cg.std_array.template(cg.uint8, 6), "mac")
    #    ],
    #    "void"
    #)
    #cg.register_method(
    #    var,
    #    "send_espnow_ex",
    #    [
    #        (cg.std_vector.template(cg.uint8), "message"),
    #        (cg.std_array.template(cg.uint8, 6), "mac")
    #    ],
    #    "void"
    #)

    if CONF_ON_MESSAGE in config:
        for conf in config[CONF_ON_MESSAGE]:
            trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var)
            await cg.register_component(trigger, conf)
            await automation.build_automation(
                trigger, 
                [
                    (cg.std_vector.template(cg.uint8), "message"),
                    (cg.std_array.template(cg.uint8, 6), "mac")
                ], 
                conf
            )
            cg.add(var.add_on_message_trigger(trigger))
    if CONF_ON_RECV_ACK in config:
        for conf in config[CONF_ON_RECV_ACK]:
            trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var)
            await cg.register_component(trigger, conf)
            await automation.build_automation(
                trigger, 
                [(cg.std_array.template(cg.uint8, 6), "mac")], 
                conf
            )
            cg.add(var.add_on_recv_ack_trigger(trigger))
    if CONF_ON_RECV_CMD in config:
        for conf in config[CONF_ON_RECV_CMD]:
            trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var)
            await cg.register_component(trigger, conf)
            await automation.build_automation(
                trigger, 
                [
                    (cg.std_array.template(cg.uint8, 6), "mac"),
                    (cg.int16, "cmd")
                ], 
                conf
            )
            cg.add(var.add_on_recv_cmd_trigger(trigger))

    return var
