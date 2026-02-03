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
 * @brief Ma Mpc Measurement interface declarations
 */

#ifndef SRC_USE_CASE_API_MA_MPC_MEASUREMENT_INTERFACE_H_
#define SRC_USE_CASE_API_MA_MPC_MEASUREMENT_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/use_case/model/mpc_types.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_client.h"
#include "src/use_case/specialization/measurement/measurement_client.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Ma Mpc Measurement Interface
 * (Ma Mpc Measurement "virtual functions table" declaration)
 */
typedef struct MaMpcMeasurementInterface MaMpcMeasurementInterface;

/**
 * @brief Ma Mpc Measurement Object type definition
 * ("abstract class", has no members but only pointer to
 * "virtual functions table")
 */
typedef struct MaMpcMeasurementObject MaMpcMeasurementObject;

/**
 * @brief MaMpcMeasurement Interface Structure
 */
struct MaMpcMeasurementInterface {
  MuMpcMeasurementNameId (*get_name)(const MaMpcMeasurementObject* self);
  EebusError (*get_data_value)(
      const MaMpcMeasurementObject* self,
      MeasurementClient* mcl,
      ElectricalConnectionClient* eccl,
      ScaledValue* measurement_value
  );
};

/**
 * @brief Ma Mpc Measurement Object Structure
 */
struct MaMpcMeasurementObject {
  const MaMpcMeasurementInterface* interface_;
};

/**
 * @brief Ma Mpc Measurement pointer typecast
 */
#define MA_MPC_MEASUREMENT_OBJECT(obj) ((MaMpcMeasurementObject*)(obj))

/**
 * @brief Ma Mpc Measurement Interface class pointer typecast
 */
#define MA_MPC_MEASUREMENT_INTERFACE(obj) (MA_MPC_MEASUREMENT_OBJECT(obj)->interface_)

/**
 * @brief Ma Mpc Measurement Get Name caller definition
 */
#define MA_MPC_MEASUREMENT_GET_NAME(obj) (MA_MPC_MEASUREMENT_INTERFACE(obj)->get_name(obj))

/**
 * @brief Ma Mpc Measurement Get Data Value caller definition
 */
#define MA_MPC_MEASUREMENT_GET_DATA_VALUE(obj, mcl, eccl, measurement_value) \
  (MA_MPC_MEASUREMENT_INTERFACE(obj)->get_data_value(obj, mcl, eccl, measurement_value))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_MA_MPC_MEASUREMENT_INTERFACE_H_
