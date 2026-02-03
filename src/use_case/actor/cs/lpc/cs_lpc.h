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
 * @brief Controllable System LPC use case
 */

#ifndef SRC_EEBUS_SRC_USE_CASE_CONTROLLABLE_SYSTEM_LPC_CS_LPC_H_
#define SRC_EEBUS_SRC_USE_CASE_CONTROLLABLE_SYSTEM_LPC_CS_LPC_H_

#include "src/spine/entity/entity_local.h"
#include "src/spine/model/electrical_connection_types.h"
#include "src/use_case/api/cs_lpc_listener_interface.h"
#include "src/use_case/model/load_limit_types.h"
#include "src/use_case/use_case.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct CsLpcUseCaseObject CsLpcUseCaseObject;
struct CsLpcUseCaseObject {
  /** Inherits the Use Case */
  UseCaseObject obj;
};

#define CS_LPC_USE_CASE_OBJECT(obj) ((CsLpcUseCaseObject*)(obj))

CsLpcUseCaseObject* CsLpcUseCaseCreate(
    EntityLocalObject* local_entity,
    ElectricalConnectionIdType ec_id,
    CsLpcListenerObject* cs_lpc_listener
);

static inline void CsLpcUseCaseDelete(CsLpcUseCaseObject* cs_lpc_use_case) {
  if (cs_lpc_use_case != NULL) {
    USE_CASE_DESTRUCT(USE_CASE_OBJECT(cs_lpc_use_case));
    EEBUS_FREE(cs_lpc_use_case);
  }
}

/**
 * @brief Get the current load control limit data
 * @param self LPC CS Use Case instance to get the load control limit with
 * @param limit Buffer to load control limit data into
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError GetConsumptionLimit(const CsLpcUseCaseObject* self, LoadLimit* limit);

/**
 * @brief Set the current load control limit data
 */
EebusError
SetConsumptionLimit(CsLpcUseCaseObject* self, int64_t limit, int8_t scale, bool is_active, bool is_changeable);

/**
 * @brief Get the Failsafe Limit for the consumed active (real) power of the
 * Controllable System. This limit becomes activated in "init" state or "failsafe state".
 * @param self LPC CS Use Case instance to get the Failsafe Limit with
 * @param power_limit Output buffer to store the Failsafe Power Limit value
 * @param is_changeable Output buffer to store the changeable status
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError
GetFailsafeConsumptionActivePowerLimit(const CsLpcUseCaseObject* self, ScaledValue* power_limit, bool* is_changeable);

/**
 * @brief Set the Failsafe Limit for the consumed active (real) power of the
 * Controllable System. This limit becomes activated in "init" state or "failsafe state".
 * @param self LPC CS Use Case instance to set the Failsafe Limit with
 * @param power_limit Failsafe Power Limit value to be set
 * @param is_changeable Flag indicating if the client service can change this value
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError
SetFailsafeConsumptionActivePowerLimit(CsLpcUseCaseObject* self, const ScaledValue* power_limit, bool is_changeable);

/**
 * @brief Get the minimum time the Controllable System remains in "failsafe state" unless conditions
 * specified in this Use Case permit leaving the "failsafe state"
 * @param self LPC CS Use Case instance to get the Failsafe Duration Minimum with
 * @param duration Output buffer to store the Failsafe Duration Minimum
 * @param is_changeable Output buffer to store the changeable status
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError GetFailsafeDurationMinimum(const CsLpcUseCaseObject* self, DurationType* duration, bool* is_changeable);

/**
 * @brief Set minimum time the Controllable System remains in "failsafe state" unless conditions
 * specified in this Use Case permit leaving the "failsafe state"
 * @param self LPC CS Use Case instance to set the Failsafe Duration Minimum with
 * @param duration Duration to beset, has to be >= 2h and <= 24h
 * @param is_changeable Flag indicating if the client service can change this value
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError SetFailsafeDurationMinimum(CsLpcUseCaseObject* self, const DurationType* duration, bool is_changeable);

/**
 * @brief Start sending heartbeat from the local entity supporting this usecase.
 * The heartbeat is started by default when a non 0 timeout is set in the service configuration
 * @param self LPC CS Use Case instance to start the heartbeat with
 */
void StartHeartbeat(CsLpcUseCaseObject* self);

/**
 * @brief Stop sending heartbeat from the local CEM entity
 * @param self LPC CS Use Case instance to stop the heartbeat with
 */
void StopHeartbeat(CsLpcUseCaseObject* self);

/**
 * @brief Check if the currently available heartbeat data is within a time duration
 * @param self LPC CS Use Case instance to check the heartbeat data with
 * @return true if check is passed, false otherwise
 */
bool IsHeartbeatWithinDuration(CsLpcUseCaseObject* self);

/**
 * @brief Get the nominal maximum active (real) power the Controllable System is allowed to consume.
 *
 * If the local device type is an EnergyManagementSystem, the contractual consumption
 * nominal max is returned, otherwise the power consumption nominal max is returned.
 *
 * @param self LPC CS Use Case instance to get the nominal max with
 * @param nominal_max Pointer to the ScaledValue structure to store
 *                    the nominal max power consumption in W.
 * @return EebusError indicating the success or failure of the operation.
 */
EebusError GetConsumptionNominalMax(CsLpcUseCaseObject* self, ScaledValue* nominal_max);

/**
 * @brief Set the nominal maximum active (real) power the Controllable System is allowed to consume.
 *
 * If the local device type is an EnergyManagementSystem, the contractual consumption
 * nominal max is set, otherwise the power consumption nominal max is set.
 *
 * @param self Pointer to the CsLpcUseCaseObject instance.
 * @param new_nominal_max Pointer to the ScaledValue structure containing
 *                        the new nominal max power consumption in W.
 * @return EebusError indicating the success or failure of the operation.
 */
EebusError SetConsumptionNominalMax(CsLpcUseCaseObject* self, const ScaledValue* new_nominal_max);

/**
 * @brief Get the characteristic type depending on the local entities device devicetype
 * @param self LPC CS Use Case instance to check the heartbeat data with
 * @return Electrical connection characteristic type
 */
ElectricalConnectionCharacteristicTypeType GetElectricalConnectionCharacteristicType(const CsLpcUseCaseObject* self);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_EEBUS_SRC_USE_CASE_CONTROLLABLE_SYSTEM_LPC_CS_LPC_H_
