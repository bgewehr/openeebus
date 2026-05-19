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
 * @brief GCP MGCP Monitor interface declarations
 */

#ifndef SRC_USE_CASE_API_GCP_MGCP_MONITOR_INTERFACE_H_
#define SRC_USE_CASE_API_GCP_MGCP_MONITOR_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/use_case/api/eebus_measurement_interface.h"
#include "src/use_case/model/mgcp_types.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/measurement/measurement_server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct GcpMgcpMonitorInterface GcpMgcpMonitorInterface;
typedef struct GcpMgcpMonitorObject GcpMgcpMonitorObject;

struct GcpMgcpMonitorInterface {
  void (*destruct)(GcpMgcpMonitorObject* self);
  GcpMonitorNameId (*get_name)(const GcpMgcpMonitorObject* self);
  EebusError (*configure)(
      GcpMgcpMonitorObject* self,
      MeasurementServer* msrv,
      ElectricalConnectionServer* ecsrv,
      ElectricalConnectionIdType electrical_connection_id,
      MeasurementConstraintsListDataType* measurements_constraints
  );
  EebusMeasurementObject* (*get_measurement)(
      const GcpMgcpMonitorObject* self,
      GcpMeasurementNameId measurement_name_id
  );
  EebusError (*flush_measurement_cache)(GcpMgcpMonitorObject* self, MeasurementListDataType* measurement_data_list);
};

struct GcpMgcpMonitorObject {
  const GcpMgcpMonitorInterface* interface_;
};

#define GCP_MGCP_MONITOR_OBJECT(obj) ((GcpMgcpMonitorObject*)(obj))
#define GCP_MGCP_MONITOR_INTERFACE(obj) (GCP_MGCP_MONITOR_OBJECT(obj)->interface_)

#define GCP_MGCP_MONITOR_DESTRUCT(obj) (GCP_MGCP_MONITOR_INTERFACE(obj)->destruct(obj))

#define GCP_MGCP_MONITOR_GET_NAME(obj) (GCP_MGCP_MONITOR_INTERFACE(obj)->get_name(obj))

#define GCP_MGCP_MONITOR_CONFIGURE(obj, msrv, ecsrv, electrical_connection_id, measurements_constraints) \
  (GCP_MGCP_MONITOR_INTERFACE(obj)->configure(obj, msrv, ecsrv, electrical_connection_id, measurements_constraints))

#define GCP_MGCP_MONITOR_GET_MEASUREMENT(obj, measurement_name_id) \
  (GCP_MGCP_MONITOR_INTERFACE(obj)->get_measurement(obj, measurement_name_id))

#define GCP_MGCP_MONITOR_FLUSH_MEASUREMENT_CACHE(obj, measurement_data_list) \
  (GCP_MGCP_MONITOR_INTERFACE(obj)->flush_measurement_cache(obj, measurement_data_list))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_GCP_MGCP_MONITOR_INTERFACE_H_
