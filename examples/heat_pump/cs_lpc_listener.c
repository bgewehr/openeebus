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

#include "examples/heat_pump/cs_lpc_listener.h"
#include "src/common/eebus_arguments.h"
#include "src/common/eebus_date_time/eebus_date_time.h"
#include "src/common/eebus_malloc.h"
#include "src/use_case/api/cs_lpc_listener_interface.h"
#include "src/use_case/model/scaled_value.h"

typedef struct CsLpcListener CsLpcListener;

struct CsLpcListener {
  /** Implements the CS LPC Listener Interface */
  CsLpcListenerObject obj;
};

#define CS_LPC_LISTENER(obj) ((CsLpcListener*)(obj))

static void Destruct(CsLpcListenerObject* self);
static void OnPowerLimitReceive(
    CsLpcListenerObject* self,
    const ScaledValue* power_limit,
    const DurationType* duration,
    bool is_active
);
static void OnFailsafePowerLimitReceive(CsLpcListenerObject* self, const ScaledValue* power_limit);
static void OnFailsafeDurationReceive(CsLpcListenerObject* self, const DurationType* duration);
static void OnHeartbeatReceive(CsLpcListenerObject* self, uint64_t heartbeat_counter);

static const CsLpcListenerInterface cs_lpc_listener_methods = {
    .destruct                        = Destruct,
    .on_power_limit_receive          = OnPowerLimitReceive,
    .on_failsafe_power_limit_receive = OnFailsafePowerLimitReceive,
    .on_failsafe_duration_receive    = OnFailsafeDurationReceive,
    .on_heartbeat_receive            = OnHeartbeatReceive,
};

static EebusError CsLpcListenerConstruct(CsLpcListener* self);

EebusError CsLpcListenerConstruct(CsLpcListener* self) {
  // Override "virtual functions table"
  CS_LPC_LISTENER_INTERFACE(self) = &cs_lpc_listener_methods;

  return kEebusErrorOk;
}

CsLpcListenerObject* CsLpcListenerCreate(void) {
  CsLpcListener* const cs_lpc_listener = (CsLpcListener*)EEBUS_MALLOC(sizeof(CsLpcListener));
  if (cs_lpc_listener == NULL) {
    return NULL;
  }

  if (CsLpcListenerConstruct(cs_lpc_listener) != kEebusErrorOk) {
    CsLpcListenerDelete(CS_LPC_LISTENER_OBJECT(cs_lpc_listener));
    return NULL;
  }

  return CS_LPC_LISTENER_OBJECT(cs_lpc_listener);
}

void Destruct(CsLpcListenerObject* self) {
  UNUSED(self);

  // Nothing to be deallocated yet
}

void OnPowerLimitReceive(
    CsLpcListenerObject* self,
    const ScaledValue* power_limit,
    const EebusDuration* duration,
    bool is_active
) {
  UNUSED(self);

  ScaledValuePrint("New Limit received %sW, ", power_limit);
  EebusDurationPrint("duration = %s, ", duration);
  printf("active = %s\n", is_active ? "true" : "false");
}

void OnFailsafePowerLimitReceive(CsLpcListenerObject* self, const ScaledValue* power_limit) {
  UNUSED(self);

  ScaledValuePrint("New Failsafe Consumption Active Power Limit received:  %sW\n", power_limit);
}

void OnFailsafeDurationReceive(CsLpcListenerObject* self, const DurationType* duration) {
  UNUSED(self);

  EebusDurationPrint("New Failsafe Duration Minimum received: %s\n", duration);
}

void OnHeartbeatReceive(CsLpcListenerObject* self, uint64_t heartbeat_counter) {
  UNUSED(self);

  printf("Heartbeat received, counter = %" PRIu64 "\n", heartbeat_counter);
}
