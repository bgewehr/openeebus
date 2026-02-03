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
#include "src/use_case/model/load_limit_types.h"

#include "src/spine/model/entity_types.h"
#include "src/use_case/actor/common/load_control.h"
#include "src/use_case/actor/eg/lpc/eg_lpc.h"
#include "src/use_case/actor/eg/lpc/eg_lpc_internal.h"
#include "src/use_case/model/load_limit_types.h"
#include "src/use_case/specialization/device_configuration/device_configuration_client.h"
#include "src/use_case/specialization/device_diagnosis/device_diagnosis_client.h"
#include "src/use_case/specialization/load_control/load_control_client.h"
#include "src/use_case/use_case.h"

//-------------------------------------------------------------------------------------------//
//
// Scenario 1
//
//-------------------------------------------------------------------------------------------//

EebusError EgLpcGetActivePowerConsumptionLimitInternal(
    const EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    LoadLimit* limit
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  LoadControlClient lcc;
  EebusError err = LoadControlClientConstruct(&lcc, use_case->local_entity, remote_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const LoadControlLimitDescriptionDataType filter = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  const LoadControlLimitDescriptionDataType* const limit_description
      = LoadControlCommonGetLimitDescriptionWithFilter(&lcc.load_control_common, &filter);
  if ((limit_description == NULL) || (limit_description->limit_id == NULL)) {
    return kEebusErrorNoChange;
  }

  const LoadControlLimitDataType* const limit_data
      = LoadControlCommonGetLimitWithId(&lcc.load_control_common, *limit_description->limit_id);

  return LoadLimitInitWithLoadControlLimitData(limit, limit_data);
}

EebusError EgLpcGetActivePowerConsumptionLimit(
    const EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    LoadLimit* limit
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (limit == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcGetActivePowerConsumptionLimitInternal(EG_LPC_USE_CASE(self), remote_entity_addr, limit);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError EgLpcSetActivePowerConsumptionLimitInternal(
    EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    const LoadLimit* limit
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  const LoadControlLimitDescriptionDataType filter = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  return LoadControlWriteLimit(use_case->local_entity, remote_entity, &filter, limit);
}

EebusError EgLpcSetActivePowerConsumptionLimit(
    EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    const LoadLimit* limit
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (limit == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcSetActivePowerConsumptionLimitInternal(EG_LPC_USE_CASE(self), remote_entity_addr, limit);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

//-------------------------------------------------------------------------------------------//
//
// Scenario 2
//
//-------------------------------------------------------------------------------------------//

EebusError EgLpcGetFailsafeConsumptionActivePowerLimitInternal(
    const EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    ScaledValue* power_limit
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  static const DeviceConfigurationKeyNameType key_name
      = kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit;

  DeviceConfigurationClient dcc;
  EebusError err = DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, remote_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name   = &key_name,
      .value_type = &(DeviceConfigurationKeyValueTypeType){kDeviceConfigurationKeyValueTypeTypeScaledNumber},
  };

  const DeviceConfigurationKeyValueDataType* const key_value
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dcc.device_cfg_common, &filter);

  const ScaledNumberType* const scaled_number = DeviceConfigurationKeyValueGetScaledNumber(key_value);
  return ScaledValueInitWithScaledNumber(power_limit, scaled_number);
}

EebusError EgLpcGetFailsafeConsumptionActivePowerLimit(
    const EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    ScaledValue* power_limit
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (power_limit == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcGetFailsafeConsumptionActivePowerLimitInternal(EG_LPC_USE_CASE(self), remote_entity_addr, power_limit);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError EgLpcSetFailsafeConsumptionActivePowerLimitInternal(
    EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    const ScaledValue* power_limit
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  static const DeviceConfigurationKeyNameType key_name
      = kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit;

  DeviceConfigurationClient dcc;
  const EebusError err = DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, remote_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &key_name,
  };

  const DeviceConfigurationKeyValueDescriptionDataType* const description
      = DeviceConfigurationCommonGetKeyValueDescriptionWithFilter(&dcc.device_cfg_common, &filter);

  if ((description == NULL) || (description->key_id == NULL)) {
    return kEebusErrorNotAvailable;
  }

  const DeviceConfigurationKeyValueDataType key_value = {
      .key_id = description->key_id,
      .value  = &(DeviceConfigurationKeyValueValueType){
          .scaled_number = &(ScaledNumberType){
               .number = &power_limit->value,
               .scale  = &power_limit->scale,
          },
      },
  };

  const DeviceConfigurationKeyValueListDataType key_value_list = {
      .device_configuration_key_value_data      = &(const DeviceConfigurationKeyValueDataType*){&key_value},
      .device_configuration_key_value_data_size = 1,
  };

  return DeviceConfigurationClientWriteKeyValueList(&dcc, &key_value_list);
}

EebusError EgLpcSetFailsafeConsumptionActivePowerLimit(
    EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    const ScaledValue* power_limit
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (power_limit == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcSetFailsafeConsumptionActivePowerLimitInternal(EG_LPC_USE_CASE(self), remote_entity_addr, power_limit);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError EgLpcGetFailsafeDurationMinimumInternal(
    const EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    DurationType* duration
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  DeviceConfigurationClient dcc;
  EebusError err = DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, remote_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name   = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
      .value_type = &(DeviceConfigurationKeyValueTypeType){kDeviceConfigurationKeyValueTypeTypeDuration},
  };

  const DeviceConfigurationKeyValueDataType* const key_value
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dcc.device_cfg_common, &filter);

  return DeviceConfigurationKeyValueGetDuration(key_value, duration);
}

EebusError EgLpcGetFailsafeDurationMinimum(
    const EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    DurationType* duration
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (duration == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcGetFailsafeDurationMinimumInternal(EG_LPC_USE_CASE(self), remote_entity_addr, duration);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

EebusError EgLpcSetFailsafeDurationMinimumInternal(
    EgLpcUseCase* self,
    const EntityAddressType* remote_entity_addr,
    const EebusDuration* duration
) {
  const UseCase* const use_case = USE_CASE(self);

  EntityRemoteObject* const remote_entity
      = USE_CASE_GET_REMOTE_ENTITY_WITH_ADDRESS(USE_CASE_OBJECT(self), remote_entity_addr);

  if (remote_entity == NULL) {
    return kEebusErrorNoChange;
  }

  static const EebusDuration two_hours         = {.hours = 2, .minutes = 0, .seconds = 0};
  static const EebusDuration twenty_four_hours = {.hours = 24, .minutes = 0, .seconds = 0};
  if (EebusDurationCompare(duration, &two_hours) < 0 || EebusDurationCompare(duration, &twenty_four_hours) > 0) {
    return kEebusErrorInputArgumentOutOfRange;
  }

  DeviceConfigurationClient dcc;
  EebusError err = DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, remote_entity);
  if (err != kEebusErrorOk) {
    return err;
  }

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
  };

  const DeviceConfigurationKeyValueDataType* const key_value_tmp
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dcc.device_cfg_common, &filter);

  if (key_value_tmp == NULL) {
    return kEebusErrorNotAvailable;
  }

  const DeviceConfigurationKeyValueDataType* const key_value = &(DeviceConfigurationKeyValueDataType){
      .key_id = key_value_tmp->key_id,
      .value  = &(DeviceConfigurationKeyValueValueType){
          .duration = duration,
      },
  };

  const DeviceConfigurationKeyValueListDataType key_value_list = {
      .device_configuration_key_value_data      = &(const DeviceConfigurationKeyValueDataType*){key_value},
      .device_configuration_key_value_data_size = 1,
  };

  return DeviceConfigurationClientWriteKeyValueList(&dcc, &key_value_list);
}

EebusError EgLpcSetFailsafeDurationMinimum(
    EgLpcUseCaseObject* self,
    const EntityAddressType* remote_entity_addr,
    const EebusDuration* duration
) {
  const UseCase* const use_case = USE_CASE(self);

  if ((remote_entity_addr == NULL) || (duration == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  EebusError err = kEebusErrorOk;

  DEVICE_LOCAL_LOCK(use_case->local_device);
  err = EgLpcSetFailsafeDurationMinimumInternal(EG_LPC_USE_CASE(self), remote_entity_addr, duration);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);

  return err;
}

//-------------------------------------------------------------------------------------------//
//
// Scenario 3
//
//-------------------------------------------------------------------------------------------//

void EgLpcStartHeartbeat(EgLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  HeartbeatManagerObject* const hm = ENTITY_LOCAL_GET_HEARTBEAT_MANAGER(use_case->local_entity);
  if (hm != NULL) {
    HEARTBEAT_MANAGER_START(hm);
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
}

void EgLpcStopHeartbeat(EgLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  HeartbeatManagerObject* const hm = ENTITY_LOCAL_GET_HEARTBEAT_MANAGER(use_case->local_entity);
  if (hm != NULL) {
    HEARTBEAT_MANAGER_STOP(hm);
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
}

bool EgLpcIsHeartbeatWithinDuration(EgLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  bool ret = false;

  DeviceDiagnosisClient ddc;
  const EebusError err = DeviceDiagnosisClientConstruct(&ddc, use_case->local_entity, NULL);

  if (err == kEebusErrorOk) {
    ret = DeviceDiagnosisCommonIsHeartbeatWithinDuration(&ddc.device_diag_common, &(DurationType){.minutes = 2});
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}
