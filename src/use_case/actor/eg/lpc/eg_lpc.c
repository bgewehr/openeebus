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
 * @brief Energy Guard LPC use case implementation
 */

#include "src/use_case/actor/eg/lpc/eg_lpc.h"

#include <stddef.h>

#include "src/common/array_util.h"
#include "src/spine/entity/entity_local.h"
#include "src/spine/feature/feature_local.h"
#include "src/spine/model/loadcontrol_types.h"
#include "src/spine/model/usecase_information_types.h"
#include "src/use_case/actor/eg/lpc/eg_lpc_events.h"
#include "src/use_case/actor/eg/lpc/eg_lpc_internal.h"
#include "src/use_case/use_case.h"

static const UseCaseInterface lpc_use_case_methods = {
    .destruct                       = UseCaseDestruct,
    .is_entity_compatible           = UseCaseIsEntityCompatible,
    .is_use_case_compatible         = UseCaseIsUseCaseCompatible,
    .get_remote_entity_with_address = UseCaseGetRemoteEntityWithAddress,
};

static const UseCaseActorType valid_actor_types[] = {kUseCaseActorTypeControllableSystem};
static const EntityTypeType valid_entity_types[]  = {
    kEntityTypeTypeCEM,
    kEntityTypeTypeCompressor,
    kEntityTypeTypeEVSE,
    kEntityTypeTypeHeatPumpAppliance,
    kEntityTypeTypeInverter,
    kEntityTypeTypeSmartEnergyAppliance,
    kEntityTypeTypeSubMeterElectricity,
};

static const FeatureTypeType use_case_scenario_support_1_features[] = {kFeatureTypeTypeLoadControl};
static const FeatureTypeType use_case_scenario_support_2_features[] = {kFeatureTypeTypeDeviceConfiguration};
#if 0
static const FeatureTypeType use_case_scenario_support_3_features[] = {kFeatureTypeTypeDeviceDiagnosis};
#endif
static const FeatureTypeType use_case_scenario_support_4_features[] = {kFeatureTypeTypeElectricalConnection};

static const UseCaseScenario use_case_scenarios[] = {
    {
     .scenario             = (UseCaseScenarioSupportType)1,
     .mandatory            = true,
     .server_features      = use_case_scenario_support_1_features,
     .server_features_size = ARRAY_SIZE(use_case_scenario_support_1_features),
     },
    {
     .scenario             = (UseCaseScenarioSupportType)2,
     .mandatory            = true,
     .server_features      = use_case_scenario_support_2_features,
     .server_features_size = ARRAY_SIZE(use_case_scenario_support_2_features),
     },
#if 0
    {
     .scenario             = (UseCaseScenarioSupportType)3,
     .mandatory            = true,
     .server_features      = use_case_scenario_support_3_features,
     .server_features_size = ARRAY_SIZE(use_case_scenario_support_3_features),
     },
#endif
    {
     .scenario             = (UseCaseScenarioSupportType)4,
     .mandatory            = false,
     .server_features      = use_case_scenario_support_4_features,
     .server_features_size = ARRAY_SIZE(use_case_scenario_support_4_features),
     },
};

static const UseCaseInfo eg_lpc_use_case_info = {
    .valid_actor_types       = valid_actor_types,
    .valid_actor_types_size  = ARRAY_SIZE(valid_actor_types),
    .valid_entity_types      = valid_entity_types,
    .valid_entity_types_size = ARRAY_SIZE(valid_entity_types),
    .use_case_scenarios      = use_case_scenarios,
    .use_case_scenarios_size = ARRAY_SIZE(use_case_scenarios),
    .actor                   = kUseCaseActorTypeEnergyGuard,
    .use_case_name_id        = kUseCaseNameTypeLimitationOfPowerConsumption,
    .version                 = "1.0.0",
    .sub_revision            = "release",
    .available               = true,
};

static void AddFeatures(EntityLocalObject* entity);
static void
EgLpcUseCaseConstruct(EgLpcUseCase* self, EntityLocalObject* local_entity, EgLpcListenerObject* eg_lpc_listener);

void AddFeatures(EntityLocalObject* entity) {
  // Energy Guard LPC client features
  static const FeatureTypeType eg_lpc_client_features[] = {
      kFeatureTypeTypeDeviceDiagnosis,
      kFeatureTypeTypeLoadControl,
      kFeatureTypeTypeDeviceConfiguration,
      kFeatureTypeTypeElectricalConnection,
  };

  for (size_t i = 0; i < ARRAY_SIZE(eg_lpc_client_features); ++i) {
    ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, eg_lpc_client_features[i], kRoleTypeClient);
  }

  // server features
  FeatureLocalObject* const fl
      = ENTITY_LOCAL_ADD_FEATURE_WITH_TYPE_AND_ROLE(entity, kFeatureTypeTypeDeviceDiagnosis, kRoleTypeServer);
  FEATURE_LOCAL_SET_FUNCTION_OPERATIONS(fl, kFunctionTypeDeviceDiagnosisHeartbeatData, true, false);
}

void EgLpcUseCaseConstruct(EgLpcUseCase* self, EntityLocalObject* local_entity, EgLpcListenerObject* eg_lpc_listener) {
  UseCaseConstruct(USE_CASE(self), &eg_lpc_use_case_info, local_entity, EgLpcHandleEvent);
  // Override "virtual functions table"
  USE_CASE_INTERFACE(self) = &lpc_use_case_methods;

  self->eg_lpc_listener = eg_lpc_listener;
  AddFeatures(local_entity);
}

EgLpcUseCaseObject* EgLpcUseCaseCreate(EntityLocalObject* local_entity, EgLpcListenerObject* eg_lpc_listener) {
  EgLpcUseCase* eg_lpc_use_case = EEBUS_MALLOC(sizeof(*eg_lpc_use_case));
  if (eg_lpc_use_case == NULL) {
    return NULL;
  }

  EgLpcUseCaseConstruct(eg_lpc_use_case, local_entity, eg_lpc_listener);

  return EG_LPC_USE_CASE_OBJECT(eg_lpc_use_case);
}
