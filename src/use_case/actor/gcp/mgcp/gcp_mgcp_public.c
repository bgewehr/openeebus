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
#include "src/use_case/specialization/measurement/measurement_server.h"
#include "src/use_case/use_case.h"

/* ---- internal helpers ---- */

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

static GcpMgcpMeasurementObject* GetMeasurement(const GcpMgcpUseCase* self, GcpMeasurementNameId measurement_name) {
  const GcpMgcpMonitorObject* const monitor = GetMonitor(self, measurement_name);
  if (monitor == NULL) {
    return NULL;
  }

  return GCP_MGCP_MONITOR_GET_MEASUREMENT(monitor, measurement_name);
}

/* ---- public API ---- */

EebusError GcpMgcpGetMeasurementData(
    const GcpMgcpUseCaseObject* self,
    GcpMeasurementNameId measurement_name,
    ScaledValue* measurement_value
) {
  const UseCase* const use_case = USE_CASE(self);

  if (measurement_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  GcpMgcpMeasurementObject* const measurement = GetMeasurement(GCP_MGCP_USE_CASE(self), measurement_name);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  MeasurementServer msrv = {0};
  EebusError err         = MeasurementServerConstruct(&msrv, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = GCP_MGCP_MEASUREMENT_GET_DATA_VALUE(measurement, &msrv, measurement_value);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

static EebusError SetMeasurementDataCacheWithTime(
    GcpMgcpUseCase* self,
    GcpMeasurementNameId measurement_name,
    const ScaledValue* measurement_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  GcpMgcpMeasurementObject* const measurement = GetMeasurement(self, measurement_name);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  EebusError err;

  EEBUS_MUTEX_LOCK(self->mutex);
  err = GCP_MGCP_MEASUREMENT_SET_DATA_CACHE(
      measurement,
      measurement_value,
      timestamp,
      value_state,
      start_time,
      end_time
  );
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

  return SetMeasurementDataCacheWithTime(
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
  return SetMeasurementDataCacheWithTime(
      GCP_MGCP_USE_CASE(self),
      kGcpEnergyFeedIn,
      energy_feed_in,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}

EebusError GcpMgcpSetEnergyConsumedCache(
    GcpMgcpUseCaseObject* self,
    const ScaledValue* energy_consumed,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  return SetMeasurementDataCacheWithTime(
      GCP_MGCP_USE_CASE(self),
      kGcpEnergyConsumed,
      energy_consumed,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}
