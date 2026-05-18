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
 * @brief MA MGCP Measurement implementation
 */

#include "src/common/array_util.h"
#include "src/use_case/api/ma_mgcp_measurement_interface.h"
#include "src/use_case/model/mgcp_types.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_client.h"
#include "src/use_case/specialization/measurement/measurement_client.h"

typedef struct MaMgcpMeasurement MaMgcpMeasurement;

struct MaMgcpMeasurement {
  /** Implements the MA MGCP Measurement Interface */
  MaMgcpMeasurementObject obj;

  GcpMeasurementNameId name;
  MeasurementTypeType measurement_type;
  ScopeTypeType scope;
  /** NULL means "don't care" (matches any phase) */
  const ElectricalConnectionPhaseNameType* phases;
  /** NULL means "don't care" */
  const ElectricalConnectionPhaseNameType* in_reference_to;

  EebusError (*get_measurement_strategy)(
      const MaMgcpMeasurement* measurement,
      MeasurementClient* mcl,
      ElectricalConnectionClient* eccl,
      ScaledValue* value
  );
};

#define MA_MGCP_MEASUREMENT(obj) ((MaMgcpMeasurement*)(obj))

static GcpMeasurementNameId GetName(const MaMgcpMeasurementObject* self);
static EebusError GetDataValue(
    const MaMgcpMeasurementObject* self,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* measurement_value
);

static const MaMgcpMeasurementInterface ma_mgcp_measurement_methods = {
    .get_name       = GetName,
    .get_data_value = GetDataValue,
};

static EebusError GetPowerStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetEnergyStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetCurrentStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetVoltageStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetFrequencyStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);

#define MA_MGCP_MEASUREMENT_POWER_TOTAL                                         \
  {                                                                             \
      .obj                      = {.interface_ = &ma_mgcp_measurement_methods}, \
      .name                     = kGcpPowerTotal,                               \
      .measurement_type         = kMeasurementTypeTypePower,                    \
      .scope                    = kScopeTypeTypeACPowerTotal,                   \
      .phases                   = NULL,                                         \
      .in_reference_to          = NULL,                                         \
      .get_measurement_strategy = GetPowerStrategy,                             \
  }

#define MA_MGCP_MEASUREMENT_ENERGY(name_id, energy_scope)                       \
  {                                                                             \
      .obj                      = {.interface_ = &ma_mgcp_measurement_methods}, \
      .name                     = name_id,                                      \
      .measurement_type         = kMeasurementTypeTypeEnergy,                   \
      .scope                    = energy_scope,                                 \
      .phases                   = NULL,                                         \
      .in_reference_to          = NULL,                                         \
      .get_measurement_strategy = GetEnergyStrategy,                            \
  }

#define MA_MGCP_MEASUREMENT_CURRENT(name_id, phase)                                                                \
  {                                                                                                                \
      .obj                      = {.interface_ = &ma_mgcp_measurement_methods},                                    \
      .name                     = name_id,                                                                         \
      .measurement_type         = kMeasurementTypeTypeCurrent,                                                     \
      .scope                    = kScopeTypeTypeACCurrent,                                                         \
      .phases                   = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##phase}, \
      .in_reference_to          = NULL,                                                                            \
      .get_measurement_strategy = GetCurrentStrategy,                                                              \
  }

#define MA_MGCP_MEASUREMENT_VOLTAGE(name_id, phase, ref_phase)                                                         \
  {                                                                                                                    \
      .obj                      = {.interface_ = &ma_mgcp_measurement_methods},                                        \
      .name                     = name_id,                                                                             \
      .measurement_type         = kMeasurementTypeTypeVoltage,                                                         \
      .scope                    = kScopeTypeTypeACVoltage,                                                             \
      .phases                   = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##phase},     \
      .in_reference_to          = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##ref_phase}, \
      .get_measurement_strategy = GetVoltageStrategy,                                                                  \
  }

#define MA_MGCP_MEASUREMENT_FREQUENCY                                           \
  {                                                                             \
      .obj                      = {.interface_ = &ma_mgcp_measurement_methods}, \
      .name                     = kGcpFrequency,                                \
      .measurement_type         = kMeasurementTypeTypeFrequency,                \
      .scope                    = kScopeTypeTypeACFrequency,                    \
      .phases                   = NULL,                                         \
      .in_reference_to          = NULL,                                         \
      .get_measurement_strategy = GetFrequencyStrategy,                         \
  }

static const MaMgcpMeasurement measurement_table[] = {
    /* Scenario 2 */
    MA_MGCP_MEASUREMENT_POWER_TOTAL,
    /* Scenario 3 */
    MA_MGCP_MEASUREMENT_ENERGY(kGcpEnergyFeedIn, kScopeTypeTypeGridFeedIn),
    /* Scenario 4 */
    MA_MGCP_MEASUREMENT_ENERGY(kGcpEnergyConsumed, kScopeTypeTypeGridConsumption),
    /* Scenario 5 */
    MA_MGCP_MEASUREMENT_CURRENT(kGcpCurrentPhaseA, A),
    MA_MGCP_MEASUREMENT_CURRENT(kGcpCurrentPhaseB, B),
    MA_MGCP_MEASUREMENT_CURRENT(kGcpCurrentPhaseC, C),
    /* Scenario 6 */
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseA, A, Neutral),
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseB, B, Neutral),
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseC, C, Neutral),
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseAb, A, B),
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseBc, B, C),
    MA_MGCP_MEASUREMENT_VOLTAGE(kGcpVoltagePhaseAc, C, A),
    /* Scenario 7 */
    MA_MGCP_MEASUREMENT_FREQUENCY,
};

const MaMgcpMeasurementObject* MaMgcpMeasurementGetInstanceWithNameId(GcpMeasurementNameId name) {
  for (size_t i = 0; i < ARRAY_SIZE(measurement_table); ++i) {
    if (measurement_table[i].name == name) {
      return MA_MGCP_MEASUREMENT_OBJECT(&measurement_table[i]);
    }
  }
  return NULL;
}

static bool MaMgcpMeasurementPhasesMatch(
    const MaMgcpMeasurement* measurement,
    const ElectricalConnectionPhaseNameType* phases,
    const ElectricalConnectionPhaseNameType* in_reference_to
) {
  if (measurement->phases != NULL) {
    if ((phases == NULL) || (*measurement->phases != *phases)) {
      return false;
    }
  }
  if (measurement->in_reference_to != NULL) {
    if ((in_reference_to == NULL) || (*measurement->in_reference_to != *in_reference_to)) {
      return false;
    }
  }
  return true;
}

static bool MaMgcpMeasurementMatchesTypeAndScopeAndPhases(
    const MaMgcpMeasurement* measurement,
    MeasurementTypeType measurement_type,
    ScopeTypeType scope,
    const ElectricalConnectionPhaseNameType* phases,
    const ElectricalConnectionPhaseNameType* in_reference_to
) {
  if ((measurement->measurement_type != measurement_type) || (measurement->scope != scope)) {
    return false;
  }
  return MaMgcpMeasurementPhasesMatch(measurement, phases, in_reference_to);
}

static EebusError GetPhasesWithMeasurementId(
    const ElectricalConnectionClient* eccl,
    MeasurementIdType measurement_id,
    const ElectricalConnectionPhaseNameType** phases,
    const ElectricalConnectionPhaseNameType** in_reference_to
) {
  if ((phases == NULL) && (in_reference_to == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  const ElectricalConnectionParameterDescriptionDataType filter = {
      .measurement_id = &measurement_id,
  };

  const ElectricalConnectionParameterDescriptionDataType* const parameter_description
      = ElectricalConnectionCommonGetParameterDescriptionWithFilter(&eccl->el_connection_common, &filter);

  if (parameter_description == NULL) {
    return kEebusErrorNotAvailable;
  }

  *phases          = parameter_description->ac_measured_phases;
  *in_reference_to = parameter_description->ac_measured_in_reference_to;
  return kEebusErrorOk;
}

const MaMgcpMeasurementObject* MaMgcpMeasurementGetInstance(
    const MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    const MeasurementDataType* measurement_data
) {
  if ((measurement_data == NULL) || (measurement_data->measurement_id == NULL)) {
    return NULL;
  }

  const MeasurementDescriptionDataType* const description
      = MeasurementCommonGetMeasurementDescriptionWithId(&mcl->measurement_common, *measurement_data->measurement_id);

  if ((description == NULL) || (description->measurement_type == NULL) || (description->scope_type == NULL)) {
    return NULL;
  }

  const MeasurementTypeType measurement_type = *description->measurement_type;
  const ScopeTypeType scope                  = *description->scope_type;

  const ElectricalConnectionPhaseNameType* phases          = NULL;
  const ElectricalConnectionPhaseNameType* in_reference_to = NULL;
  if (GetPhasesWithMeasurementId(eccl, *measurement_data->measurement_id, &phases, &in_reference_to) != kEebusErrorOk) {
    return NULL;
  }

  for (size_t i = 0; i < ARRAY_SIZE(measurement_table); ++i) {
    if (MaMgcpMeasurementMatchesTypeAndScopeAndPhases(
            &measurement_table[i],
            measurement_type,
            scope,
            phases,
            in_reference_to
        )) {
      return MA_MGCP_MEASUREMENT_OBJECT(&measurement_table[i]);
    }
  }

  return NULL;
}

static bool CheckPhaseSpecificData(
    const MaMgcpMeasurement* measurement,
    const ElectricalConnectionClient* eccl,
    const EnergyDirectionType* energy_direction,
    const MeasurementDataType* item
) {
  if ((item->value == NULL) || (item->value->number == NULL) || (item->measurement_id == NULL)) {
    return false;
  }

  if (measurement->phases != NULL) {
    const ElectricalConnectionPhaseNameType* phases          = NULL;
    const ElectricalConnectionPhaseNameType* in_reference_to = NULL;
    if (GetPhasesWithMeasurementId(eccl, *item->measurement_id, &phases, &in_reference_to) != kEebusErrorOk) {
      return false;
    }
    if (!MaMgcpMeasurementPhasesMatch(measurement, phases, in_reference_to)) {
      return false;
    }
  }

  if (energy_direction != NULL) {
    const ElectricalConnectionParameterDescriptionDataType filter = {
        .measurement_id = item->measurement_id,
    };
    const ElectricalConnectionDescriptionDataType* const description
        = ElectricalConnectionCommonGetDescriptionWithParameterDescriptionFilter(&eccl->el_connection_common, &filter);
    if (description == NULL) {
      return false;
    }
    if ((description->positive_energy_direction == NULL)
        || (*description->positive_energy_direction != *energy_direction)) {
      return false;
    }
  }

  if ((item->value_state != NULL) && (*item->value_state != kMeasurementValueStateTypeNormal)) {
    return false;
  }

  return true;
}

static EebusError GetPhaseSpecificData(
    const MaMgcpMeasurement* measurement,
    const MeasurementClient* mcl,
    const ElectricalConnectionClient* eccl,
    const EnergyDirectionType* energy_direction,
    ScaledValue* value
) {
  const MeasurementDescriptionDataType filter = {
      .measurement_type = &measurement->measurement_type,
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .scope_type       = &measurement->scope,
  };

  EebusDataListMatchIterator it = {0};
  MeasurementCommonGetMeasurementDescriptionMatchFirst(&mcl->measurement_common, &filter, &it);

  for (; !EebusDataListMatchIteratorIsDone(&it); EebusDataListMatchIteratorNext(&it)) {
    const MeasurementDescriptionDataType* const description = EebusDataListMatchIteratorGet(&it);

    const MeasurementDataType filter2 = {
        .measurement_id = description->measurement_id,
    };

    const MeasurementListDataType* const measurement_list = MeasurementCommonGetMeasurements(&mcl->measurement_common);

    EebusDataListMatchIterator it2 = {0};
    HelperListMatchFirst(kFunctionTypeMeasurementListData, measurement_list, &filter2, &it2);

    for (; !EebusDataListMatchIteratorIsDone(&it2); EebusDataListMatchIteratorNext(&it2)) {
      const MeasurementDataType* const data = EebusDataListMatchIteratorGet(&it2);
      if (CheckPhaseSpecificData(measurement, eccl, energy_direction, data)) {
        return ScaledValueInitWithScaledNumber(value, data->value);
      }
    }
  }

  return kEebusErrorNotAvailable;
}

static EebusError GetPowerStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  // Grid Connection Point always uses consume as positive direction
  static const EnergyDirectionType energy_direction = kEnergyDirectionTypeConsume;
  return GetPhaseSpecificData(measurement, mcl, eccl, &energy_direction, value);
}

static EebusError GetEnergyStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  (void)eccl;

  const MeasurementDescriptionDataType filter = {
      .measurement_type = &measurement->measurement_type,
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .scope_type       = &measurement->scope,
  };

  const MeasurementDataType* const measurement_data
      = MeasurementCommonGetMeasurementWithFilter(&mcl->measurement_common, &filter);
  if (measurement_data == NULL) {
    return kEebusErrorNotAvailable;
  }

  if ((measurement_data->value_state != NULL) && (*measurement_data->value_state != kMeasurementValueStateTypeNormal)) {
    return kEebusErrorInvalid;
  }

  return ScaledValueInitWithScaledNumber(value, measurement_data->value);
}

static EebusError GetCurrentStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  static const EnergyDirectionType energy_direction = kEnergyDirectionTypeConsume;
  return GetPhaseSpecificData(measurement, mcl, eccl, &energy_direction, value);
}

static EebusError GetVoltageStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  // Voltage has no energy direction filter
  return GetPhaseSpecificData(measurement, mcl, eccl, NULL, value);
}

static EebusError GetFrequencyStrategy(
    const MaMgcpMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  (void)measurement;
  (void)eccl;

  const MeasurementDescriptionDataType filter = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeFrequency},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .scope_type       = &(ScopeTypeType){kScopeTypeTypeACFrequency},
  };

  const MeasurementDataType* const measurement_data
      = MeasurementCommonGetMeasurementWithFilter(&mcl->measurement_common, &filter);
  if (measurement_data == NULL) {
    return kEebusErrorNotAvailable;
  }

  if ((measurement_data->value_state != NULL) && (*measurement_data->value_state != kMeasurementValueStateTypeNormal)) {
    return kEebusErrorInvalid;
  }

  return ScaledValueInitWithScaledNumber(value, measurement_data->value);
}

static GcpMeasurementNameId GetName(const MaMgcpMeasurementObject* self) {
  return MA_MGCP_MEASUREMENT(self)->name;
}

static EebusError GetDataValue(
    const MaMgcpMeasurementObject* self,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* measurement_value
) {
  const MaMgcpMeasurement* const measurement = MA_MGCP_MEASUREMENT(self);
  return measurement->get_measurement_strategy(measurement, mcl, eccl, measurement_value);
}
