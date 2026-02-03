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
 * @brief Controllable System LPC use case implementation
 */

#include "src/use_case/actor/cs/lpc/cs_lpc.h"

#include <stddef.h>

#include "src/common/array_util.h"
#include "src/spine/entity/entity_local.h"
#include "src/spine/feature/feature_local.h"
#include "src/spine/model/loadcontrol_types.h"
#include "src/spine/model/model.h"
#include "src/spine/model/usecase_information_types.h"
#include "src/use_case/actor/cs/lpc/cs_lpc_events.h"
#include "src/use_case/actor/cs/lpc/cs_lpc_internal.h"
#include "src/use_case/specialization/device_configuration/device_configuration_server.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_server.h"
#include "src/use_case/specialization/load_control/load_control_server.h"
#include "src/use_case/use_case.h"

static void CsLpcUseCaseDestruct(UseCaseObject* self);

static const UseCaseInterface lpc_use_case_methods = {
    .destruct                       = CsLpcUseCaseDestruct,
    .is_entity_compatible           = UseCaseIsEntityCompatible,
    .is_use_case_compatible         = UseCaseIsUseCaseCompatible,
    .get_remote_entity_with_address = UseCaseGetRemoteEntityWithAddress,
};

static const UseCaseActorType valid_actor_types[] = {kUseCaseActorTypeEnergyGuard};
static const EntityTypeType valid_entity_types[]  = {
    kEntityTypeTypeGridGuard,
    kEntityTypeTypeCEM,  // KEO uses this entity type for an SMGW whysoever
};

static const FeatureTypeType use_case_scenario_support_3_features[] = {kFeatureTypeTypeDeviceDiagnosis};

static const UseCaseScenario use_case_scenarios[] = {
    {
     .scenario  = (UseCaseScenarioSupportType)1,
     .mandatory = true,
     },
    {
     .scenario  = (UseCaseScenarioSupportType)2,
     .mandatory = true,
     },
    {
     .scenario             = (UseCaseScenarioSupportType)3,
     .mandatory            = true,
     .server_features      = use_case_scenario_support_3_features,
     .server_features_size = ARRAY_SIZE(use_case_scenario_support_3_features),
     },
    {
     .scenario  = (UseCaseScenarioSupportType)4,
     .mandatory = true,
     },
};

static const UseCaseInfo cs_lpc_use_case_info = {
    .valid_actor_types       = valid_actor_types,
    .valid_actor_types_size  = ARRAY_SIZE(valid_actor_types),
    .valid_entity_types      = valid_entity_types,
    .valid_entity_types_size = ARRAY_SIZE(valid_entity_types),
    .use_case_scenarios      = use_case_scenarios,
    .use_case_scenarios_size = ARRAY_SIZE(use_case_scenarios),
    .actor                   = kUseCaseActorTypeControllableSystem,
    .use_case_name_id        = kUseCaseNameTypeLimitationOfPowerConsumption,
    .version                 = "1.0.0",
    .sub_revision            = "release",
    .available               = true,
};

static void AddFeatures(UseCaseObject* self, EntityLocalObject* entity);
static void CsLpcUseCaseConstruct(
    CsLpcUseCase* self,
    EntityLocalObject* local_entity,
    ElectricalConnectionIdType ec_id,
    CsLpcListenerObject* cs_lpc_listener
);

void AddLoadControlFeature(UseCaseObject* self, EntityLocalObject* entity) {
  FeatureLocalObject* const fl
      = ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeLoadControl, kRoleTypeServer);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeLoadControlLimitDescriptionListData, true, false);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeLoadControlLimitListData, true, true);
  // TODO: Add write approval callback
  // _ = f.AddWriteApprovalCallback(e.loadControlWriteCB)

  LoadControlServer lc;
  const EebusError err = LoadControlServerConstruct(&lc, entity);
  if (err != kEebusErrorOk) {
    return;
  }

  // measurement_id = 0 is a fake Measurement ID, as there is no Electrical Connection server defined,
  // it can't provide any meaningful. But KEO requires this to be set
  LoadControlLimitDescriptionDataType new_limit_desc = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_category  = &(LoadControlCategoryType){kLoadControlCategoryTypeObligation},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .measurement_id  = &(MeasurementIdType){(MeasurementIdType)0},
      .unit            = &(UnitOfMeasurementType){kUnitOfMeasurementTypeW},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  LoadControlLimitIdType limit_id = 0;
  if (LoadControlServerAddLimitDescription(&lc, &new_limit_desc, &limit_id) != kEebusErrorOk) {
    return;
  }

  LoadControlLimitDataType limit_data = {
      .value               = &(ScaledNumberType){0},
      .is_limit_changeable = &(bool){true},
      .is_limit_active     = &(bool){false},
  };

  LoadControlServerUpdateLimitWithId(&lc, &limit_data, limit_id);
}

void AddDeviceConfigurationFeature(UseCaseObject* self, EntityLocalObject* entity) {
  FeatureLocalObject* const fl
      = ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeDeviceConfiguration, kRoleTypeServer);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeDeviceConfigurationKeyValueDescriptionListData, true, false);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeDeviceConfigurationKeyValueListData, true, true);

  DeviceConfigurationServer dcs;
  if (DeviceConfigurationServerConstruct(&dcs, entity) != kEebusErrorOk) {
    return;
  }

  const DeviceConfigurationKeyValueDescriptionDataType failsafe_consumption_description = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit},
      .value_type = &(DeviceConfigurationKeyValueTypeType){kDeviceConfigurationKeyValueTypeTypeScaledNumber},
      .unit       = &(UnitOfMeasurementType){kUnitOfMeasurementTypeW},
  };

  DeviceConfigurationServerAddKeyValueDescription(&dcs, &failsafe_consumption_description);

  // Only add if it doesn't exist yet
  const DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
  };

  EebusDataListMatchIterator it = {0};
  DeviceConfigurationCommonKeyValueDescriptionMatchFirst(&dcs.device_cfg_common, &filter, &it);
  if (EebusDataListMatchIteratorIsDone(&it)) {
    DeviceConfigurationKeyValueDescriptionDataType failsafe_duration_min_description = {
        .key_name   = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
        .value_type = &(DeviceConfigurationKeyValueTypeType){kDeviceConfigurationKeyValueTypeTypeDuration},
    };

    DeviceConfigurationServerAddKeyValueDescription(&dcs, &failsafe_duration_min_description);
  }

  DeviceConfigurationKeyValueDataType failsafe_power_limit = {
     .value = &(DeviceConfigurationKeyValueValueType) {
       .scaled_number = &(ScaledNumberType){0},
     },
 
     .is_value_changeable = &(bool){true},
   };

  DeviceConfigurationKeyValueDescriptionDataType failsafe_power_descripton = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit},
  };

  DeviceConfigurationServerUpdateKeyValueWithFilter(&dcs, &failsafe_power_limit, NULL, &failsafe_power_descripton);

  const DeviceConfigurationKeyValueDataType failsafe_duration_minimum = {
     .value = &(DeviceConfigurationKeyValueValueType) {
       .duration = &(DurationType){0},
     },
 
     .is_value_changeable = &(bool){true},
   };

  DeviceConfigurationKeyValueDescriptionDataType failsafe_duration_description = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum},
  };

  DeviceConfigurationServerUpdateKeyValueWithFilter(
      &dcs,
      &failsafe_duration_minimum,
      NULL,
      &failsafe_duration_description
  );
}

void AddDeviceDiagnosisFeature(UseCaseObject* self, EntityLocalObject* entity) {
  FeatureLocalObject* const fl
      = ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeDeviceDiagnosis, kRoleTypeServer);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeDeviceDiagnosisHeartbeatData, true, false);
}

void AddElectricalConnection(UseCaseObject* self, EntityLocalObject* entity) {
  CsLpcUseCase* const cs_lpc = CS_LPC_USE_CASE(self);

  FeatureLocalObject* const fl
      = ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeElectricalConnection, kRoleTypeServer);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeElectricalConnectionCharacteristicListData, true, false);

  ElectricalConnectionServer ecs;
  if (ElectricalConnectionServerConstruct(&ecs, entity) != kEebusErrorOk) {
    return;
  }

  static const ElectricalConnectionCharacteristicContextType characteristic_context
      = kElectricalConnectionCharacteristicContextTypeEntity;

  const ElectricalConnectionCharacteristicTypeType characteristic_type
      = GetElectricalConnectionCharacteristicType(CS_LPC_USE_CASE_OBJECT(self));

  const ElectricalConnectionCharacteristicDataType new_characteristic = {
      .electrical_connection_id = &(ElectricalConnectionIdType){cs_lpc->electrical_connection_id},
      .parameter_id             = &(ElectricalConnectionParameterIdType){0},
      .characteristic_context   = &(ElectricalConnectionCharacteristicContextType){characteristic_context},
      .characteristic_type      = &(ElectricalConnectionCharacteristicTypeType){characteristic_type},
      .unit                     = &(UnitOfMeasurementType){kUnitOfMeasurementTypeW},
  };

  ElectricalConnectionServerAddCharacteristic(&ecs, &new_characteristic);
}

void AddFeatures(UseCaseObject* self, EntityLocalObject* entity) {
  // Client features
  ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeDeviceDiagnosis, kRoleTypeClient);

  // Server features
  AddLoadControlFeature(self, entity);
  AddDeviceConfigurationFeature(self, entity);
  AddDeviceDiagnosisFeature(self, entity);
  AddElectricalConnection(self, entity);
}

void CsLpcUseCaseConstruct(
    CsLpcUseCase* self,
    EntityLocalObject* local_entity,
    ElectricalConnectionIdType ec_id,
    CsLpcListenerObject* cs_lpc_listener
) {
  UseCaseConstruct(USE_CASE(self), &cs_lpc_use_case_info, local_entity, CsLpcHandleEvent);
  // Override "virtual functions table"
  USE_CASE_INTERFACE(self) = &lpc_use_case_methods;

  self->electrical_connection_id = ec_id;
  self->cs_lpc_listener          = cs_lpc_listener;
  AddFeatures(USE_CASE_OBJECT(self), local_entity);
  self->heartbeat_diag_client    = NULL;
  self->heartbeat_keo_workaround = false;
}

CsLpcUseCaseObject* CsLpcUseCaseCreate(
    EntityLocalObject* local_entity,
    ElectricalConnectionIdType ec_id,
    CsLpcListenerObject* cs_lpc_listener
) {
  CsLpcUseCase* cs_lpc_use_case = EEBUS_MALLOC(sizeof(*cs_lpc_use_case));
  if (cs_lpc_use_case == NULL) {
    return NULL;
  }

  CsLpcUseCaseConstruct(cs_lpc_use_case, local_entity, ec_id, cs_lpc_listener);

  return CS_LPC_USE_CASE_OBJECT(cs_lpc_use_case);
}

void CsLpcUseCaseDestruct(UseCaseObject* self) {
  CsLpcUseCase* cs_lpc = CS_LPC_USE_CASE(self);

  DeviceDiagnosisClientDelete(cs_lpc->heartbeat_diag_client);

  UseCaseDestruct(self);
}
