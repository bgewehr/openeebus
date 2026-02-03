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
 * @brief Eg Lpc Listener interface declarations
 */

#ifndef SRC_USE_CASE_API_EG_LPC_LISTENER_INTERFACE_H_
#define SRC_USE_CASE_API_EG_LPC_LISTENER_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>

#include "src/common/eebus_date_time/eebus_duration.h"
#include "src/use_case/model/load_limit_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Eg Lpc Listener Interface
 * (Eg Lpc Listener "virtual functions table" declaration)
 */
typedef struct EgLpcListenerInterface EgLpcListenerInterface;

/**
 * @brief Eg Lpc Listener Object type definition
 * ("abstract class", has no members but only pointer to
 * "virtual functions table")
 */
typedef struct EgLpcListenerObject EgLpcListenerObject;

/**
 * @brief EgLpcListener Interface Structure
 */
struct EgLpcListenerInterface {
  void (*destruct)(EgLpcListenerObject* self);
  void (*on_remote_entity_connect)(EgLpcListenerObject* self, const EntityAddressType* entity_addr);
  void (*on_remote_entity_disconnect)(EgLpcListenerObject* self, const EntityAddressType* entity_addr);
  void (*on_power_limit_receive)(
      EgLpcListenerObject* self,
      const ScaledValue* power_limit,
      const DurationType* duration,
      bool is_active
  );
  void (*on_failsafe_power_limit_receive)(EgLpcListenerObject* self, const ScaledValue* power_limit);
  void (*on_failsafe_duration_receive)(EgLpcListenerObject* self, const DurationType* duration);
  void (*on_heartbeat_receive)(EgLpcListenerObject* self, uint64_t heartbeat_counter);
};

/**
 * @brief Eg Lpc Listener Object Structure
 */
struct EgLpcListenerObject {
  const EgLpcListenerInterface* interface_;
};

/**
 * @brief Eg Lpc Listener pointer typecast
 */
#define EG_LPC_LISTENER_OBJECT(obj) ((EgLpcListenerObject*)(obj))

/**
 * @brief Eg Lpc Listener Interface class pointer typecast
 */
#define EG_LPC_LISTENER_INTERFACE(obj) (EG_LPC_LISTENER_OBJECT(obj)->interface_)

/**
 * @brief Eg Lpc Listener Destruct caller definition
 */
#define EG_LPC_LISTENER_DESTRUCT(obj) (EG_LPC_LISTENER_INTERFACE(obj)->destruct(obj))

/**
 * @brief Eg Lpc Listener On Remote Entity Connect caller definition
 */
#define EG_LPC_LISTENER_ON_REMOTE_ENTITY_CONNECT(obj, entity_addr) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_remote_entity_connect(obj, entity_addr))

/**
 * @brief Eg Lpc Listener On Remote Entity Disconnect caller definition
 */
#define EG_LPC_LISTENER_ON_REMOTE_ENTITY_DISCONNECT(obj, entity_addr) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_remote_entity_disconnect(obj, entity_addr))

/**
 * @brief Eg Lpc Listener On Power Limit Receive caller definition
 */
#define EG_LPC_LISTENER_ON_POWER_LIMIT_RECEIVE(obj, power_limit, duration, is_active) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_power_limit_receive(obj, power_limit, duration, is_active))

/**
 * @brief Eg Lpc Listener On Failsafe Power Limit Receive caller definition
 */
#define EG_LPC_LISTENER_ON_FAILSAFE_POWER_LIMIT_RECEIVE(obj, power_limit) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_failsafe_power_limit_receive(obj, power_limit))

/**
 * @brief Eg Lpc Listener On Failsafe Duration Receive caller definition
 */
#define EG_LPC_LISTENER_ON_FAILSAFE_DURATION_RECEIVE(obj, duration) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_failsafe_duration_receive(obj, duration))

/**
 * @brief Eg Lpc Listener On Heartbeat Receive caller definition
 */
#define EG_LPC_LISTENER_ON_HEARTBEAT_RECEIVE(obj, heartbeat_counter) \
  (EG_LPC_LISTENER_INTERFACE(obj)->on_heartbeat_receive(obj, heartbeat_counter))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_EG_LPC_LISTENER_INTERFACE_H_
