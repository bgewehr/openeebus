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
 * @brief Energy Guard LPC events handling implementation
 */

#include "src/spine/events/events.h"
#include "src/spine/model/device_diagnosis_types.h"
#include "src/use_case/actor/eg/lpc/eg_lpc_internal.h"
#include "src/use_case/specialization/device_configuration/device_configuration_client.h"
#include "src/use_case/specialization/device_diagnosis/device_diagnosis_client.h"
#include "src/use_case/specialization/load_control/load_control_client.h"

static void OnLoadControlLimitDataUpdate(EgLpcUseCase* self, const EventPayload* payload);
static void OnConfigurationDataUpdate(const EgLpcUseCase* self, const EventPayload* payload);
static void OnHeartbeat(const EgLpcUseCase* self, const EventPayload* payload);
static void OnDataChange(EgLpcUseCase* self, const EventPayload* payload);
static void OnEntityAdded(const EgLpcUseCase* self, const EventPayload* payload);
static void OnEntityRemoved(const EgLpcUseCase* self, const EventPayload* payload);

void OnEntityAddedHandleLoadControl(const EgLpcUseCase* self, EntityRemoteObject* entity) {
  const UseCase* const use_case = USE_CASE(self);

  LoadControlClient load_control;
  if (LoadControlClientConstruct(&load_control, use_case->local_entity, entity) != kEebusErrorOk) {
    return;
  }

  FeatureInfoClient* feature_info = &load_control.feature_info_client;
  if (!HasSubscription(feature_info)) {
    Subscribe(feature_info);
  }

  if (!HasBinding(feature_info)) {
    Bind(feature_info);
  }

  // Get descriptions
  const LoadControlLimitDescriptionListDataSelectorsType selectors = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  LoadControlClientRequestLimitDescriptions(&load_control, &selectors, NULL);
}

void OnEntityAddedHandleDeviceConfiguration(const EgLpcUseCase* self, EntityRemoteObject* entity) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationClient device_configuraton;
  const EebusError err = DeviceConfigurationClientConstruct(&device_configuraton, use_case->local_entity, entity);
  if (err != kEebusErrorOk) {
    return;
  }

  FeatureInfoClient* feature_info = &device_configuraton.feature_info_client;
  if (!HasSubscription(feature_info)) {
    Subscribe(feature_info);
  }

  if (!HasBinding(feature_info)) {
    Bind(feature_info);
  }

  // Get descriptions
  // don't use selectors yet, as we would have to query 2 which could result in 2 full reads
  DeviceConfigurationClientRequestKeyValueDescription(&device_configuraton, NULL, NULL);
}

void OnEntityAddedHandleDeviceDiagnosis(const EgLpcUseCase* self, EntityRemoteObject* entity) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceDiagnosisClient device_diagnosis;
  const EebusError err = DeviceDiagnosisClientConstruct(&device_diagnosis, use_case->local_entity, entity);
  if (err != kEebusErrorOk) {
    return;
  }

  FeatureInfoClient* feature_info = &device_diagnosis.feature_info_client;
  if (!HasSubscription(feature_info)) {
    Subscribe(feature_info);
  }

  DeviceDiagnosisClientRequestHeartbeat(&device_diagnosis);
}

void OnEntityAdded(const EgLpcUseCase* self, const EventPayload* payload) {
  EntityRemoteObject* entity = payload->entity;
  if (entity == NULL) {
    return;
  }

  if (!USE_CASE_IS_USE_CASE_COMPATIBLE(USE_CASE_OBJECT(self), payload->use_case_filter)) {
    return;
  }

  // Initialise features, e.g. subscriptions, descriptions
  OnEntityAddedHandleLoadControl(self, entity);
  OnEntityAddedHandleDeviceConfiguration(self, entity);
  OnEntityAddedHandleDeviceDiagnosis(self, entity);

  if (self->eg_lpc_listener != NULL) {
    const EntityAddressType* const entity_addr = ENTITY_GET_ADDRESS(ENTITY_OBJECT(entity));
    EG_LPC_LISTENER_ON_REMOTE_ENTITY_CONNECT(self->eg_lpc_listener, entity_addr);
  }
}

void OnEntityRemoved(const EgLpcUseCase* self, const EventPayload* payload) {
  EntityRemoteObject* entity = payload->entity;
  if (entity == NULL) {
    return;
  }

  if (!USE_CASE_IS_USE_CASE_COMPATIBLE(USE_CASE_OBJECT(self), payload->use_case_filter)) {
    return;
  }

  if (self->eg_lpc_listener != NULL) {
    const EntityAddressType* const entity_addr = ENTITY_GET_ADDRESS(ENTITY_OBJECT(entity));
    EG_LPC_LISTENER_ON_REMOTE_ENTITY_DISCONNECT(self->eg_lpc_listener, entity_addr);
  }
}

void OnLoadControlLimitDescriptionDataUpdate(const EgLpcUseCase* self, const EventPayload* payload) {
  const UseCase* const use_case = USE_CASE(self);

  LoadControlClient lcc;
  if (LoadControlClientConstruct(&lcc, use_case->local_entity, payload->entity) != kEebusErrorOk) {
    return;
  }

  // Get values
  const LoadControlLimitDescriptionDataType filter = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  const LoadControlLimitDescriptionDataType* const description
      = LoadControlCommonGetLimitDescriptionWithFilter(&lcc.load_control_common, &filter);

  if ((description == NULL) || (description->limit_id == NULL)) {
    return;
  }

  const LoadControlLimitListDataSelectorsType selectors = {
      .limit_id = description->limit_id,
  };

  LoadControlClientRequestLimitData(&lcc, &selectors, NULL);
}

void OnLoadControlLimitDataUpdate(EgLpcUseCase* self, const EventPayload* payload) {
  const UseCase* const use_case = USE_CASE(self);

  LoadControlClient load_control;
  if (LoadControlClientConstruct(&load_control, use_case->local_entity, payload->entity) != kEebusErrorOk) {
    return;
  }

  const LoadControlLimitDescriptionDataType filter = {
      .limit_type      = &(LoadControlLimitTypeType){kLoadControlLimitTypeTypeSignDependentAbsValueLimit},
      .limit_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
      .scope_type      = &(ScopeTypeType){kScopeTypeTypeActivePowerLimit},
  };

  if (!LoadControlCommonCheckLimitWithFilter(&load_control.load_control_common, payload->function_data, &filter)) {
    return;
  }

  LoadLimit limit;
  const EntityAddressType* entity_addr = ENTITY_GET_ADDRESS(ENTITY_OBJECT(payload->entity));

  EebusError ret = EgLpcGetActivePowerConsumptionLimitInternal(self, entity_addr, &limit);
  if (ret == kEebusErrorOk) {
    EG_LPC_LISTENER_ON_POWER_LIMIT_RECEIVE(self->eg_lpc_listener, &limit.value, &limit.duration, limit.is_active);
  }
}

void OnConfigurationDescriptionDataUpdate(const EgLpcUseCase* self, const EventPayload* payload) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationClient dcc;
  if (DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, payload->entity) != kEebusErrorOk) {
    return;
  }

  // Key value descriptions received, now get the data
  DeviceConfigurationClientRequestKeyValue(&dcc, NULL, NULL);
}

void OnConfigurationDataUpdate(const EgLpcUseCase* self, const EventPayload* payload) {
  const UseCase* const use_case = USE_CASE(self);

  DeviceConfigurationClient dcc;
  if (DeviceConfigurationClientConstruct(&dcc, use_case->local_entity, payload->entity) != kEebusErrorOk) {
    return;
  }

  DeviceConfigurationKeyValueDescriptionDataType filter = {
      .key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeConsumptionActivePowerLimit},
  };

  if (self->eg_lpc_listener == NULL) {
    return;
  }

  const DeviceConfigurationKeyValueListDataType* const key_value_list = payload->function_data;
  if (DeviceConfigurationCommonCheckKeyValueWithFilter(&dcc.device_cfg_common, key_value_list, &filter)) {
    const EntityAddressType* const entity_addr = ENTITY_GET_ADDRESS(ENTITY_OBJECT(payload->entity));

    ScaledValue power_limit = {0};
    const EebusError err    = EgLpcGetFailsafeConsumptionActivePowerLimitInternal(self, entity_addr, &power_limit);
    if (err == kEebusErrorOk) {
      EG_LPC_LISTENER_ON_FAILSAFE_POWER_LIMIT_RECEIVE(self->eg_lpc_listener, &power_limit);
    }
  }

  filter.key_name = &(DeviceConfigurationKeyNameType){kDeviceConfigurationKeyNameTypeFailsafeDurationMinimum};

  if (DeviceConfigurationCommonCheckKeyValueWithFilter(&dcc.device_cfg_common, key_value_list, &filter)) {
    const EntityAddressType* const entity_addr = ENTITY_GET_ADDRESS(ENTITY_OBJECT(payload->entity));

    DurationType duration = {0};
    const EebusError err  = EgLpcGetFailsafeDurationMinimumInternal(self, entity_addr, &duration);

    if (err == kEebusErrorOk) {
      EG_LPC_LISTENER_ON_FAILSAFE_DURATION_RECEIVE(self->eg_lpc_listener, &duration);
    }
  }
}

void OnHeartbeat(const EgLpcUseCase* self, const EventPayload* payload) {
  if ((payload->cmd_classifier == NULL) && (*payload->cmd_classifier == kCommandClassifierTypeNotify)) {
    return;
  }

  const DeviceDiagnosisHeartbeatDataType* const data = payload->function_data;
  if ((data == NULL) || (data->heartbeat_counter == NULL)) {
    return;
  }

  if (self->eg_lpc_listener != NULL) {
    EG_LPC_LISTENER_ON_HEARTBEAT_RECEIVE(self->eg_lpc_listener, *data->heartbeat_counter);
  }
}

void OnDataChange(EgLpcUseCase* self, const EventPayload* payload) {
  switch (payload->function_type) {
    case kFunctionTypeLoadControlLimitDescriptionListData:
      OnLoadControlLimitDescriptionDataUpdate(self, payload);
      break;

    case kFunctionTypeLoadControlLimitListData: OnLoadControlLimitDataUpdate(self, payload); break;

    case kFunctionTypeDeviceConfigurationKeyValueDescriptionListData:
      OnConfigurationDescriptionDataUpdate(self, payload);
      break;

    case kFunctionTypeDeviceConfigurationKeyValueListData: OnConfigurationDataUpdate(self, payload); break;

    case kFunctionTypeDeviceDiagnosisHeartbeatData: OnHeartbeat(self, payload); break;

    default: break;
  }
}

void EgLpcHandleEvent(const EventPayload* payload, void* ctx) {
  EgLpcUseCase* eg_lpc_use_case = (EgLpcUseCase*)ctx;

  if (!USE_CASE_IS_ENTITY_COMPATIBLE(USE_CASE_OBJECT(eg_lpc_use_case), payload->entity)) {
    return;
  }

  if (payload->event_type == kEventTypeUseCaseChange) {
    if (payload->change_type == kElementChangeAdd) {
      OnEntityAdded(eg_lpc_use_case, payload);
    } else if (payload->change_type == kElementChangeRemove) {
      OnEntityRemoved(eg_lpc_use_case, payload);
    }
  } else if ((payload->event_type == kEventTypeDataChange) || (payload->change_type == kElementChangeUpdate)) {
    OnDataChange(eg_lpc_use_case, payload);
  }
}
