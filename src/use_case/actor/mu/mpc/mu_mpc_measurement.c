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
#include <string.h>

#include "src/common/array_util.h"
#include "src/spine/model/absolute_or_relative_time.h"
#include "src/spine/model/measurement_types.h"
#include "src/use_case/actor/mu/mpc/mu_mpc_measurement.h"

/**
 * @brief MU MPC Measurement is measurement info (including id, phases, value source and constraints)
 * for the MU MPC use case scenarios 1-5
 */
typedef struct MuMpcMeasurement MuMpcMeasurement;

/**
 * @brief MU MPC Measurement Configuration Strategy
 */
typedef EebusError (*MeasurementConfigurationStrategy)(
    MuMpcMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
);

/**
 * @brief MU MPC Measurement structure
 */
struct MuMpcMeasurement {
  /** Implements the MU MPC Measurement Object Interface */
  MuMpcMeasurementObject obj;

  MuMpcMeasurementNameId name;
  /** MuMpcMeasurement Id (required). Shall be obtained while MEASUREMENT_CONFIGURE() exectution */
  MeasurementIdType id;
  /** Measurement Object (total or per phase) */
  ScopeTypeType scope;
  /** Measured phases for total or single one for per phase */
  ElectricalConnectionPhaseNameType phases;
  /** The source of the values (required) */
  MeasurementValueSourceType value_source;
  /** The constraints for the voltage values (optional) */
  MeasurementConstraintsDataType* constraints;
  /** The strategy to configure the measurement scope */
  MeasurementConfigurationStrategy cfg_strategy;
  /** Cache for the measurement data */
  MeasurementDataType* measurement_data;
};

#define MEASUREMENT(obj) ((MuMpcMeasurement*)(obj))

static void Destruct(MuMpcMeasurementObject* self);
static MuMpcMeasurementNameId GetName(const MuMpcMeasurementObject* self);
static EebusError
GetDataValue(const MuMpcMeasurementObject* self, MeasurementServer* msrv, ScaledValue* measurement_value);
static const MeasurementConstraintsDataType* GetConstraints(const MuMpcMeasurementObject* self);
static EebusError Configure(
    MuMpcMeasurementObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
);
static EebusError SetDataCache(
    MuMpcMeasurementObject* self,
    const ScaledValue* measured_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
);
static MeasurementDataType* ReleaseDataCache(MuMpcMeasurementObject* self);

static const MuMpcMeasurementInterface measurement_scope_methods = {
    .destruct           = Destruct,
    .get_name           = GetName,
    .get_data_value     = GetDataValue,
    .get_constraints    = GetConstraints,
    .configure          = Configure,
    .set_data_cache     = SetDataCache,
    .release_data_cache = ReleaseDataCache,
};

static EebusError MuMpcMeasurementConstruct(
    MuMpcMeasurement* self,
    MuMpcMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const MuMpcMeasurementConfig* cfg,
    MeasurementConfigurationStrategy cfg_strategy
);

EebusError MuMpcMeasurementConstruct(
    MuMpcMeasurement* self,
    MuMpcMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const MuMpcMeasurementConfig* cfg,
    MeasurementConfigurationStrategy cfg_strategy
) {
  // Override "virtual functions table"
  MU_MPC_MEASUREMENT_INTERFACE(self) = &measurement_scope_methods;

  self->name             = name;
  self->id               = 0;
  self->scope            = scope;
  self->phases           = phases;
  self->value_source     = 0;
  self->constraints      = NULL;
  self->cfg_strategy     = cfg_strategy;
  self->measurement_data = NULL;

  if (cfg == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  if (cfg->constraints != NULL) {
    self->constraints = MeasurementConstraintsDataCopy(cfg->constraints);
    if (self->constraints == NULL) {
      return kEebusErrorMemoryAllocate;
    }
  }

  return kEebusErrorOk;
}

MuMpcMeasurementObject* MuMpcMeasurementCreateInternal(
    MuMpcMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const MuMpcMeasurementConfig* cfg,
    MeasurementConfigurationStrategy cfg_strategy
) {
  MuMpcMeasurement* const measurement = (MuMpcMeasurement*)EEBUS_MALLOC(sizeof(MuMpcMeasurement));
  if (measurement == NULL) {
    return NULL;
  }

  EebusError err = MuMpcMeasurementConstruct(measurement, name, scope, phases, cfg, cfg_strategy);
  if (err != kEebusErrorOk) {
    MuMpcMeasurementDelete(MU_MPC_MEASUREMENT_OBJECT(measurement));
    return NULL;
  }

  return MU_MPC_MEASUREMENT_OBJECT(measurement);
}

void Destruct(MuMpcMeasurementObject* self) {
  MuMpcMeasurement* const measurement = MEASUREMENT(self);

  MeasurementConstraintsDataDelete(measurement->constraints);
  measurement->constraints = NULL;

  MeasurementDataDelete(measurement->measurement_data);
  measurement->measurement_data = NULL;
}

MuMpcMeasurementNameId GetName(const MuMpcMeasurementObject* self) {
  return MEASUREMENT(self)->name;
}

EebusError GetDataValue(const MuMpcMeasurementObject* self, MeasurementServer* msrv, ScaledValue* measurement_value) {
  MuMpcMeasurement* const measurement = MEASUREMENT(self);

  if ((msrv == NULL) || (measurement_value == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  const MeasurementDataType* data = MeasurementCommonGetMeasurementWithId(&msrv->measurement_common, measurement->id);
  if (data == NULL) {
    return kEebusErrorNoChange;
  }

  return ScaledValueInitWithScaledNumber(measurement_value, data->value);
}

const MeasurementConstraintsDataType* GetConstraints(const MuMpcMeasurementObject* self) {
  return MEASUREMENT(self)->constraints;
}

EebusError Configure(
    MuMpcMeasurementObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  MuMpcMeasurement* const measurement = MEASUREMENT(self);

  if (measurement->cfg_strategy == NULL) {
    return kEebusErrorInit;
  }

  // Call the strategy to configure the measurement scope
  EebusError err = measurement->cfg_strategy(measurement, msrv, ecsrv, electrical_connection_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  // If constraints are configured, set the constraints id
  if (measurement->constraints != NULL) {
    err = MeasurementConstraintsSetId(measurement->constraints, measurement->id);
  }

  return err;
}

EebusError SetDataCache(
    MuMpcMeasurementObject* self,
    const ScaledValue* measured_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  MuMpcMeasurement* const measurement = MEASUREMENT(self);

  if (measured_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  // If measurement data already exists, release it
  MeasurementDataDelete(measurement->measurement_data);

  const AbsoluteOrRelativeTimeType* const start_time_tmp = ABSOLUTE_OR_RELATIVE_TIME_PTR(start_time);
  const AbsoluteOrRelativeTimeType* const end_time_tmp   = ABSOLUTE_OR_RELATIVE_TIME_PTR(end_time);

  const TimePeriodType* const evaluation_period_tmp
      = ((start_time_tmp != NULL) && (end_time_tmp != NULL))
            ? &(TimePeriodType){.start_time = start_time_tmp, .end_time = end_time_tmp}
            : NULL;

  const MeasurementDataType measurement_data_new = {
      .measurement_id    = &measurement->id,
      .value_type        = &(MeasurementValueTypeType){kMeasurementValueTypeTypeValue},
      .timestamp         = ABSOLUTE_OR_RELATIVE_TIME_PTR(timestamp),
      .value             = &(ScaledNumberType){.number = &measured_value->value, .scale = &measured_value->scale},
      .evaluation_period = evaluation_period_tmp,
      .value_source      = &measurement->value_source,
      .value_tendency    = NULL,
      .value_state       = value_state,
  };

  measurement->measurement_data = MeasurementDataCopy(&measurement_data_new);
  if (measurement->measurement_data == NULL) {
    return kEebusErrorMemoryAllocate;
  }

  return kEebusErrorOk;
}

MeasurementDataType* ReleaseDataCache(MuMpcMeasurementObject* self) {
  MuMpcMeasurement* const measurement = MEASUREMENT(self);

  MeasurementDataType* const measurement_data = measurement->measurement_data;

  measurement->measurement_data = NULL;

  return measurement_data;
}

//-------------------------------------------------------------------------------------------//
//
// Power MuMpcMeasurment Object Creation (Scenario 1)
//
//-------------------------------------------------------------------------------------------//
EebusError ConfigureElectricalConnectionDescription(
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const ElectricalConnectionDescriptionDataType description = {
      .power_supply_type         = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .positive_energy_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
  };

  return ElectricalConnectionServerAddDescriptionWithId(ecsrv, &description, electrical_connection_id);
}

EebusError ConfigurePower(
    MuMpcMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const MeasurementDescriptionDataType measurement_description = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypePower},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeW},
      .scope_type       = &measurement->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &measurement_description, &measurement->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  err = ConfigureElectricalConnectionDescription(ecsrv, electrical_connection_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType parameter_description = {
      .electrical_connection_id    = &electrical_connection_id,
      .measurement_id              = &measurement->id,
      .voltage_type                = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases          = &measurement->phases,
      .ac_measured_in_reference_to = &ELECTRICAL_CONNECTION_PHASE_NAME(Neutral),
      .ac_measurement_type         = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
      .ac_measurement_variant      = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType parameter_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &parameter_description, &parameter_id);
}

ElectricalConnectionPhaseNameType GetPowerPhaseWithNameId(MuMpcMeasurementNameId name) {
  switch (name) {
    case kMpcPowerPhaseA: return kElectricalConnectionPhaseNameTypeA;
    case kMpcPowerPhaseB: return kElectricalConnectionPhaseNameTypeB;
    case kMpcPowerPhaseC: return kElectricalConnectionPhaseNameTypeC;
    default: return kElectricalConnectionPhaseNameTypeNone;  // Invalid phase
  }
}

MuMpcMeasurementObject* MuMpcMeasurementPowerCreate(MuMpcMeasurementNameId name, const MuMpcMeasurementConfig* cfg) {
  if (!((uint8_t)name & (uint8_t)kMpcMonitorPower)) {
    return NULL;
  }

  const ScopeTypeType scope = kScopeTypeTypeACPower;

  const ElectricalConnectionPhaseNameType phases = GetPowerPhaseWithNameId(name);
  return MuMpcMeasurementCreateInternal(name, scope, phases, cfg, ConfigurePower);
}

MuMpcMeasurementObject*
MuMpcMeasurementPowerTotalCreate(ElectricalConnectionPhaseNameType phases, const MuMpcMeasurementConfig* cfg) {
  return MuMpcMeasurementCreateInternal(kMpcPowerTotal, kScopeTypeTypeACPowerTotal, phases, cfg, ConfigurePower);
}

//-------------------------------------------------------------------------------------------//
//
// Energy MuMpcMeasurment Object Creation (Scenario 2)
//
//-------------------------------------------------------------------------------------------//
EebusError ConfigureEnergy(
    MuMpcMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const MeasurementDescriptionDataType measurement_description = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeEnergy},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeWh},
      .scope_type       = &measurement->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &measurement_description, &measurement->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType parameter_description = {
      .electrical_connection_id = &electrical_connection_id,
      .measurement_id           = &measurement->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measurement_type      = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
  };

  ElectricalConnectionParameterIdType parameter_description_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &parameter_description, &parameter_description_id);
}

ScopeTypeType GetEnergyScopeType(MuMpcMeasurementNameId name) {
  switch (name) {
    case kMpcEnergyConsumed: return kScopeTypeTypeACEnergyConsumed;
    case kMpcEnergyProduced: return kScopeTypeTypeACEnergyProduced;
    default: return kScopeTypeTypeACEnergy;
  }
}

MuMpcMeasurementObject* MuMpcMeasurementEnergyCreate(MuMpcMeasurementNameId name, const MuMpcMeasurementConfig* cfg) {
  if (!((uint8_t)name & (uint8_t)kMpcMonitorEnergy)) {
    return NULL;
  }

  const ScopeTypeType scope = GetEnergyScopeType(name);

  const ElectricalConnectionPhaseNameType phase = kElectricalConnectionPhaseNameTypeNone;
  return MuMpcMeasurementCreateInternal(name, scope, phase, cfg, ConfigureEnergy);
}

//-------------------------------------------------------------------------------------------//
//
// Current MuMpcMeasurment Object Creation (Scenario 3)
//
//-------------------------------------------------------------------------------------------//
EebusError ConfigureCurrent(
    MuMpcMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const MeasurementDescriptionDataType measurement_description = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeCurrent},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeA},
      .scope_type       = &measurement->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &measurement_description, &measurement->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  err = ConfigureElectricalConnectionDescription(ecsrv, electrical_connection_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType parameter_description = {
      .electrical_connection_id = &electrical_connection_id,
      .measurement_id           = &measurement->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases       = &measurement->phases,
      .ac_measurement_type      = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
      .ac_measurement_variant   = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType parameter_description_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &parameter_description, &parameter_description_id);
}

ElectricalConnectionPhaseNameType GetCurrentPhaseWithNameId(MuMpcMeasurementNameId name) {
  switch (name) {
    case kMpcCurrentPhaseA: return kElectricalConnectionPhaseNameTypeA;
    case kMpcCurrentPhaseB: return kElectricalConnectionPhaseNameTypeB;
    case kMpcCurrentPhaseC: return kElectricalConnectionPhaseNameTypeC;
    default: return kElectricalConnectionPhaseNameTypeNone;  // Invalid phase
  }
}

MuMpcMeasurementObject* MuMpcMeasurementCurrentCreate(MuMpcMeasurementNameId name, const MuMpcMeasurementConfig* cfg) {
  if (!((uint8_t)name & (uint8_t)kMpcMonitorCurrent)) {
    return NULL;
  }

  const ElectricalConnectionPhaseNameType phase = GetCurrentPhaseWithNameId(name);
  return MuMpcMeasurementCreateInternal(name, kScopeTypeTypeACCurrent, phase, cfg, ConfigureCurrent);
}

//-------------------------------------------------------------------------------------------//
//
// Voltage MuMpcMeasurment Object Creation (Scenario 4)
//
//-------------------------------------------------------------------------------------------//
ElectricalConnectionPhaseNameType GetPhaseFrom(ElectricalConnectionPhaseNameType phases) {
  switch (phases) {
    case kElectricalConnectionPhaseNameTypeA: return kElectricalConnectionPhaseNameTypeA;
    case kElectricalConnectionPhaseNameTypeB: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeC: return kElectricalConnectionPhaseNameTypeC;
    case kElectricalConnectionPhaseNameTypeAb: return kElectricalConnectionPhaseNameTypeA;
    case kElectricalConnectionPhaseNameTypeBc: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeAc: return kElectricalConnectionPhaseNameTypeC;
    default: return 0;  // Invalid phase
  }
}

ElectricalConnectionPhaseNameType GetPhaseTo(ElectricalConnectionPhaseNameType phase) {
  switch (phase) {
    case kElectricalConnectionPhaseNameTypeA: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeB: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeC: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeAb: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeBc: return kElectricalConnectionPhaseNameTypeC;
    case kElectricalConnectionPhaseNameTypeAc: return kElectricalConnectionPhaseNameTypeA;
    default: return 0;  // Invalid phase
  }
}

EebusError ConfigureVoltage(
    MuMpcMeasurement* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const MeasurementDescriptionDataType measurement_description = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeVoltage},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeV},
      .scope_type       = &self->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &measurement_description, &self->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType parameter_description = {
      .electrical_connection_id    = &electrical_connection_id,
      .measurement_id              = &self->id,
      .voltage_type                = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases          = &(ElectricalConnectionPhaseNameType){GetPhaseFrom(self->phases)},
      .ac_measured_in_reference_to = &(ElectricalConnectionPhaseNameType){GetPhaseTo(self->phases)},
      .ac_measurement_type         = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Apparent),
      .ac_measurement_variant      = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType parameter_description_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &parameter_description, &parameter_description_id);
}

ElectricalConnectionPhaseNameType GetVoltagePhaseWithNameId(MuMpcMeasurementNameId name) {
  switch (name) {
    case kMpcVoltagePhaseA: return kElectricalConnectionPhaseNameTypeA;
    case kMpcVoltagePhaseB: return kElectricalConnectionPhaseNameTypeB;
    case kMpcVoltagePhaseC: return kElectricalConnectionPhaseNameTypeC;
    case kMpcVoltagePhaseAb: return kElectricalConnectionPhaseNameTypeAb;
    case kMpcVoltagePhaseBc: return kElectricalConnectionPhaseNameTypeBc;
    case kMpcVoltagePhaseAc: return kElectricalConnectionPhaseNameTypeAc;
    default: return kElectricalConnectionPhaseNameTypeNone;  // Invalid phase
  }
}

MuMpcMeasurementObject* MuMpcMeasurementVoltageCreate(MuMpcMeasurementNameId name, const MuMpcMeasurementConfig* cfg) {
  if (!((uint8_t)name & (uint8_t)kMpcMonitorVoltage)) {
    return NULL;
  }

  const ElectricalConnectionPhaseNameType phases = GetVoltagePhaseWithNameId(name);
  return MuMpcMeasurementCreateInternal(name, kScopeTypeTypeACVoltage, phases, cfg, ConfigureVoltage);
}

//-------------------------------------------------------------------------------------------//
//
// Frequency MuMpcMeasurment Object Creation (Scenario 5)
//
//-------------------------------------------------------------------------------------------//
EebusError ConfigureFrequency(
    MuMpcMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
) {
  const MeasurementDescriptionDataType measurement_description = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeFrequency},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeHz},
      .scope_type       = &measurement->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &measurement_description, &measurement->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType parameter_description = {
      .electrical_connection_id = &electrical_connection_id,
      .measurement_id           = &measurement->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
  };

  ElectricalConnectionParameterIdType parameter_description_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &parameter_description, &parameter_description_id);
}

MuMpcMeasurementObject* MuMpcMeasurementFrequencyCreate(const MuMpcMeasurementConfig* cfg) {
  const ScopeTypeType scope = kScopeTypeTypeACFrequency;

  const ElectricalConnectionPhaseNameType phase = kElectricalConnectionPhaseNameTypeNone;
  return MuMpcMeasurementCreateInternal(kMpcFrequency, scope, phase, cfg, ConfigureFrequency);
}

//-------------------------------------------------------------------------------------------//
//
// Generic MuMpcMeasurement Object Creation entry point
//
//-------------------------------------------------------------------------------------------//
MuMpcMeasurementObject* MuMpcMeasurementCreate(MuMpcMeasurementNameId name, const MuMpcMeasurementConfig* cfg) {
  switch (name) {
    case kMpcCurrentPhaseA:
    case kMpcCurrentPhaseB:
    case kMpcCurrentPhaseC: {
      return MuMpcMeasurementCurrentCreate(name, cfg);
    }

    case kMpcEnergyConsumed:
    case kMpcEnergyProduced: {
      return MuMpcMeasurementEnergyCreate(name, cfg);
    }

    case kMpcFrequency: {
      return MuMpcMeasurementFrequencyCreate(cfg);
    }

    case kMpcPowerPhaseA:
    case kMpcPowerPhaseB:
    case kMpcPowerPhaseC: {
      return MuMpcMeasurementPowerCreate(name, cfg);
    }

    case kMpcVoltagePhaseA:
    case kMpcVoltagePhaseB:
    case kMpcVoltagePhaseC:
    case kMpcVoltagePhaseAb:
    case kMpcVoltagePhaseBc:
    case kMpcVoltagePhaseAc: {
      return MuMpcMeasurementVoltageCreate(name, cfg);
    }

    default: return NULL;  // Invalid measurement name
  }
}
