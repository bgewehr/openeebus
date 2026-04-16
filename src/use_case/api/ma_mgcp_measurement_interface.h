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
 * @brief MA MGCP Measurement interface declarations
 */

#ifndef SRC_USE_CASE_API_MA_MGCP_MEASUREMENT_INTERFACE_H_
#define SRC_USE_CASE_API_MA_MGCP_MEASUREMENT_INTERFACE_H_

#include "src/common/eebus_errors.h"
#include "src/use_case/model/mgcp_types.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_client.h"
#include "src/use_case/specialization/measurement/measurement_client.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct MaMgcpMeasurementInterface MaMgcpMeasurementInterface;
typedef struct MaMgcpMeasurementObject MaMgcpMeasurementObject;

struct MaMgcpMeasurementInterface {
  GcpMeasurementNameId (*get_name)(const MaMgcpMeasurementObject* self);
  EebusError (*get_data_value)(
      const MaMgcpMeasurementObject* self,
      MeasurementClient* mcl,
      ElectricalConnectionClient* eccl,
      ScaledValue* measurement_value
  );
};

struct MaMgcpMeasurementObject {
  const MaMgcpMeasurementInterface* interface_;
};

#define MA_MGCP_MEASUREMENT_OBJECT(obj) ((MaMgcpMeasurementObject*)(obj))
#define MA_MGCP_MEASUREMENT_INTERFACE(obj) (MA_MGCP_MEASUREMENT_OBJECT(obj)->interface_)

#define MA_MGCP_MEASUREMENT_GET_NAME(obj) (MA_MGCP_MEASUREMENT_INTERFACE(obj)->get_name(obj))

#define MA_MGCP_MEASUREMENT_GET_DATA_VALUE(obj, mcl, eccl, measurement_value) \
  (MA_MGCP_MEASUREMENT_INTERFACE(obj)->get_data_value(obj, mcl, eccl, measurement_value))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_API_MA_MGCP_MEASUREMENT_INTERFACE_H_
