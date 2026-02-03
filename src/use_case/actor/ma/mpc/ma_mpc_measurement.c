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
 * @brief Ma Mpc Measurement implementation
 */

#include "src/common/array_util.h"
#include "src/use_case/api/ma_mpc_measurement_interface.h"
#include "src/use_case/model/mpc_types.h"
#include "src/use_case/specialization/electrical_connection/electrical_connection_client.h"
#include "src/use_case/specialization/measurement/measurement_client.h"

typedef struct MaMpcMeasurement MaMpcMeasurement;

/**
 * @brief MA MPC Measurement is measurement info (including id, phases, value source and constraints)
 * for the MA MPC use case scenarios 1-5
 */
typedef struct MaMpcMeasurement MaMpcMeasurement;

/**
 * @brief MA MPC Measurement Read Strategy
 */
typedef EebusError (*GetMeasurementDataStrategy)(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);

struct MaMpcMeasurement {
  /** Implements the MA MPC Measurement Interface */
  MaMpcMeasurementObject obj;

  MuMpcMeasurementNameId name;
  /** Measurement Type */
  MeasurementTypeType measurement_type;
  /** Measurement Object (total or per phase) */
  ScopeTypeType scope;
  /** Measured phases for total or single one for per phase */
  const ElectricalConnectionPhaseNameType* phases;
  /** In case of per phase measurement, the phase the measurement refers to */
  const ElectricalConnectionPhaseNameType* in_reference_to;
  /** The strategy to configure the measurement scope */
  GetMeasurementDataStrategy get_measurement_strategy;
};

#define MA_MPC_MEASUREMENT(obj) ((MaMpcMeasurement*)(obj))

static MuMpcMeasurementNameId GetName(const MaMpcMeasurementObject* self);
static EebusError GetDataValue(
    const MaMpcMeasurementObject* self,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* measurement_value
);

static const MaMpcMeasurementInterface ma_mpc_measurement_scope_methods = {
    .get_name       = GetName,
    .get_data_value = GetDataValue,
};

static EebusError GetPowerStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetCurrentStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetEnergyStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetVoltageStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);
static EebusError GetFrequencyStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
);

#define MA_MPC_MEASUREMENT_POWER_TOTAL                                               \
  {                                                                                  \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods}, \
      .name                     = kMpcPowerTotal,                                    \
      .measurement_type         = kMeasurementTypeTypePower,                         \
      .scope                    = kScopeTypeTypeACPowerTotal,                        \
      .phases                   = NULL,                                              \
      .in_reference_to          = NULL,                                              \
      .get_measurement_strategy = GetPowerStrategy,                                  \
  }

#define MA_MPC_MEASUREMENT_POWER(name_id, phase)                                                                   \
  {                                                                                                                \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods},                               \
      .name                     = name_id,                                                                         \
      .measurement_type         = kMeasurementTypeTypePower,                                                       \
      .scope                    = kScopeTypeTypeACPower,                                                           \
      .phases                   = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##phase}, \
      .in_reference_to          = NULL,                                                                            \
      .get_measurement_strategy = GetPowerStrategy,                                                                \
  }

#define MA_MPC_MEASUREMENT_ENERGY(name_id, energy_scope)                             \
  {                                                                                  \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods}, \
      .name                     = name_id,                                           \
      .measurement_type         = kMeasurementTypeTypeEnergy,                        \
      .scope                    = energy_scope,                                      \
      .phases                   = NULL,                                              \
      .in_reference_to          = NULL,                                              \
      .get_measurement_strategy = GetEnergyStrategy,                                 \
  }

#define MA_MPC_MEASUREMENT_CURRENT(name_id, phase)                                                                 \
  {                                                                                                                \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods},                               \
      .name                     = name_id,                                                                         \
      .measurement_type         = kMeasurementTypeTypeCurrent,                                                     \
      .scope                    = kScopeTypeTypeACCurrent,                                                         \
      .phases                   = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##phase}, \
      .in_reference_to          = NULL,                                                                            \
      .get_measurement_strategy = GetCurrentStrategy,                                                              \
  }

#define MA_MPC_MEASUREMENT_VOLTAGE(name_id, phase, ref_phase)                                                          \
  {                                                                                                                    \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods},                                   \
      .name                     = name_id,                                                                             \
      .measurement_type         = kMeasurementTypeTypeVoltage,                                                         \
      .scope                    = kScopeTypeTypeACVoltage,                                                             \
      .phases                   = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##phase},     \
      .in_reference_to          = &(ElectricalConnectionPhaseNameType){kElectricalConnectionPhaseNameType##ref_phase}, \
      .get_measurement_strategy = GetVoltageStrategy,                                                                  \
  }

#define MA_MPC_MEASUREMENT_FREQUENCY                                                 \
  {                                                                                  \
      .obj                      = {.interface_ = &ma_mpc_measurement_scope_methods}, \
      .name                     = kMpcFrequency,                                     \
      .measurement_type         = kMeasurementTypeTypeFrequency,                     \
      .scope                    = kScopeTypeTypeACFrequency,                         \
      .phases                   = NULL,                                              \
      .get_measurement_strategy = GetFrequencyStrategy,                              \
  }

static const MaMpcMeasurement measurement_table[] = {
    MA_MPC_MEASUREMENT_POWER_TOTAL,
    MA_MPC_MEASUREMENT_POWER(kMpcPowerPhaseA, A),
    MA_MPC_MEASUREMENT_POWER(kMpcPowerPhaseB, B),
    MA_MPC_MEASUREMENT_POWER(kMpcPowerPhaseC, C),
    MA_MPC_MEASUREMENT_ENERGY(kMpcEnergyConsumed, kScopeTypeTypeACEnergyConsumed),
    MA_MPC_MEASUREMENT_ENERGY(kMpcEnergyProduced, kScopeTypeTypeACEnergyProduced),
    MA_MPC_MEASUREMENT_CURRENT(kMpcCurrentPhaseA, A),
    MA_MPC_MEASUREMENT_CURRENT(kMpcCurrentPhaseB, B),
    MA_MPC_MEASUREMENT_CURRENT(kMpcCurrentPhaseC, C),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseA, A, Neutral),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseB, B, Neutral),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseC, C, Neutral),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseAb, A, B),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseBc, B, C),
    MA_MPC_MEASUREMENT_VOLTAGE(kMpcVoltagePhaseAc, C, A),
    MA_MPC_MEASUREMENT_FREQUENCY,
};

const MaMpcMeasurementObject* MaMpcMeasurementGetInstanceWithNameId(MuMpcMeasurementNameId name) {
  for (size_t i = 0; i < ARRAY_SIZE(measurement_table); ++i) {
    if (measurement_table[i].name == name) {
      return MA_MPC_MEASUREMENT_OBJECT(&measurement_table[i]);
    }
  }

  return NULL;
}

bool MaMpcMeasurementPhasesMatch(
    const MaMpcMeasurement* measurement,
    const ElectricalConnectionPhaseNameType* phases,
    const ElectricalConnectionPhaseNameType* in_reference_to
) {
  // Note: NULL phases in measurement means "don't care"
  if (measurement->phases != NULL) {
    if (phases == NULL || (*measurement->phases != *phases)) {
      return false;
    }
  }

  // Note: NULL in_reference_to in measurement means "don't care"
  if (measurement->in_reference_to != NULL) {
    if (in_reference_to == NULL || (*measurement->in_reference_to != *in_reference_to)) {
      return false;
    }
  }

  return true;
}

bool MaMpcMeasurementMatchesTypeAndScopeAndPhases(
    const MaMpcMeasurement* measurement,
    MeasurementTypeType measurement_type,
    ScopeTypeType scope,
    const ElectricalConnectionPhaseNameType* phases,
    const ElectricalConnectionPhaseNameType* in_reference_to
) {
  if ((measurement->measurement_type != measurement_type) || (measurement->scope != scope)) {
    return false;
  }

  return MaMpcMeasurementPhasesMatch(measurement, phases, in_reference_to);
}

EebusError GetPhasesWithMeasurementId(
    const ElectricalConnectionClient* eccl,
    MeasurementIdType measurement_id,
    const ElectricalConnectionPhaseNameType** const phases,
    const ElectricalConnectionPhaseNameType** const in_reference_to
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

const MaMpcMeasurement* MaMpcMeasurementGetInstance(
    const MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    const MeasurementDataType* measurement_data
) {
  if ((measurement_data == NULL) || (measurement_data->measurement_id == NULL)) {
    return NULL;
  }

  const MeasurementDescriptionDataType* description
      = MeasurementCommonGetMeasurementDescriptionWithId(&mcl->measurement_common, *measurement_data->measurement_id);

  if ((description == NULL) || (description->measurement_type == NULL) || (description->scope_type == NULL)) {
    return NULL;
  }

  const MeasurementTypeType measurement_type = *description->measurement_type;

  const ScopeTypeType scope = *description->scope_type;

  const ElectricalConnectionPhaseNameType* phases          = NULL;
  const ElectricalConnectionPhaseNameType* in_reference_to = NULL;

  EebusError error = GetPhasesWithMeasurementId(eccl, *measurement_data->measurement_id, &phases, &in_reference_to);
  if (error != kEebusErrorOk) {
    return NULL;
  }

  for (size_t i = 0; i < ARRAY_SIZE(measurement_table); ++i) {
    if (MaMpcMeasurementMatchesTypeAndScopeAndPhases(
            &measurement_table[i],
            measurement_type,
            scope,
            phases,
            in_reference_to
        )) {
      return &measurement_table[i];
    }
  }

  return NULL;
}

bool CheckPhaseSpecificData(
    const MaMpcMeasurement* measurement,
    const MeasurementClient* mcl,
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

    if (!MaMpcMeasurementPhasesMatch(measurement, phases, in_reference_to)) {
      return false;
    }
  }

  if (energy_direction != NULL) {
    const ElectricalConnectionParameterDescriptionDataType filter = {
        .measurement_id = item->measurement_id,
    };

    const ElectricalConnectionDescriptionDataType* description
        = ElectricalConnectionCommonGetDescriptionWithParameterDescriptionFilter(&eccl->el_connection_common, &filter);

    if (description == NULL) {
      return false;
    }

    // If energy direction doesn't match
    if ((description->positive_energy_direction == NULL)
        || (*description->positive_energy_direction != *energy_direction)) {
      return false;
    }
  }

  // If the value state is set and not normal, the value is not valid and should be ignored
  // therefore we return an error
  if ((item->value_state != NULL) && (*item->value_state != kMeasurementValueStateTypeNormal)) {
    return false;
  }

  return true;
}

EebusError GetPhaseSpecificData(
    const MaMpcMeasurement* measurement,
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
    const MeasurementDescriptionDataType* description = EebusDataListMatchIteratorGet(&it);

    const MeasurementDataType filter2 = {
        .measurement_id = description->measurement_id,
    };

    const MeasurementListDataType* const measurement_list = MeasurementCommonGetMeasurements(&mcl->measurement_common);

    EebusDataListMatchIterator it2 = {0};
    HelperListMatchFirst(kFunctionTypeMeasurementListData, measurement_list, &filter2, &it2);

    for (; !EebusDataListMatchIteratorIsDone(&it2); EebusDataListMatchIteratorNext(&it2)) {
      const MeasurementDataType* const measurement_data = EebusDataListMatchIteratorGet(&it2);
      if (CheckPhaseSpecificData(measurement, mcl, eccl, energy_direction, measurement_data)) {
        return ScaledValueInitWithScaledNumber(value, measurement_data->value);
      }
    }
  }

  return kEebusErrorNotAvailable;
}

EebusError GetPowerStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  static const EnergyDirectionType energy_direction = kEnergyDirectionTypeConsume;
  return GetPhaseSpecificData(measurement, mcl, eccl, &energy_direction, value);
}

EebusError GetCurrentStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  static const EnergyDirectionType energy_direction = kEnergyDirectionTypeConsume;
  return GetPhaseSpecificData(measurement, mcl, eccl, &energy_direction, value);
}

EebusError GetEnergyStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  const MeasurementDescriptionDataType filter = {
      .measurement_type = &measurement->measurement_type,
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .scope_type       = &measurement->scope,
  };

  // Assume there is only one unique result
  const MeasurementDataType* const measurement_data
      = MeasurementCommonGetMeasurementWithFilter(&mcl->measurement_common, &filter);
  if (measurement_data == NULL) {
    return kEebusErrorNotAvailable;
  }

  // If the value state is set and not normal, the value is not valid and should be ignored
  // therefore we return an error
  if ((measurement_data->value_state != NULL) && (*measurement_data->value_state != kMeasurementValueStateTypeNormal)) {
    return kEebusErrorInvalid;
  }

  return ScaledValueInitWithScaledNumber(value, measurement_data->value);
}

EebusError GetVoltageStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
  return GetPhaseSpecificData(measurement, mcl, eccl, NULL, value);
}

EebusError GetFrequencyStrategy(
    const MaMpcMeasurement* measurement,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* value
) {
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

  // If the value state is set and not normal,
  // the value is not valid and should be ignored therefore an error is returned
  if ((measurement_data->value_state != NULL) && (*measurement_data->value_state != kMeasurementValueStateTypeNormal)) {
    return kEebusErrorInvalid;
  }

  return ScaledValueInitWithScaledNumber(value, measurement_data->value);
}

MuMpcMeasurementNameId GetName(const MaMpcMeasurementObject* self) {
  const MaMpcMeasurement* const measurement = MA_MPC_MEASUREMENT(self);
  return measurement->name;
}

EebusError GetDataValue(
    const MaMpcMeasurementObject* self,
    MeasurementClient* mcl,
    ElectricalConnectionClient* eccl,
    ScaledValue* measurement_value
) {
  const MaMpcMeasurement* const measurement = MA_MPC_MEASUREMENT(self);

  return measurement->get_measurement_strategy(measurement, mcl, eccl, measurement_value);
}
