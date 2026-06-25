/*
 * Copyright 2025 bgewehr (bg-hems branch)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @file eebus_lpc.h
 * @brief ESPHome component wrapping the openeebus LPC CS use case.
 *
 * Exposes the EEBus LPC power limit received from a Westnetz CLS-Steuerbox
 * to ESPHome automations. The component:
 *
 *  1. Generates a self-signed X.509 certificate on first boot (stored in NVS).
 *  2. Starts the openeebus SHIP service as a CS (Controllable System).
 *  3. Fires on_limit_active / on_limit_cleared triggers.
 *  4. Publishes the current limit as a float sensor (watts).
 *
 * §14a EnWG compliance: the failsafe_limit_w (default 4200 W) is applied
 * automatically if the EEBus heartbeat is lost.
 */

#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

// openeebus C API
extern "C" {
#include "src/service/api/eebus_service_interface.h"
#include "src/service/api/eebus_service_config.h"
#include "src/use_case/api/cs_lp_listener_interface.h"
#include "src/use_case/actor/cs/lpc/cs_lpc.h"
#include "src/use_case/model/load_limit_types.h"
#include "src/use_case/model/scaled_value.h"
}

namespace esphome {
namespace eebus_lpc {

static const char* const TAG = "eebus_lpc";

class EebusLpcComponent;

/* -------------------------------------------------------------------------
 * Triggers
 * ---------------------------------------------------------------------- */

class LimitActiveTrigger : public Trigger<float> {
 public:
  explicit LimitActiveTrigger(EebusLpcComponent* parent);
};

class LimitClearedTrigger : public Trigger<> {
 public:
  explicit LimitClearedTrigger(EebusLpcComponent* parent);
};

/* -------------------------------------------------------------------------
 * Main component
 * ---------------------------------------------------------------------- */

class EebusLpcComponent : public Component {
 public:
  /* ESPHome lifecycle */
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

  /* Configuration setters (called from generated code) */
  void set_ship_port(uint16_t port)           { this->ship_port_       = port; }
  void set_remote_ski(const std::string& ski) { this->remote_ski_      = ski; }
  void set_device_brand(const std::string& b) { this->device_brand_    = b; }
  void set_device_type(const std::string& t)  { this->device_type_     = t; }
  void set_device_model(const std::string& m) { this->device_model_    = m; }
  void set_failsafe_limit_w(float w)          { this->failsafe_limit_w_ = w; }

  /* Trigger registration */
  void add_on_limit_active_trigger(LimitActiveTrigger* t)  { limit_active_triggers_.push_back(t); }
  void add_on_limit_cleared_trigger(LimitClearedTrigger* t) { limit_cleared_triggers_.push_back(t); }

  /* Called from the C listener callback */
  void on_power_limit_receive(float limit_w, bool is_active);
  void on_failsafe_limit_receive(float limit_w);
  void on_heartbeat_receive(uint64_t counter);

  /* Current state accessors */
  bool  is_limit_active() const  { return limit_active_; }
  float current_limit_w() const  { return current_limit_w_; }
  float failsafe_limit_w() const { return failsafe_limit_w_; }

 protected:
  /* Certificate management */
  bool load_or_generate_cert_();
  bool store_cert_nvs_(const uint8_t* cert, size_t cert_len,
                       const uint8_t* key,  size_t key_len);
  bool load_cert_nvs_(uint8_t** cert, size_t* cert_len,
                      uint8_t** key,  size_t* key_len);

  /* openeebus service lifecycle */
  bool start_eebus_service_(const uint8_t* cert_der, size_t cert_len,
                             const uint8_t* key_der,  size_t key_len);

  /* State */
  uint16_t    ship_port_       {4712};
  std::string remote_ski_      {};
  std::string device_brand_    {"DIY"};
  std::string device_type_     {"HEMS"};
  std::string device_model_    {"ESP32-HEMS-14a"};
  float       failsafe_limit_w_{4200.0f};

  bool        limit_active_    {false};
  float       current_limit_w_ {0.0f};
  bool        heartbeat_lost_  {false};
  uint32_t    last_heartbeat_ms_{0};

  /* openeebus handles */
  EebusServiceObject* service_{nullptr};
  CsLpcObject*        cs_lpc_ {nullptr};

  /* Trigger lists */
  std::vector<LimitActiveTrigger*>  limit_active_triggers_;
  std::vector<LimitClearedTrigger*> limit_cleared_triggers_;

  /* C-compatible listener object — wraps back to this instance */
  struct LpcListener {
    CsLpListenerObject obj;   /* must be first */
    EebusLpcComponent* self;
  } listener_{};
};

/* -------------------------------------------------------------------------
 * C-compatible listener vtable callbacks (bridge to C++ component)
 * ---------------------------------------------------------------------- */

extern "C" {

static void LpcListenerDestruct(CsLpListenerObject* /*self*/) {}

static void LpcListenerOnPowerLimitReceive(
    CsLpListenerObject*  self,
    const ScaledValue*   power_limit,
    const DurationType*  /*duration*/,
    bool                 is_active)
{
  auto* listener = reinterpret_cast<EebusLpcComponent::LpcListener*>(self);
  float watts = (float)(power_limit->value) *
                powf(10.0f, (float)(power_limit->scale));
  listener->self->on_power_limit_receive(watts, is_active);
}

static void LpcListenerOnFailsafePowerLimitReceive(
    CsLpListenerObject* self,
    const ScaledValue*  power_limit)
{
  auto* listener = reinterpret_cast<EebusLpcComponent::LpcListener*>(self);
  float watts = (float)(power_limit->value) *
                powf(10.0f, (float)(power_limit->scale));
  listener->self->on_failsafe_limit_receive(watts);
}

static void LpcListenerOnFailsafeDurationReceive(
    CsLpListenerObject* /*self*/,
    const DurationType* /*duration*/) {}

static void LpcListenerOnHeartbeatReceive(
    CsLpListenerObject* self,
    uint64_t            heartbeat_counter)
{
  auto* listener = reinterpret_cast<EebusLpcComponent::LpcListener*>(self);
  listener->self->on_heartbeat_receive(heartbeat_counter);
}

static const CsLpListenerInterface kLpcListenerMethods = {
  .destruct                      = LpcListenerDestruct,
  .on_power_limit_receive        = LpcListenerOnPowerLimitReceive,
  .on_failsafe_power_limit_receive = LpcListenerOnFailsafePowerLimitReceive,
  .on_failsafe_duration_receive  = LpcListenerOnFailsafeDurationReceive,
  .on_heartbeat_receive          = LpcListenerOnHeartbeatReceive,
};

}  // extern "C"

/* -------------------------------------------------------------------------
 * Trigger constructors
 * ---------------------------------------------------------------------- */

inline LimitActiveTrigger::LimitActiveTrigger(EebusLpcComponent* parent) {
  parent->add_on_limit_active_trigger(this);
}

inline LimitClearedTrigger::LimitClearedTrigger(EebusLpcComponent* parent) {
  parent->add_on_limit_cleared_trigger(this);
}

}  // namespace eebus_lpc
}  // namespace esphome
