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
 * @brief Mu Mpc Measurement interface declarations
 */

#ifndef SRC_USE_CASE_API_MU_MPC_MEASUREMENT_INTERFACE_H_
#define SRC_USE_CASE_API_MU_MPC_MEASUREMENT_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/spine/model/common_data_types.h"
#include "src/spine/model/measurement_types.h"
#include "src/use_case/model/mpc_types.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/measurement/measurement_server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Mu Mpc Measurement Interface
 * (Mu Mpc Measurement "virtual functions table" declaration)
 */
typedef struct MuMpcMeasurementInterface MuMpcMeasurementInterface;

/**
 * @brief Mu Mpc Measurement Object type definition
 * ("abstract class", has no members but only pointer to
 * "virtual functions table")
 */
typedef struct MuMpcMeasurementObject MuMpcMeasurementObject;

/**
 * @brief MuMpcMeasurement Interface Structure
 */
struct MuMpcMeasurementInterface {
  void (*destruct)(MuMpcMeasurementObject* self);
  MuMpcMeasurementNameId (*get_name)(const MuMpcMeasurementObject* self);
  EebusError (*get_data_value)(
      const MuMpcMeasurementObject* self,
      MeasurementServer* msrv,
      ScaledValue* measurement_value
  );
  const MeasurementConstraintsDataType* (*get_constraints)(const MuMpcMeasurementObject* self);
  EebusError (*configure)(
      MuMpcMeasurementObject* self,
      MeasurementServer* msrv,
      ElectricalConnectionServer* ecsrv,
      ElectricalConnectionIdType electrical_connection_id
  );
  EebusError (*set_data_cache)(
      MuMpcMeasurementObject* self,
      const ScaledValue* measured_value,
      const EebusDateTime* timestamp,
      const MeasurementValueStateType* value_state,
      const EebusDateTime* start_time,
      const EebusDateTime* end_time
  );
  MeasurementDataType* (*release_data_cache)(MuMpcMeasurementObject* self);
};

/**
 * @brief Mu Mpc Measurement Object Structure
 */
struct MuMpcMeasurementObject {
  const MuMpcMeasurementInterface* interface_;
};

/**
 * @brief Mu Mpc Measurement pointer typecast
 */
#define MU_MPC_MEASUREMENT_OBJECT(obj) ((MuMpcMeasurementObject*)(obj))

/**
 * @brief Mu Mpc Measurement Interface class pointer typecast
 */
#define MU_MPC_MEASUREMENT_INTERFACE(obj) (MU_MPC_MEASUREMENT_OBJECT(obj)->interface_)

/**
 * @brief Mu Mpc Measurement Destruct caller definition
 */
#define MU_MPC_MEASUREMENT_DESTRUCT(obj) (MU_MPC_MEASUREMENT_INTERFACE(obj)->destruct(obj))

/**
 * @brief Mu Mpc Measurement Get Name caller definition
 */
#define MU_MPC_MEASUREMENT_GET_NAME(obj) (MU_MPC_MEASUREMENT_INTERFACE(obj)->get_name(obj))

/**
 * @brief Mu Mpc Measurement Get Data Value caller definition
 */
#define MU_MPC_MEASUREMENT_GET_DATA_VALUE(obj, msrv, measurement_value) \
  (MU_MPC_MEASUREMENT_INTERFACE(obj)->get_data_value(obj, msrv, measurement_value))

/**
 * @brief Mu Mpc Measurement Get Constraints caller definition
 */
#define MU_MPC_MEASUREMENT_GET_CONSTRAINTS(obj) (MU_MPC_MEASUREMENT_INTERFACE(obj)->get_constraints(obj))

/**
 * @brief Mu Mpc Measurement Configure caller definition
 */
#define MU_MPC_MEASUREMENT_CONFIGURE(obj, msrv, ecsrv, electrical_connection_id) \
  (MU_MPC_MEASUREMENT_INTERFACE(obj)->configure(obj, msrv, ecsrv, electrical_connection_id))

/**
 * @brief Mu Mpc Measurement Set Data Cache caller definition
 */
#define MU_MPC_MEASUREMENT_SET_DATA_CACHE(obj, measured_value, timestamp, value_state, start_time, end_time) \
  (MU_MPC_MEASUREMENT_INTERFACE(obj)->set_data_cache(obj, measured_value, timestamp, value_state, start_time, end_time))

/**
 * @brief Mu Mpc Measurement Release Data Cache caller definition
 */
#define MU_MPC_MEASUREMENT_RELEASE_DATA_CACHE(obj) (MU_MPC_MEASUREMENT_INTERFACE(obj)->release_data_cache(obj))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_MU_MPC_MEASUREMENT_INTERFACE_H_
