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
 * @brief GCP MGCP Measurement implementation
 */

#include <string.h>

#include "src/common/array_util.h"
#include "src/spine/model/absolute_or_relative_time.h"
#include "src/spine/model/measurement_types.h"
#include "src/use_case/actor/gcp/mgcp/gcp_mgcp_measurement.h"

typedef struct GcpMgcpMeasurement GcpMgcpMeasurement;

typedef EebusError (*MeasurementConfigurationStrategy)(
    GcpMgcpMeasurement* measurement,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id
);

struct GcpMgcpMeasurement {
  GcpMgcpMeasurementObject obj;

  GcpMeasurementNameId name;
  MeasurementIdType id;
  ScopeTypeType scope;
  ElectricalConnectionPhaseNameType phases;
  MeasurementValueSourceType value_source;
  MeasurementConstraintsDataType* constraints;
  MeasurementConfigurationStrategy cfg_strategy;
  MeasurementDataType* measurement_data;
};

#define MEASUREMENT(obj) ((GcpMgcpMeasurement*)(obj))

/* ---- interface method forward declarations ---- */
static void Destruct(GcpMgcpMeasurementObject* self);
static GcpMeasurementNameId GetName(const GcpMgcpMeasurementObject* self);
static EebusError GetDataValue(const GcpMgcpMeasurementObject* self, MeasurementServer* msrv, ScaledValue* value);
static const MeasurementConstraintsDataType* GetConstraints(const GcpMgcpMeasurementObject* self);
static EebusError Configure(
    GcpMgcpMeasurementObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
);
static EebusError SetDataCache(
    GcpMgcpMeasurementObject* self,
    const ScaledValue* value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
);
static MeasurementDataType* ReleaseDataCache(GcpMgcpMeasurementObject* self);

static const GcpMgcpMeasurementInterface measurement_methods = {
    .destruct           = Destruct,
    .get_name           = GetName,
    .get_data_value     = GetDataValue,
    .get_constraints    = GetConstraints,
    .configure          = Configure,
    .set_data_cache     = SetDataCache,
    .release_data_cache = ReleaseDataCache,
};

/* ---- construction ---- */

static EebusError GcpMgcpMeasurementConstruct(
    GcpMgcpMeasurement* self,
    GcpMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const GcpMgcpMeasurementConfig* cfg,
    MeasurementConfigurationStrategy cfg_strategy
) {
  GCP_MGCP_MEASUREMENT_INTERFACE(self) = &measurement_methods;

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

  self->value_source = cfg->value_source;

  if (cfg->constraints != NULL) {
    self->constraints = MeasurementConstraintsDataCopy(cfg->constraints);
    if (self->constraints == NULL) {
      return kEebusErrorMemoryAllocate;
    }
  }

  return kEebusErrorOk;
}

static GcpMgcpMeasurementObject* GcpMgcpMeasurementCreateInternal(
    GcpMeasurementNameId name,
    ScopeTypeType scope,
    ElectricalConnectionPhaseNameType phases,
    const GcpMgcpMeasurementConfig* cfg,
    MeasurementConfigurationStrategy cfg_strategy
) {
  GcpMgcpMeasurement* const m = (GcpMgcpMeasurement*)EEBUS_MALLOC(sizeof(GcpMgcpMeasurement));
  if (m == NULL) {
    return NULL;
  }

  if (GcpMgcpMeasurementConstruct(m, name, scope, phases, cfg, cfg_strategy) != kEebusErrorOk) {
    GcpMgcpMeasurementDelete(GCP_MGCP_MEASUREMENT_OBJECT(m));
    return NULL;
  }

  return GCP_MGCP_MEASUREMENT_OBJECT(m);
}

/* ---- interface implementations ---- */

static void Destruct(GcpMgcpMeasurementObject* self) {
  GcpMgcpMeasurement* const m = MEASUREMENT(self);
  MeasurementConstraintsDataDelete(m->constraints);
  m->constraints = NULL;
  MeasurementDataDelete(m->measurement_data);
  m->measurement_data = NULL;
}

static GcpMeasurementNameId GetName(const GcpMgcpMeasurementObject* self) {
  return MEASUREMENT(self)->name;
}

static EebusError GetDataValue(const GcpMgcpMeasurementObject* self, MeasurementServer* msrv, ScaledValue* value) {
  const GcpMgcpMeasurement* const m = MEASUREMENT(self);
  if ((msrv == NULL) || (value == NULL)) {
    return kEebusErrorInputArgumentNull;
  }
  const MeasurementDataType* const data = MeasurementCommonGetMeasurementWithId(&msrv->measurement_common, m->id);
  if (data == NULL) {
    return kEebusErrorNoChange;
  }
  return ScaledValueInitWithScaledNumber(value, data->value);
}

static const MeasurementConstraintsDataType* GetConstraints(const GcpMgcpMeasurementObject* self) {
  return MEASUREMENT(self)->constraints;
}

static EebusError Configure(
    GcpMgcpMeasurementObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  GcpMgcpMeasurement* const m = MEASUREMENT(self);
  if (m->cfg_strategy == NULL) {
    return kEebusErrorInit;
  }

  EebusError err = m->cfg_strategy(m, msrv, ecsrv, ec_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  if (m->constraints != NULL) {
    err = MeasurementConstraintsSetId(m->constraints, m->id);
  }

  return err;
}

static EebusError SetDataCache(
    GcpMgcpMeasurementObject* self,
    const ScaledValue* measured_value,
    const EebusDateTime* timestamp,
    const MeasurementValueStateType* value_state,
    const EebusDateTime* start_time,
    const EebusDateTime* end_time
) {
  GcpMgcpMeasurement* const m = MEASUREMENT(self);

  if (measured_value == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  MeasurementDataDelete(m->measurement_data);

  const AbsoluteOrRelativeTimeType* const start_tmp = ABSOLUTE_OR_RELATIVE_TIME_PTR(start_time);
  const AbsoluteOrRelativeTimeType* const end_tmp   = ABSOLUTE_OR_RELATIVE_TIME_PTR(end_time);

  const TimePeriodType* const eval_period = ((start_tmp != NULL) && (end_tmp != NULL))
                                                ? &(TimePeriodType){.start_time = start_tmp, .end_time = end_tmp}
                                                : NULL;

  const MeasurementDataType new_data = {
      .measurement_id    = &m->id,
      .value_type        = &(MeasurementValueTypeType){kMeasurementValueTypeTypeValue},
      .timestamp         = ABSOLUTE_OR_RELATIVE_TIME_PTR(timestamp),
      .value             = &(ScaledNumberType){.number = &measured_value->value, .scale = &measured_value->scale},
      .evaluation_period = eval_period,
      .value_source      = &m->value_source,
      .value_tendency    = NULL,
      .value_state       = value_state,
  };

  m->measurement_data = MeasurementDataCopy(&new_data);
  if (m->measurement_data == NULL) {
    return kEebusErrorMemoryAllocate;
  }

  return kEebusErrorOk;
}

static MeasurementDataType* ReleaseDataCache(GcpMgcpMeasurementObject* self) {
  GcpMgcpMeasurement* const m     = MEASUREMENT(self);
  MeasurementDataType* const data = m->measurement_data;
  m->measurement_data             = NULL;
  return data;
}

/* ---- configuration strategies ---- */

static EebusError
EnsureElectricalConnectionDescription(ElectricalConnectionServer* ecsrv, ElectricalConnectionIdType ec_id) {
  if (ElectricalConnectionCommonGetDescriptionWithId(&ecsrv->el_connection_common, ec_id) != NULL) {
    return kEebusErrorOk;
  }

  const ElectricalConnectionDescriptionDataType desc = {
      .power_supply_type         = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .positive_energy_direction = &(EnergyDirectionType){kEnergyDirectionTypeConsume},
  };

  return ElectricalConnectionServerAddDescriptionWithId(ecsrv, &desc, ec_id);
}

/* --- Scenario 2: Power --- */
static EebusError ConfigurePower(
    GcpMgcpMeasurement* m,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  const MeasurementDescriptionDataType meas_desc = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypePower},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeW},
      .scope_type       = &m->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &meas_desc, &m->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  err = EnsureElectricalConnectionDescription(ecsrv, ec_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType param_desc = {
      .electrical_connection_id    = &ec_id,
      .measurement_id              = &m->id,
      .voltage_type                = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases          = &m->phases,
      .ac_measured_in_reference_to = &ELECTRICAL_CONNECTION_PHASE_NAME(Neutral),
      .ac_measurement_type         = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
      .ac_measurement_variant      = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType param_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &param_desc, &param_id);
}

/* --- Scenarios 3 & 4: Energy --- */
static EebusError ConfigureEnergy(
    GcpMgcpMeasurement* m,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  const MeasurementDescriptionDataType meas_desc = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeEnergy},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeWh},
      .scope_type       = &m->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &meas_desc, &m->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType param_desc = {
      .electrical_connection_id = &ec_id,
      .measurement_id           = &m->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measurement_type      = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
  };

  ElectricalConnectionParameterIdType param_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &param_desc, &param_id);
}

/* --- Scenario 5: Current --- */
static EebusError ConfigureCurrent(
    GcpMgcpMeasurement* m,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  const MeasurementDescriptionDataType meas_desc = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeCurrent},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeA},
      .scope_type       = &m->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &meas_desc, &m->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  err = EnsureElectricalConnectionDescription(ecsrv, ec_id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType param_desc = {
      .electrical_connection_id = &ec_id,
      .measurement_id           = &m->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases       = &m->phases,
      .ac_measurement_type      = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Real),
      .ac_measurement_variant   = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType param_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &param_desc, &param_id);
}

/* --- Scenario 6: Voltage --- */
static ElectricalConnectionPhaseNameType VoltagePhaseFrom(ElectricalConnectionPhaseNameType phases) {
  switch (phases) {
    case kElectricalConnectionPhaseNameTypeA: return kElectricalConnectionPhaseNameTypeA;
    case kElectricalConnectionPhaseNameTypeB: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeC: return kElectricalConnectionPhaseNameTypeC;
    case kElectricalConnectionPhaseNameTypeAb: return kElectricalConnectionPhaseNameTypeA;
    case kElectricalConnectionPhaseNameTypeBc: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeAc: return kElectricalConnectionPhaseNameTypeC;
    default: return 0;
  }
}

static ElectricalConnectionPhaseNameType VoltagePhaseTo(ElectricalConnectionPhaseNameType phases) {
  switch (phases) {
    case kElectricalConnectionPhaseNameTypeA: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeB: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeC: return kElectricalConnectionPhaseNameTypeNeutral;
    case kElectricalConnectionPhaseNameTypeAb: return kElectricalConnectionPhaseNameTypeB;
    case kElectricalConnectionPhaseNameTypeBc: return kElectricalConnectionPhaseNameTypeC;
    case kElectricalConnectionPhaseNameTypeAc: return kElectricalConnectionPhaseNameTypeA;
    default: return 0;
  }
}

static EebusError ConfigureVoltage(
    GcpMgcpMeasurement* m,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  const MeasurementDescriptionDataType meas_desc = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeVoltage},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeV},
      .scope_type       = &m->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &meas_desc, &m->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType param_desc = {
      .electrical_connection_id    = &ec_id,
      .measurement_id              = &m->id,
      .voltage_type                = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
      .ac_measured_phases          = &(ElectricalConnectionPhaseNameType){VoltagePhaseFrom(m->phases)},
      .ac_measured_in_reference_to = &(ElectricalConnectionPhaseNameType){VoltagePhaseTo(m->phases)},
      .ac_measurement_type         = &ELECTRICAL_CONNECTION_AC_MEASUREMENT_TYPE(Apparent),
      .ac_measurement_variant      = &ELECTRICAL_CONNECTION_MEASURAND_VARIANT(Rms),
  };

  ElectricalConnectionParameterIdType param_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &param_desc, &param_id);
}

/* --- Scenario 7: Frequency --- */
static EebusError ConfigureFrequency(
    GcpMgcpMeasurement* m,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id
) {
  const MeasurementDescriptionDataType meas_desc = {
      .measurement_type = &(MeasurementTypeType){kMeasurementTypeTypeFrequency},
      .commodity_type   = &(CommodityTypeType){kCommodityTypeTypeElectricity},
      .unit             = &(UnitOfMeasurementType){kUnitOfMeasurementTypeHz},
      .scope_type       = &m->scope,
  };

  EebusError err = MeasurementServerAddDescription(msrv, &meas_desc, &m->id);
  if (err != kEebusErrorOk) {
    return err;
  }

  const ElectricalConnectionParameterDescriptionDataType param_desc = {
      .electrical_connection_id = &ec_id,
      .measurement_id           = &m->id,
      .voltage_type             = &ELECTRICAL_CONNECTION_VOLTAGE_TYPE(Ac),
  };

  ElectricalConnectionParameterIdType param_id;
  return ElectricalConnectionServerAddParameterDescription(ecsrv, &param_desc, &param_id);
}

/* ---- public factory ---- */

static ScopeTypeType GetEnergyScopeType(GcpMeasurementNameId name) {
  switch (name) {
    case kGcpEnergyFeedIn: return kScopeTypeTypeGridFeedIn;
    case kGcpEnergyConsumed: return kScopeTypeTypeGridConsumption;
    default: return kScopeTypeTypeACEnergy;
  }
}

static ElectricalConnectionPhaseNameType GetCurrentPhase(GcpMeasurementNameId name) {
  switch (name) {
    case kGcpCurrentPhaseA: return kElectricalConnectionPhaseNameTypeA;
    case kGcpCurrentPhaseB: return kElectricalConnectionPhaseNameTypeB;
    case kGcpCurrentPhaseC: return kElectricalConnectionPhaseNameTypeC;
    default: return kElectricalConnectionPhaseNameTypeNone;
  }
}

static ElectricalConnectionPhaseNameType GetVoltagePhase(GcpMeasurementNameId name) {
  switch (name) {
    case kGcpVoltagePhaseA: return kElectricalConnectionPhaseNameTypeA;
    case kGcpVoltagePhaseB: return kElectricalConnectionPhaseNameTypeB;
    case kGcpVoltagePhaseC: return kElectricalConnectionPhaseNameTypeC;
    case kGcpVoltagePhaseAb: return kElectricalConnectionPhaseNameTypeAb;
    case kGcpVoltagePhaseBc: return kElectricalConnectionPhaseNameTypeBc;
    case kGcpVoltagePhaseAc: return kElectricalConnectionPhaseNameTypeAc;
    default: return kElectricalConnectionPhaseNameTypeNone;
  }
}

GcpMgcpMeasurementObject*
GcpMgcpMeasurementPowerTotalCreate(ElectricalConnectionPhaseNameType phases, const GcpMgcpMeasurementConfig* cfg) {
  return GcpMgcpMeasurementCreateInternal(kGcpPowerTotal, kScopeTypeTypeACPowerTotal, phases, cfg, ConfigurePower);
}

GcpMgcpMeasurementObject* GcpMgcpMeasurementCreate(GcpMeasurementNameId name, const GcpMgcpMeasurementConfig* cfg) {
  switch (name) {
    case kGcpPowerTotal:
      /* Power total: phases is set per-monitor via GcpMgcpMonitorPowerCreate */
      return GcpMgcpMeasurementCreateInternal(
          name,
          kScopeTypeTypeACPowerTotal,
          kElectricalConnectionPhaseNameTypeNone,
          cfg,
          ConfigurePower
      );

    case kGcpEnergyFeedIn:
    case kGcpEnergyConsumed:
      return GcpMgcpMeasurementCreateInternal(
          name,
          GetEnergyScopeType(name),
          kElectricalConnectionPhaseNameTypeNone,
          cfg,
          ConfigureEnergy
      );

    case kGcpCurrentPhaseA:
    case kGcpCurrentPhaseB:
    case kGcpCurrentPhaseC:
      return GcpMgcpMeasurementCreateInternal(
          name,
          kScopeTypeTypeACCurrent,
          GetCurrentPhase(name),
          cfg,
          ConfigureCurrent
      );

    case kGcpVoltagePhaseA:
    case kGcpVoltagePhaseB:
    case kGcpVoltagePhaseC:
    case kGcpVoltagePhaseAb:
    case kGcpVoltagePhaseBc:
    case kGcpVoltagePhaseAc:
      return GcpMgcpMeasurementCreateInternal(
          name,
          kScopeTypeTypeACVoltage,
          GetVoltagePhase(name),
          cfg,
          ConfigureVoltage
      );

    case kGcpFrequency:
      return GcpMgcpMeasurementCreateInternal(
          name,
          kScopeTypeTypeACFrequency,
          kElectricalConnectionPhaseNameTypeNone,
          cfg,
          ConfigureFrequency
      );

    default: return NULL;
  }
}
