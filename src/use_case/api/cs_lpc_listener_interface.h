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
 * @brief Cs Lpc Listener interface declarations
 */

#ifndef SRC_USE_CASE_API_CS_LPC_LISTENER_INTERFACE_H_
#define SRC_USE_CASE_API_CS_LPC_LISTENER_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>

#include "src/common/eebus_date_time/eebus_duration.h"
#include "src/use_case/model/load_limit_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Cs Lpc Listener Interface
 * (Cs Lpc Listener "virtual functions table" declaration)
 */
typedef struct CsLpcListenerInterface CsLpcListenerInterface;

/**
 * @brief Cs Lpc Listener Object type definition
 * ("abstract class", has no members but only pointer to
 * "virtual functions table")
 */
typedef struct CsLpcListenerObject CsLpcListenerObject;

/**
 * @brief CsLpcListener Interface Structure
 */
struct CsLpcListenerInterface {
  void (*destruct)(CsLpcListenerObject* self);
  void (*on_power_limit_receive)(
      CsLpcListenerObject* self,
      const ScaledValue* power_limit,
      const DurationType* duration,
      bool is_active
  );
  void (*on_failsafe_power_limit_receive)(CsLpcListenerObject* self, const ScaledValue* power_limit);
  void (*on_failsafe_duration_receive)(CsLpcListenerObject* self, const DurationType* duration);
  void (*on_heartbeat_receive)(CsLpcListenerObject* self, uint64_t heartbeat_counter);
};

/**
 * @brief Cs Lpc Listener Object Structure
 */
struct CsLpcListenerObject {
  const CsLpcListenerInterface* interface_;
};

/**
 * @brief Cs Lpc Listener pointer typecast
 */
#define CS_LPC_LISTENER_OBJECT(obj) ((CsLpcListenerObject*)(obj))

/**
 * @brief Cs Lpc Listener Interface class pointer typecast
 */
#define CS_LPC_LISTENER_INTERFACE(obj) (CS_LPC_LISTENER_OBJECT(obj)->interface_)

/**
 * @brief Cs Lpc Listener Destruct caller definition
 */
#define CS_LPC_LISTENER_DESTRUCT(obj) (CS_LPC_LISTENER_INTERFACE(obj)->destruct(obj))

/**
 * @brief Cs Lpc Listener On Power Limit Receive caller definition
 */
#define CS_LPC_LISTENER_ON_POWER_LIMIT_RECEIVE(obj, power_limit, duration, is_active) \
  (CS_LPC_LISTENER_INTERFACE(obj)->on_power_limit_receive(obj, power_limit, duration, is_active))

/**
 * @brief Cs Lpc Listener On Failsafe Power Limit Receive caller definition
 */
#define CS_LPC_LISTENER_ON_FAILSAFE_POWER_LIMIT_RECEIVE(obj, power_limit) \
  (CS_LPC_LISTENER_INTERFACE(obj)->on_failsafe_power_limit_receive(obj, power_limit))

/**
 * @brief Cs Lpc Listener On Failsafe Duration Receive caller definition
 */
#define CS_LPC_LISTENER_ON_FAILSAFE_DURATION_RECEIVE(obj, duration) \
  (CS_LPC_LISTENER_INTERFACE(obj)->on_failsafe_duration_receive(obj, duration))

/**
 * @brief Cs Lpc Listener On Heartbeat Receive caller definition
 */
#define CS_LPC_LISTENER_ON_HEARTBEAT_RECEIVE(obj, heartbeat_counter) \
  (CS_LPC_LISTENER_INTERFACE(obj)->on_heartbeat_receive(obj, heartbeat_counter))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_CS_LPC_LISTENER_INTERFACE_H_
