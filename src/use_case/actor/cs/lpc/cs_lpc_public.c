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
 * @brief CS LPC public functions
 */

#include "src/common/array_util.h"
#include "src/common/eebus_data/eebus_data_list.h"
#include "src/common/eebus_errors.h"
#include "src/spine/model/loadcontrol_types.h"
#include "src/spine/model/model.h"
#include "src/use_case/actor/common/load_control.h"
#include "src/use_case/actor/cs/lpc/cs_lpc.h"
#include "src/use_case/actor/cs/lpc/cs_lpc_internal.h"
#include "src/use_case/model/load_limit_types.h"
#include "src/use_case/specialization/device_configuration/device_configuration_server.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/load_control/load_control_common.h"
#include "src/use_case/specialization/load_control/load_control_server.h"

//-------------------------------------------------------------------------------------------//
//
// Scenario 1
//
//-------------------------------------------------------------------------------------------//
EebusError GetLimitId(LoadControlServer* load_control_server, LoadControlLimitIdType* limit_id) {
  if ((load_control_server == NULL) || (limit_id == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  const LoadControlLimitDescriptionDataType filter = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_category  = &(LoadControlCategoryType){kLoadControlCategoryTypeObligation},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  const LoadControlLimitDescriptionDataType* const description
      = LoadControlCommonGetLimitDescriptionWithFilter(&load_control_server->load_control_common, &filter);
  if (description == NULL) {
    return kEebusErrorNoChange;
  }

  *limit_id = *description->limit_id;
  return kEebusErrorOk;
}

EebusError GetConsumptionLimitInternal(const CsLpcUseCase* self, LoadLimit* limit) {
  UseCase* const use_case = USE_CASE(self);

  LoadControlServer lcs;
  const EebusError lcs_construct_err = LoadControlServerConstruct(&lcs, use_case->local_entity);
  if (lcs_construct_err != kEebusErrorOk) {
    return lcs_construct_err;
  }

  LoadControlLimitIdType limit_id;
  const EebusError limid_id_err = GetLimitId(&lcs, &limit_id);
  if (limid_id_err != kEebusErrorOk) {
    return limid_id_err;
  }

  const LoadControlLimitDataType* const limit_data
      = LoadControlCommonGetLimitWithId(&lcs.load_control_common, limit_id);

  return LoadLimitInitWithLoadControlLimitData(limit, limit_data);
}

EebusError GetConsumptionLimit(const CsLpcUseCaseObject* self, LoadLimit* limit) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = GetConsumptionLimitInternal(CS_LPC_USE_CASE(self), limit);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

EebusError
SetConsumptionLimitInternal(CsLpcUseCase* self, int64_t limit, int8_t scale, bool is_active, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  LoadControlServer lcs;
  const EebusError lcs_construct_err = LoadControlServerConstruct(&lcs, use_case->local_entity);
  if (lcs_construct_err != kEebusErrorOk) {
    return lcs_construct_err;
  }

  LoadControlLimitIdType limit_id;
  const EebusError limid_id_err = GetLimitId(&lcs, &limit_id);
  if (limid_id_err != kEebusErrorOk) {
    return limid_id_err;
  }

  // TODO: Add duration handling
  const LoadControlLimitDataType limit_data = {
      .is_limit_changeable = &(bool){is_changeable},
      .is_limit_active     = &(bool){is_active},
      .value               = &(ScaledNumberType){.number = &limit, .scale = &scale},
  };

  const LoadControlLimitDescriptionDataType filter = {
      .limit_id = &limit_id,
  };

  const LoadControlLimitListDataSelectorsType delete_selectors = {
      .limit_id = &limit_id,
  };

  return LoadControlServerUpdateLimitWithFilter(&lcs, &limit_data, &filter, (void*)&delete_selectors, NULL);
}

EebusError
SetConsumptionLimit(CsLpcUseCaseObject* self, int64_t limit, int8_t scale, bool is_active, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = SetConsumptionLimitInternal(CS_LPC_USE_CASE(self), limit, scale, is_active, is_changeable);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

// TODO: add pending requests handling API

//-------------------------------------------------------------------------------------------//
//
// Scenario 2
//
//-------------------------------------------------------------------------------------------//
EebusError GetFailsafeConsumptionActivePowerLimitInternal(
    const CsLpcUseCase* self,
    ScaledValue* power_limit,
    bool* is_changeable
) {
  UseCase* const use_case = USE_CASE(self);

  if ((power_limit == NULL) || (is_changeable == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  DeviceConfigurationServer dc      = {0};
  const EebusError dc_construct_err = DeviceConfigurationServerConstruct(&dc, use_case->local_entity);
  if (dc_construct_err != kEebusErrorOk) {
    return dc_construct_err;
  }

  DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit},
  };

  const DeviceConfigurationKeyValueDataType* const key_data
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dc.device_cfg_common, &filter);
  if (!DeviceConfigurationKeyValueIsValid(key_data)) {
    return kEebusErrorOther;
  }

  const ScaledNumberType* const scaled_number = DeviceConfigurationKeyValueGetScaledNumber(key_data);

  EebusError err = ScaledValueInitWithScaledNumber(power_limit, scaled_number);
  if (err != kEebusErrorOk) {
    return err;
  }

  *is_changeable = DeviceConfigurationKeyValueIsChangeable(key_data);
  return kEebusErrorOk;
}

EebusError
GetFailsafeConsumptionActivePowerLimit(const CsLpcUseCaseObject* self, ScaledValue* power_limit, bool* is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret
      = GetFailsafeConsumptionActivePowerLimitInternal(CS_LPC_USE_CASE(self), power_limit, is_changeable);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

EebusError
SetFailsafeConsumptionActivePowerLimitInternal(CsLpcUseCase* self, const ScaledValue* power_limit, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationServer dc      = {0};
  const EebusError dc_construct_err = DeviceConfigurationServerConstruct(&dc, use_case->local_entity);
  if (dc_construct_err != kEebusErrorOk) {
    return dc_construct_err;
  }

  const DeviceConfigurationKeyValueDataType data = {
      .value = &(DeviceConfigurationKeyValueValueType){
          .scaled_number = &(ScaledNumberType){
              .number = &(int64_t){power_limit->value},
              .scale  = &(int8_t){power_limit->scale},
          },
      },

      .is_value_changeable = &is_changeable,
  };

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit},
  };

  return DeviceConfigurationServerUpdateKeyValueWithFilter(&dc, &data, NULL, &filter);
}

EebusError
SetFailsafeConsumptionActivePowerLimit(CsLpcUseCaseObject* self, const ScaledValue* power_limit, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret
      = SetFailsafeConsumptionActivePowerLimitInternal(CS_LPC_USE_CASE(self), power_limit, is_changeable);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

EebusError GetFailsafeDurationMinimumInternal(const CsLpcUseCase* self, DurationType* duration, bool* is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  if ((duration == NULL) || (is_changeable == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  DeviceConfigurationServer dc      = {0};
  const EebusError dc_construct_err = DeviceConfigurationServerConstruct(&dc, use_case->local_entity);
  if (dc_construct_err != kEebusErrorOk) {
    return dc_construct_err;
  }

  DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
  };

  const DeviceConfigurationKeyValueDataType* const key_data
      = DeviceConfigurationCommonGetKeyValueWithFilter(&dc.device_cfg_common, &filter);
  if (!DeviceConfigurationKeyValueIsValid(key_data) || (key_data->value->duration == NULL)) {
    return kEebusErrorOther;
  }

  if (DeviceConfigurationKeyValueGetDuration(key_data, duration) != kEebusErrorOk) {
    return kEebusErrorOther;
  }

  *is_changeable = DeviceConfigurationKeyValueIsChangeable(key_data);
  return kEebusErrorOk;
}

EebusError GetFailsafeDurationMinimum(const CsLpcUseCaseObject* self, DurationType* duration, bool* is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = GetFailsafeDurationMinimumInternal(CS_LPC_USE_CASE(self), duration, is_changeable);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

EebusError SetFailsafeDurationMinimumInternal(CsLpcUseCase* self, const DurationType* duration, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  // TODO: Add duration range check

  if (duration == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  DeviceConfigurationServer dc      = {0};
  const EebusError dc_construct_err = DeviceConfigurationServerConstruct(&dc, use_case->local_entity);
  if (dc_construct_err != kEebusErrorOk) {
    return dc_construct_err;
  }

  const DeviceConfigurationKeyValueDataType data = {
    .value = &(DeviceConfigurationKeyValueValueType){
      .duration = (DurationType*)duration,
    },

    .is_value_changeable = &is_changeable,
  };

  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
  };

  return DeviceConfigurationServerUpdateKeyValueWithFilter(&dc, &data, NULL, &filter);
}

EebusError SetFailsafeDurationMinimum(CsLpcUseCaseObject* self, const DurationType* duration, bool is_changeable) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = SetFailsafeDurationMinimumInternal(CS_LPC_USE_CASE(self), duration, is_changeable);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

//-------------------------------------------------------------------------------------------//
//
// Scenario 3
//
//-------------------------------------------------------------------------------------------//
void StartHeartbeat(CsLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  HeartbeatManagerObject* const hm = ENTITY_LOCAL_GET_HEARTBEAT_MANAGER(use_case->local_entity);
  if (hm != NULL) {
    HEARTBEAT_MANAGER_START(hm);
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
}

void StopHeartbeat(CsLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  HeartbeatManagerObject* const hm = ENTITY_LOCAL_GET_HEARTBEAT_MANAGER(use_case->local_entity);
  if (hm != NULL) {
    HEARTBEAT_MANAGER_STOP(hm);
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
}

bool IsHeartbeatWithinDuration(CsLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  bool ret = false;

  const DeviceDiagnosisClient* const hdc = CS_LPC_USE_CASE(self)->heartbeat_diag_client;
  if (hdc != NULL) {
    ret = DeviceDiagnosisCommonIsHeartbeatWithinDuration(&hdc->device_diag_common, &(DurationType){.minutes = 2});
  }

  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

//-------------------------------------------------------------------------------------------//
//
// Scenario 4
//
//-------------------------------------------------------------------------------------------//
const ElectricalConnectionCharacteristicDataType*
GetElectricalConnectionCharacteristics(const CsLpcUseCase* self, const ElectricalConnectionServer* ecs) {
  ElectricalConnectionCharacteristicContextType characteristic_context
      = kElectricalConnectionCharacteristicContextTypeEntity;

  ElectricalConnectionCharacteristicTypeType characteristic_type
      = GetElectricalConnectionCharacteristicTypeInternal(self);

  ElectricalConnectionCharacteristicDataType filter = {
      .electrical_connection_id = &(ElectricalConnectionIdType){0},
      .parameter_id             = &(ElectricalConnectionParameterIdType){0},
      .characteristic_context   = &characteristic_context,
      .characteristic_type      = &characteristic_type,
  };

  return ElectricalConnectionCommonGetCharacteristicWithFilter(&ecs->el_connection_common, &filter);
}

EebusError GetConsumptionNominalMaxInternal(const CsLpcUseCase* self, ScaledValue* nominal_max) {
  UseCase* const use_case = USE_CASE(self);

  if (nominal_max == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  ElectricalConnectionServer ecs;
  const EebusError ecs_construct_err = ElectricalConnectionServerConstruct(&ecs, use_case->local_entity);
  if (ecs_construct_err != kEebusErrorOk) {
    return ecs_construct_err;
  }

  const ElectricalConnectionCharacteristicDataType* const characteristic
      = GetElectricalConnectionCharacteristics(self, &ecs);

  if ((characteristic->characteristic_id == NULL) || (characteristic->value == NULL)) {
    return kEebusErrorNoChange;
  }

  return ScaledValueInitWithScaledNumber(nominal_max, characteristic->value);
}

EebusError GetConsumptionNominalMax(CsLpcUseCaseObject* self, ScaledValue* nominal_max) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = GetConsumptionNominalMaxInternal(CS_LPC_USE_CASE(self), nominal_max);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

EebusError SetConsumptionNominalMaxInternal(CsLpcUseCase* self, const ScaledValue* new_nominal_max) {
  UseCase* const use_case = USE_CASE(self);

  if (new_nominal_max == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  ElectricalConnectionServer ecs;
  const EebusError ecs_construct_err = ElectricalConnectionServerConstruct(&ecs, use_case->local_entity);
  if (ecs_construct_err != kEebusErrorOk) {
    return ecs_construct_err;
  }

  const ElectricalConnectionCharacteristicDataType* const characteristic
      = GetElectricalConnectionCharacteristics(self, &ecs);

  if (characteristic->characteristic_id == NULL) {
    return kEebusErrorNoChange;
  }

  ElectricalConnectionCharacteristicDataType new_characteristic = {
      .electrical_connection_id = &(ElectricalConnectionIdType){0},
      .parameter_id             = &(ElectricalConnectionParameterIdType){0},
      .characteristic_id        = characteristic->characteristic_id,

      .value = &(ScaledNumberType){
          .number = &(int64_t){new_nominal_max->value},
          .scale  = &(int8_t){new_nominal_max->scale},
      },
  };

  return ElectricalConnectionServerUpdateCharacteristic(&ecs, &new_characteristic, NULL);
}

EebusError SetConsumptionNominalMax(CsLpcUseCaseObject* self, const ScaledValue* new_nominal_max) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const EebusError ret = SetConsumptionNominalMaxInternal(CS_LPC_USE_CASE(self), new_nominal_max);
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}

ElectricalConnectionCharacteristicTypeType GetElectricalConnectionCharacteristicTypeInternal(const CsLpcUseCase* self) {
  UseCase* const use_case = USE_CASE(self);

  const DeviceTypeType* const device_type = DEVICE_GET_DEVICE_TYPE(DEVICE_OBJECT(use_case->local_device));

  // According to LPC V1.0 2.2, lines 400ff:
  // - a HEMS provides contractual consumption nominal max
  // - any other devices provides power consupmtion nominal max
  ElectricalConnectionCharacteristicTypeType ret
      = kElectricalConnectionCharacteristicTypeTypePowerConsumptionNominalMax;

  if ((device_type == NULL) || (*device_type == kDeviceTypeTypeEnergyManagementSystem)) {
    ret = kElectricalConnectionCharacteristicTypeTypeContractualConsumptionNominalMax;
  }

  return ret;
}

ElectricalConnectionCharacteristicTypeType GetElectricalConnectionCharacteristicType(const CsLpcUseCaseObject* self) {
  UseCase* const use_case = USE_CASE(self);

  DEVICE_LOCAL_LOCK(use_case->local_device);
  const ElectricalConnectionCharacteristicTypeType ret
      = GetElectricalConnectionCharacteristicTypeInternal(CS_LPC_USE_CASE(self));
  DEVICE_LOCAL_UNLOCK(use_case->local_device);
  return ret;
}
