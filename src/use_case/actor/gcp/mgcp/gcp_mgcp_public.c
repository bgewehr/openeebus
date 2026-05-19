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
 * @brief GCP MGCP public API implementation
 */

#include "src/use_case/actor/gcp/mgcp/gcp_mgcp.h"

#include "src/use_case/actor/gcp/mgcp/gcp_mgcp_internal.h"
#include "src/use_case/specialization/device_configuration/device_configuration_server.h"
#include "src/use_case/specialization/measurement/measurement_server.h"
#include "src/use_case/use_case.h"

static GcpMgcpMonitorObject* GetMonitor(const GcpMgcpUseCase* self, GcpMeasurementNameId measurement_name) {
  const GcpMonitorNameId monitor_name = (GcpMonitorNameId)((uint8_t)measurement_name & (uint8_t)kGcpMonitorNameIdMask);

  for (size_t i = 0; i < VectorGetSize(&self->monitors); ++i) {
    GcpMgcpMonitorObject* const monitor = (GcpMgcpMonitorObject*)VectorGetElement(&self->monitors, i);
    if (GCP_MGCP_MONITOR_GET_NAME(monitor) == monitor_name) {
      return monitor;
    }
  }

  return NULL;
}

static EebusMeasurementObject* GetMeasurement(const GcpMgcpUseCase* self, GcpMeasurementNameId measurement_name) {
  const GcpMgcpMonitorObject* const monitor = GetMonitor(self, measurement_name);
  if (monitor == NULL) {
    return NULL;
  }

  return GCP_MGCP_MONITOR_GET_MEASUREMENT(monitor, measurement_name);
}

static EebusError GcpMgcpGetMeasurementDataInternal(
    const GcpMgcpUseCase* self,
    GcpMeasurementNameId measurement_name,
    ScaledValue* measurement_value
) {
  const UseCase* const use_case = USE_CASE(self);

  EebusMeasurementObject* const measurement = GetMeasurement(self, measurement_name);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  MeasurementServer msrv = {0};
  EebusError err         = MeasurementServerConstruct(&msrv, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  return EEBUS_MEASUREMENT_GET_DATA_VALUE(measurement, &msrv, measurement_value);
}

EebusError GcpMgcpGetMeasurementData(
    const GcpMgcpUseCaseObject* self,
    GcpMeasurementNameId measurement_name,
    ScaledValue* measurement_value
) {
  if (measurement_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  const UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError err
      = GcpMgcpGetMeasurementDataInternal(GCP_MGCP_USE_CASE(self), measurement_name, measurement_value);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

static EebusError GcpMgcpSetMeasurementDataCacheWithTime(
    GcpMgcpUseCase* self,
    GcpMeasurementNameId measurement_name,
    const ScaledValue* measurement_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  EebusMeasurementObject* const measurement = GetMeasurement(self, measurement_name);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  EebusError err = kEebusErrorOk;
  EEBUS_MUTEX_LOCK(self->mutex);

  err = EEBUS_MEASUREMENT_SET_DATA_CACHE(measurement, measurement_value, timestamp, value_state, start_time, end_time);

  EEBUS_MUTEX_UNLOCK(self->mutex);
  return err;
}

EebusError GcpMgcpSetMeasurementDataCache(
    GcpMgcpUseCaseObject* self,
    GcpMeasurementNameId measurement_name,
    const ScaledValue* measurement_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state
) {
  if (measurement_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  return GcpMgcpSetMeasurementDataCacheWithTime(
      GCP_MGCP_USE_CASE(self),
      measurement_name,
      measurement_value,
      timestamp,
      value_state,
      NULL,
      NULL
  );
}

EebusError GcpMgcpUpdate(const GcpMgcpUseCaseObject* self) {
  const UseCase* const use_case  = USE_CASE(self);
  GcpMgcpUseCase* const gcp_mgcp = GCP_MGCP_USE_CASE(self);

  MeasurementServer msrv = {0};
  EebusError err         = MeasurementServerConstruct(&msrv, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  MeasurementListDataType* const measurement_data_list = MeasurementsCreateEmpty();
  if (measurement_data_list == NULL) {
    return kEebusErrorMemoryAllocate;
  }

  EEBUS_MUTEX_LOCK(gcp_mgcp->mutex);
  for (size_t i = 0; i < VectorGetSize(&gcp_mgcp->monitors); ++i) {
    GcpMgcpMonitorObject* const monitor = (GcpMgcpMonitorObject*)VectorGetElement(&gcp_mgcp->monitors, i);

    err = GCP_MGCP_MONITOR_FLUSH_MEASUREMENT_CACHE(monitor, measurement_data_list);
    if (err != kEebusErrorOk) {
      EEBUS_MUTEX_UNLOCK(gcp_mgcp->mutex);
      MeasurementsDelete(measurement_data_list);
      return err;
    }
  }
  EEBUS_MUTEX_UNLOCK(gcp_mgcp->mutex);

  if (measurement_data_list->measurement_data_size > 0) {
    DEVICE_LOCAL_LOCK(use_case->local_device);
    err = MeasurementServerUpdateMeasurements(&msrv, measurement_data_list, NULL, NULL);
    DEVICE_LOCAL_UNLOCK(use_case->local_device);
  }

  MeasurementsDelete(measurement_data_list);
  return kEebusErrorOk;
}

EebusError GcpMgcpSetEnergyFeedInCache(
    GcpMgcpUseCaseObject* self,
    const ScaledValue* energy_feed_in,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  return GcpMgcpSetMeasurementDataCacheWithTime(
      GCP_MGCP_USE_CASE(self),
      kGcpEnergyFeedIn,
      energy_feed_in,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}

static EebusError GcpMgcpSetPvCurtailmentLimitFactorInternal(GcpMgcpUseCase* self, const ScaledValue* value) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationServer dcs = {0};

  const EebusError err = DeviceConfigurationServerConstruct(&dcs, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  // clang-format off
  const DeviceConfigurationKeyValueDataType data = {
      .value = &(DeviceConfigurationKeyValueValueType){
          .scaled_number = &(ScaledNumberType){
              .number = &(int64_t){value->value},
              .scale  = &(int8_t){value->scale},
          },
      },
  };
  // clang-format on

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypePvCurtailmentLimitFactor},
  };

  return DeviceConfigurationServerUpdateKeyValueWithFilter(&dcs, &data, NULL, &filter);
}

EebusError GcpMgcpSetPvCurtailmentLimitFactor(GcpMgcpUseCaseObject* self, const ScaledValue* value) {
  if (value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  if (!GCP_MGCP_USE_CASE(self)->has_scenario1) {
    return kEebusErrorNotSupported;
  }

  const UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError err = GcpMgcpSetPvCurtailmentLimitFactorInternal(GCP_MGCP_USE_CASE(self), value);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

static EebusError GcpMgcpGetPvCurtailmentLimitFactorInternal(const GcpMgcpUseCase* self, ScaledValue* value) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationServer dcs = {0};

  const EebusError err = DeviceConfigurationServerConstruct(&dcs, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name   = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypePvCurtailmentLimitFactor},
      .value_type = &(DeviceConfigurationKeyValueTypeType){kDeviceConfigurationKeyValueTypeTypeScaledNumber},
  };

  const DeviceConfigurationKeyValueDataType* const key_value
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dcs.device_cfg_common, &filter);
  const ScaledNumberType* const scaled_number = DeviceConfigurationKeyValueGetScaledNumber(key_value);
  return ScaledValueInitWithScaledNumber(value, scaled_number);
}

EebusError GcpMgcpGetPvCurtailmentLimitFactor(const GcpMgcpUseCaseObject* self, ScaledValue* value) {
  if (value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  if (!GCP_MGCP_USE_CASE(self)->has_scenario1) {
    return kEebusErrorNotSupported;
  }

  const UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError err = GcpMgcpGetPvCurtailmentLimitFactorInternal(GCP_MGCP_USE_CASE(self), value);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError GcpMgcpSetEnergyConsumedCache(
    GcpMgcpUseCaseObject* self,
    const ScaledValue* energy_consumed,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  return GcpMgcpSetMeasurementDataCacheWithTime(
      GCP_MGCP_USE_CASE(self),
      kGcpEnergyConsumed,
      energy_consumed,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}
