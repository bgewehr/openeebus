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
 * @brief Mu Mpc Monitor interface declarations
 */

#ifndef SRC_USE_CASE_API_MU_MPC_MONITOR_INTERFACE_H_
#define SRC_USE_CASE_API_MU_MPC_MONITOR_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/use_case/api/mu_mpc_measurement_interface.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/measurement/measurement_server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Mu Mpc Monitor Interface
 * (Mu Mpc Monitor "virtual functions table" declaration)
 */
typedef struct MuMpcMonitorInterface MuMpcMonitorInterface;

/**
 * @brief Mu Mpc Monitor Object type definition
 * ("abstract class", has no members but only pointer to
 * "virtual functions table")
 */
typedef struct MuMpcMonitorObject MuMpcMonitorObject;

/**
 * @brief MuMpcMonitor Interface Structure
 */
struct MuMpcMonitorInterface {
  void (*destruct)(MuMpcMonitorObject* self);
  MuMpcMonitorNameId (*get_name)(const MuMpcMonitorObject* self);
  EebusError (*configure)(
      MuMpcMonitorObject* self,
      MeasurementServer* msrv,
      ElectricalConnectionServer* ecsrv,
      ElectricalConnectionIdType electrical_connection_id,
      MeasurementConstraintsListDataType* measurements_constraints
  );
  MuMpcMeasurementObject* (*get_measurement)(
      const MuMpcMonitorObject* self,
      MuMpcMeasurementNameId measurement_name_id
  );
  EebusError (*flush_measurement_cache)(MuMpcMonitorObject* self, MeasurementListDataType* measurement_data_list);
};

/**
 * @brief Mu Mpc Monitor Object Structure
 */
struct MuMpcMonitorObject {
  const MuMpcMonitorInterface* interface_;
};

/**
 * @brief Mu Mpc Monitor pointer typecast
 */
#define MU_MPC_MONITOR_OBJECT(obj) ((MuMpcMonitorObject*)(obj))

/**
 * @brief Mu Mpc Monitor Interface class pointer typecast
 */
#define MU_MPC_MONITOR_INTERFACE(obj) (MU_MPC_MONITOR_OBJECT(obj)->interface_)

/**
 * @brief Mu Mpc Monitor Destruct caller definition
 */
#define MU_MPC_MONITOR_DESTRUCT(obj) (MU_MPC_MONITOR_INTERFACE(obj)->destruct(obj))

/**
 * @brief Mu Mpc Monitor Get Name caller definition
 */
#define MU_MPC_MONITOR_GET_NAME(obj) (MU_MPC_MONITOR_INTERFACE(obj)->get_name(obj))

/**
 * @brief Mu Mpc Monitor Configure caller definition
 */
#define MU_MPC_MONITOR_CONFIGURE(obj, msrv, ecsrv, electrical_connection_id, measurements_constraints) \
  (MU_MPC_MONITOR_INTERFACE(obj)->configure(obj, msrv, ecsrv, electrical_connection_id, measurements_constraints))

/**
 * @brief Mu Mpc Monitor Get Measurement caller definition
 */
#define MU_MPC_MONITOR_GET_MEASUREMENT(obj, measurement_name_id) \
  (MU_MPC_MONITOR_INTERFACE(obj)->get_measurement(obj, measurement_name_id))

/**
 * @brief Mu Mpc Monitor Flush Measurement Cache caller definition
 */
#define MU_MPC_MONITOR_FLUSH_MEASUREMENT_CACHE(obj, measurement_data_list) \
  (MU_MPC_MONITOR_INTERFACE(obj)->flush_measurement_cache(obj, measurement_data_list))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_MU_MPC_MONITOR_INTERFACE_H_
