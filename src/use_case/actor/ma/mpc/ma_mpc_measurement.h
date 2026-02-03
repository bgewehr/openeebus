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
 * @brief Ma Mpc Measurement implementation declarations
 */

#ifndef SRC_USE_CASE_ACTOR_MA_MPC_MA_MPC_MEASUREMENT_H_
#define SRC_USE_CASE_ACTOR_MA_MPC_MA_MPC_MEASUREMENT_H_

#include "src/use_case/api/ma_mpc_measurement_interface.h"
#include "src/use_case/model/mpc_types.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_client.h"
#include "src/use_case/specialization/measurement/measurement_client.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Search for a MA MPC Measurement instance matching the given measurement data
 * @param mcl Measurement Client instance
 * @param eccl Electrical Connection Client instance
 * @param measurement_data Measurement Data to search for matching the key fields
 * @return MA MPC Measurement instance or NULL if not found
 */
const MaMpcMeasurementObject* MaMpcMeasurementGetInstance(
    const MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    const MeasurementDataType* measurement_data
);

/**
 * @brief Search for a MA MPC Measurement instance matching the given name id
 * @param name Measurement Name Id to search for
 * @return MA MPC Measurement instance or NULL if not found
 */
const MaMpcMeasurementObject* MaMpcMeasurementGetInstanceWithNameId(MuMpcMeasurementNameId name);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_ACTOR_MA_MPC_MA_MPC_MEASUREMENT_H_
