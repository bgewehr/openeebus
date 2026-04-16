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
 * @brief GCP MGCP Measurement interface declarations
 */

#ifndef SRC_USE_CASE_API_GCP_MGCP_MEASUREMENT_INTERFACE_H_
#define SRC_USE_CASE_API_GCP_MGCP_MEASUREMENT_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/spine/model/common_data_types.h"
#include "src/spine/model/measurement_types.h"
#include "src/use_case/model/mgcp_types.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/measurement/measurement_server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct GcpMgcpMeasurementInterface GcpMgcpMeasurementInterface;
typedef struct GcpMgcpMeasurementObject GcpMgcpMeasurementObject;

struct GcpMgcpMeasurementInterface {
  void (*destruct)(GcpMgcpMeasurementObject* self);
  GcpMeasurementNameId (*get_name)(const GcpMgcpMeasurementObject* self);
  EebusError (*get_data_value)(
      const GcpMgcpMeasurementObject* self,
      MeasurementServer* msrv,
      ScaledValue* measurement_value
  );
  const MeasurementConstraintsDataType* (*get_constraints)(const GcpMgcpMeasurementObject* self);
  EebusError (*configure)(
      GcpMgcpMeasurementObject* self,
      MeasurementServer* msrv,
      ElectricalConnectionServer* ecsrv,
      ElectricalConnectionIdType electrical_connection_id
  );
  EebusError (*set_data_cache)(
      GcpMgcpMeasurementObject* self,
      const ScaledValue* measured_value,
      const EebusDateTime* timestamp,
      const MeasurementValueStateType* value_state,
      const EebusDateTime* start_time,
      const EebusDateTime* end_time
  );
  MeasurementDataType* (*release_data_cache)(GcpMgcpMeasurementObject* self);
};

struct GcpMgcpMeasurementObject {
  const GcpMgcpMeasurementInterface* interface_;
};

#define GCP_MGCP_MEASUREMENT_OBJECT(obj) ((GcpMgcpMeasurementObject*)(obj))
#define GCP_MGCP_MEASUREMENT_INTERFACE(obj) (GCP_MGCP_MEASUREMENT_OBJECT(obj)->interface_)

#define GCP_MGCP_MEASUREMENT_DESTRUCT(obj) (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->destruct(obj))

#define GCP_MGCP_MEASUREMENT_GET_NAME(obj) (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->get_name(obj))

#define GCP_MGCP_MEASUREMENT_GET_DATA_VALUE(obj, msrv, measurement_value) \
  (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->get_data_value(obj, msrv, measurement_value))

#define GCP_MGCP_MEASUREMENT_GET_CONSTRAINTS(obj) (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->get_constraints(obj))

#define GCP_MGCP_MEASUREMENT_CONFIGURE(obj, msrv, ecsrv, electrical_connection_id) \
  (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->configure(obj, msrv, ecsrv, electrical_connection_id))

#define GCP_MGCP_MEASUREMENT_SET_DATA_CACHE(obj, measured_value, timestamp, value_state, start_time, end_time) \
  (GCP_MGCP_MEASUREMENT_INTERFACE(obj)                                                                         \
       ->set_data_cache(obj, measured_value, timestamp, value_state, start_time, end_time))

#define GCP_MGCP_MEASUREMENT_RELEASE_DATA_CACHE(obj) (GCP_MGCP_MEASUREMENT_INTERFACE(obj)->release_data_cache(obj))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_GCP_MGCP_MEASUREMENT_INTERFACE_H_
