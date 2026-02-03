/*
 * Copyright 2025 NIBE AB
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
 * @file
 * @brief CS LPC Listener implementation
 */

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include "examples/hems/eg_lpc_listener.h"
#include "src/common/eebus_arguments.h"
#include "src/common/eebus_date_time/eebus_date_time.h"
#include "src/common/eebus_malloc.h"
#include "src/use_case/api/eg_lpc_listener_interface.h"

typedef struct EgLpcListener EgLpcListener;

struct EgLpcListener {
  /** Implements the CS LPC Listener Interface */
  EgLpcListenerObject obj;

  /* Pointer to the HEMS instance */
  HemsObject* hems;
};

#define EG_LPC_LISTENER(obj) ((EgLpcListener*)(obj))

static void Destruct(EgLpcListenerObject* self);
static void OnRemoteEntityConnect(EgLpcListenerObject* self, const EntityAddressType* entity_addr);
static void OnRemoteEntityDisconnect(EgLpcListenerObject* self, const EntityAddressType* entity_addr);
static void OnPowerLimitReceive(
    EgLpcListenerObject* self,
    const ScaledValue* power_limit,
    const DurationType* duration,
    bool is_active
);
static void OnFailsafePowerLimitReceive(EgLpcListenerObject* self, const ScaledValue* power_limit);
static void OnFailsafeDurationReceive(EgLpcListenerObject* self, const DurationType* duration);
static void OnHeartbeatReceive(EgLpcListenerObject* self, uint64_t heartbeat_counter);

static const EgLpcListenerInterface cs_lpc_listener_methods = {
    .destruct                        = Destruct,
    .on_remote_entity_connect        = OnRemoteEntityConnect,
    .on_remote_entity_disconnect     = OnRemoteEntityDisconnect,
    .on_power_limit_receive          = OnPowerLimitReceive,
    .on_failsafe_power_limit_receive = OnFailsafePowerLimitReceive,
    .on_failsafe_duration_receive    = OnFailsafeDurationReceive,
    .on_heartbeat_receive            = OnHeartbeatReceive,
};

static void EgLpcListenerConstruct(EgLpcListener* self, HemsObject* hems);

void EgLpcListenerConstruct(EgLpcListener* self, HemsObject* hems) {
  // Override "virtual functions table"
  EG_LPC_LISTENER_INTERFACE(self) = &cs_lpc_listener_methods;

  self->hems = hems;
}

EgLpcListenerObject* EgLpcListenerCreate(HemsObject* hems) {
  EgLpcListener* const cs_lpc_listener = (EgLpcListener*)EEBUS_MALLOC(sizeof(EgLpcListener));

  EgLpcListenerConstruct(cs_lpc_listener, hems);

  return EG_LPC_LISTENER_OBJECT(cs_lpc_listener);
}

void Destruct(EgLpcListenerObject* self) {
  // Nothing to be deallocated yet
}

void OnRemoteEntityConnect(EgLpcListenerObject* self, const EntityAddressType* entity_addr) {
  EgLpcListener* const lpc_listener = EG_LPC_LISTENER(self);

  HemsSetEgLpcRemoteEntity(lpc_listener->hems, entity_addr);
}

void OnRemoteEntityDisconnect(EgLpcListenerObject* self, const EntityAddressType* entity_addr) {
  EgLpcListener* const lpc_listener = EG_LPC_LISTENER(self);

  // Currently only single remote entity is supported,
  // so just clear the remote entity address
  HemsSetEgLpcRemoteEntity(lpc_listener->hems, NULL);
}

double GetValue(const ScaledValue* value) {
  return (double)(value->value) * pow(10, (double)value->scale);
}

void OnPowerLimitReceive(
    EgLpcListenerObject* self,
    const ScaledValue* power_limit,
    const EebusDuration* duration,
    bool is_active
) {
  UNUSED(self);

  ScaledValuePrint("New Limit received %sW, ", power_limit);
  EebusDurationPrint("duration = %s, ", duration);
  printf("active = %s\n", is_active ? "true" : "false");
}

void OnFailsafePowerLimitReceive(EgLpcListenerObject* self, const ScaledValue* power_limit) {
  UNUSED(self);

  ScaledValuePrint("New Failsafe Consumption Active Power Limit received:  %sW\n", power_limit);
}

void OnFailsafeDurationReceive(EgLpcListenerObject* self, const DurationType* duration) {
  UNUSED(self);

  EebusDurationPrint("New Failsafe Duration Minimum received: %s\n", duration);
}

void OnHeartbeatReceive(EgLpcListenerObject* self, uint64_t heartbeat_counter) {
  UNUSED(self);

  printf("Heartbeat received, counter = %" PRIu64 "\n", heartbeat_counter);
}
