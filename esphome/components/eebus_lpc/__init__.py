"""ESPHome external component: eebus_lpc

Implements the EEBus SHIP/SPINE LPC (Limitation of Power Consumption)
CS (Controllable System) actor for §14a EnWG compliance.

The component connects to a Westnetz CLS-Steuerbox (acting as EG / Energy
Guard) via EEBus over the local network and exposes the received power limit
as a sensor and automation trigger.

Example YAML:
    external_components:
      - source: github://bgewehr/openeebus@bg-hems
        components: [eebus_lpc]

    eebus_lpc:
      id: hems_lpc
      ship_port: 4712
      remote_ski: "aabbccddeeff..."   # SKI of the CLS-Steuerbox
      device_brand: "DIY"
      device_type: "HEMS"
      device_model: "ESP32-HEMS-14a"
      on_limit_active:
        - then:
            - lambda: |-
                float limit_w = x;
                // Distribute limit to WP (SG-Ready) and Wallbox (Modbus)
      on_limit_cleared:
        - then:
            - logger.log: "VNB limit cleared, returning to normal operation"
      current_limit:
        name: "VNB Power Limit"
        unit_of_measurement: W
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_TRIGGER_ID,
)

DEPENDENCIES = ["network", "esp32"]
AUTO_LOAD = []
CODEOWNERS = ["@bgewehr"]

MULTI_CONF = False

# Namespace
eebus_lpc_ns = cg.esphome_ns.namespace("eebus_lpc")
EebusLpcComponent = eebus_lpc_ns.class_("EebusLpcComponent", cg.Component)

# Triggers
LimitActiveTrigger = eebus_lpc_ns.class_(
    "LimitActiveTrigger", automation.Trigger.template(cg.float_)
)
LimitClearedTrigger = eebus_lpc_ns.class_(
    "LimitClearedTrigger", automation.Trigger.template()
)

# Config keys
CONF_REMOTE_SKI     = "remote_ski"
CONF_SHIP_PORT      = "ship_port"
CONF_DEVICE_BRAND   = "device_brand"
CONF_DEVICE_TYPE    = "device_type"
CONF_DEVICE_MODEL   = "device_model"
CONF_ON_LIMIT_ACTIVE  = "on_limit_active"
CONF_ON_LIMIT_CLEARED = "on_limit_cleared"
CONF_CURRENT_LIMIT  = "current_limit"
CONF_FAILSAFE_LIMIT = "failsafe_limit_w"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EebusLpcComponent),
        cv.Optional(CONF_SHIP_PORT, default=4712): cv.port,
        cv.Optional(CONF_REMOTE_SKI, default=""): cv.string,
        cv.Optional(CONF_DEVICE_BRAND, default="DIY"): cv.string_strict,
        cv.Optional(CONF_DEVICE_TYPE, default="HEMS"): cv.string_strict,
        cv.Optional(CONF_DEVICE_MODEL, default="ESP32-HEMS-14a"): cv.string_strict,
        cv.Optional(CONF_FAILSAFE_LIMIT, default=4200.0): cv.positive_float,
        cv.Optional(CONF_ON_LIMIT_ACTIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LimitActiveTrigger),
            }
        ),
        cv.Optional(CONF_ON_LIMIT_CLEARED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LimitClearedTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_ship_port(config[CONF_SHIP_PORT]))
    cg.add(var.set_remote_ski(config[CONF_REMOTE_SKI]))
    cg.add(var.set_device_brand(config[CONF_DEVICE_BRAND]))
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
    cg.add(var.set_device_model(config[CONF_DEVICE_MODEL]))
    cg.add(var.set_failsafe_limit_w(config[CONF_FAILSAFE_LIMIT]))

    for conf in config.get(CONF_ON_LIMIT_ACTIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.float_, "x")], conf)

    for conf in config.get(CONF_ON_LIMIT_CLEARED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
