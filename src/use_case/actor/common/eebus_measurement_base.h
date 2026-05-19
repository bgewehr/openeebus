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
 * @brief Shared measurement base for MU MPC and GCP MGCP server-side use cases
 */

#ifndef SRC_USE_CASE_ACTOR_COMMON_EEBUS_MEASUREMENT_BASE_H_
#define SRC_USE_CASE_ACTOR_COMMON_EEBUS_MEASUREMENT_BASE_H_

#include "src/use_case/api/eebus_measurement_interface.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct EebusMeasurementBase EebusMeasurementBase;

typedef EebusError (*EebusMeasurementConfigureStrategy)(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);

typedef struct EebusMeasurementBaseConfig {
  MeasurementValueSourceType value_source;
  MeasurementConstraintsDataType* constraints; /**< May be NULL */
} EebusMeasurementBaseConfig;

/**
 * @brief Allocate and initialise a measurement object.
 * @return Pointer to the created object, or NULL on failure.
 */
EebusMeasurementObject* EebusMeasurementBaseCreate(
    EebusMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const EebusMeasurementBaseConfig* cfg,
    EebusMeasurementConfigureStrategy strategy
);

EebusError EebusMeasurementBaseConfigurePower(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);
EebusError EebusMeasurementBaseConfigureEnergy(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);
EebusError EebusMeasurementBaseConfigureCurrent(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);
EebusError EebusMeasurementBaseConfigureVoltage(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);
EebusError EebusMeasurementBaseConfigureFrequency(
    EebusMeasurementBase* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_ACTOR_COMMON_EEBUS_MEASUREMENT_BASE_H_
