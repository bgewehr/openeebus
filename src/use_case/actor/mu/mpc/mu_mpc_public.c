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
#include "src/common/array_util.h"
#include "src/use_case/actor/mu/mpc/mu_mpc.h"
#include "src/use_case/actor/mu/mpc/mu_mpc_internal.h"
#include "src/use_case/model/scaled_value.h"
#include "src/use_case/specialization/measurement/measurement_server.h"
#include "src/use_case/use_case.h"

EebusMonitorObject* GetMonitor(const MuMpcUseCase* self, MuMpcMeasurementNameId measurement_name) {
  const EebusMeasurementMonitorNameId monitor_name
      = (EebusMeasurementMonitorNameId)((uint8_t)measurement_name & (uint8_t)kMpcMonitorNameIdMask);

  for (size_t i = 0; i < VectorGetSize(&self->monitors); ++i) {
    EebusMonitorObject* const mu_mpc_monitor = (EebusMonitorObject*)VectorGetElement(&self->monitors, i);
    if (EEBUS_MONITOR_GET_NAME(mu_mpc_monitor) == monitor_name) {
      return mu_mpc_monitor;
    }
  }

  return NULL;
}

EebusMeasurementObject* GetMeasurement(const MuMpcUseCase* self, MuMpcMeasurementNameId measurement_name) {
  const EebusMonitorObject* const monitor = GetMonitor(self, measurement_name);
  if (monitor == NULL) {
    return NULL;
  }

  return EEBUS_MONITOR_GET_MEASUREMENT(monitor, measurement_name);
}

EebusError MuMpcGetMeasurementDataInternal(
    const MuMpcUseCase* self,
    const MuMpcMeasurementNameId measurement_element_id,
    ScaledValue* measurement_value
) {
  const UseCase* const use_case = USE_CASE(self);

  EebusMeasurementObject* measurement = GetMeasurement(self, measurement_element_id);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  MeasurementServer msrv = {0};

  EebusError err = MeasurementServerConstruct(&msrv, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  return EEBUS_MEASUREMENT_GET_DATA_VALUE(measurement, &msrv, measurement_value);
}

EebusError MuMpcGetMeasurementData(
    const MuMpcUseCaseObject* self,
    MuMpcMeasurementNameId measurement_element_id,
    ScaledValue* measurement_value
) {
  const UseCase* const use_case = USE_CASE(self);

  if (measurement_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = MuMpcGetMeasurementDataInternal(MU_MPC_USE_CASE(self), measurement_element_id, measurement_value);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError MuMpcSetMeasurementDataCacheWithTime(
    MuMpcUseCase* self,
    MuMpcMeasurementNameId measurement_name,
    const ScaledValue* measurement_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  EebusMeasurementObject* measurement = GetMeasurement(self, measurement_name);
  if (measurement == NULL) {
    return kEebusErrorNotSupported;
  }

  EebusError err = kEebusErrorOk;

  EEBUS_MUTEX_LOCK(self->mutex);
  err = EEBUS_MEASUREMENT_SET_DATA_CACHE(measurement, measurement_value, timestamp, value_state, start_time, end_time);
  EEBUS_MUTEX_UNLOCK(self->mutex);

  return err;
}

EebusError MuMpcSetMeasurementDataCache(
    MuMpcUseCaseObject* self,
    MuMpcMeasurementNameId measurement_name,
    const ScaledValue* measurement_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state
) {
  if (measurement_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  return MuMpcSetMeasurementDataCacheWithTime(
      MU_MPC_USE_CASE(self),
      measurement_name,
      measurement_value,
      timestamp,
      value_state,
      NULL,
      NULL
  );
}

EebusError MuMpcUpdate(const MuMpcUseCaseObject* self) {
  UseCase* const use_case    = USE_CASE(self);
  MuMpcUseCase* const mu_mpc = MU_MPC_USE_CASE(self);

  MeasurementServer msrv = {0};

  EebusError err = MeasurementServerConstruct(&msrv, use_case->local_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  MeasurementListDataType* const measurement_data_list = MeasurementsCreateEmpty();
  if (measurement_data_list == NULL) {
    return kEebusErrorMemoryAllocate;
  }

  EEBUS_MUTEX_LOCK(mu_mpc->mutex);
  for (size_t i = 0; i < VectorGetSize(&mu_mpc->monitors); ++i) {
    EebusMonitorObject* const mu_mpc_monitor = (EebusMonitorObject*)VectorGetElement(&mu_mpc->monitors, i);
    EebusError err = EEBUS_MONITOR_FLUSH_MEASUREMENT_CACHE(mu_mpc_monitor, measurement_data_list);
    if (err != kEebusErrorOk) {
      EEBUS_MUTEX_UNLOCK(mu_mpc->mutex);
      return err;
    }
  }

  EEBUS_MUTEX_UNLOCK(mu_mpc->mutex);

  if (measurement_data_list->measurement_data_size > 0) {
    DEVICE_LOCAL_LOCK(use_case->local_device);
    err = MeasurementServerUpdateMeasurements(&msrv, measurement_data_list, NULL, NULL);
    DEVICE_LOCAL_UNLOCK(use_case->local_device);
  }

  MeasurementsDelete(measurement_data_list);
  return kEebusErrorOk;
}

EebusError MuMpcSetEnergyConsumedCache(
    MuMpcUseCaseObject* self,
    const ScaledValue* energy_consumed,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  MuMpcUseCase* const mu_mpc = MU_MPC_USE_CASE(self);
  return MuMpcSetMeasurementDataCacheWithTime(
      mu_mpc,
      kMpcEnergyConsumed,
      energy_consumed,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}

EebusError MuMpcSetEnergyProducedCache(
    MuMpcUseCaseObject* self,
    const ScaledValue* energy_produced,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  MuMpcUseCase* const mu_mpc = MU_MPC_USE_CASE(self);
  return MuMpcSetMeasurementDataCacheWithTime(
      mu_mpc,
      kMpcEnergyProduced,
      energy_produced,
      timestamp,
      value_state,
      start_time,
      end_time
  );
}
