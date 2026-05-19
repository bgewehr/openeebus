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
 * @brief Mu Mpc Monitor implementation
 */

#include "src/use_case/actor/mu/mpc/mu_mpc_monitor.h"
#include "src/common/array_util.h"
#include "src/common/eebus_malloc.h"
#include "src/common/vector.h"
#include "src/use_case/api/mu_mpc_monitor_interface.h"

typedef struct MuMpcMonitor MuMpcMonitor;

struct MuMpcMonitor {
  /** Implements the Mu Mpc Monitor Interface */
  MuMpcMonitorObject obj;

  /** The name of the monitor */
  MuMpcMonitorNameId name;
  /** Container for all current measurements */
  Vector measurements;
};

#define MU_MPC_MONITOR(obj) ((MuMpcMonitor*)(obj))

static void Destruct(MuMpcMonitorObject* self);
static MuMpcMonitorNameId GetName(const MuMpcMonitorObject* self);
static EebusError Configure(
    MuMpcMonitorObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id,
    MeasurementConstraintsListDataType* measurements_constraints
);
static EebusMeasurementObject*
GetMeasurement(const MuMpcMonitorObject* self, MuMpcMeasurementNameId measurement_name_id);
static EebusError FlushMeasurementCache(MuMpcMonitorObject* self, MeasurementListDataType* measurement_data_list);

static const MuMpcMonitorInterface mu_mpc_monitor_methods = {
    .destruct                = Destruct,
    .get_name                = GetName,
    .configure               = Configure,
    .get_measurement         = GetMeasurement,
    .flush_measurement_cache = FlushMeasurementCache,
};

typedef struct MeasurementParameters {
  MuMpcMeasurementNameId measurement_name;
  const MuMpcMeasurementConfig* cfg;
} MeasurementParameters;

static void MeasurementDeallocator(void* p);
EebusError MuMpcMonitorConstruct(MuMpcMonitor* self, MuMpcMonitorNameId name);
static EebusError MuMpcMonitorConstructWithMeasurements(
    MuMpcMonitor* self,
    MuMpcMonitorNameId name,
    const MeasurementParameters* measurement_params,
    size_t measurement_params_size
);
static EebusError
AddMeasurements(MuMpcMonitor* self, const MeasurementParameters* measurement_params, size_t measurement_params_size);

void MeasurementDeallocator(void* p) {
  MuMpcMeasurementDelete((EebusMeasurementObject*)p);
}

EebusError MuMpcMonitorConstruct(MuMpcMonitor* self, MuMpcMonitorNameId name) {
  // Override "virtual functions table"
  MU_MPC_MONITOR_INTERFACE(self) = &mu_mpc_monitor_methods;

  self->name = name;
  VectorConstructWithDeallocator(&self->measurements, MeasurementDeallocator);
  return kEebusErrorOk;
}

EebusError MuMpcMonitorConstructWithMeasurements(
    MuMpcMonitor* self,
    MuMpcMonitorNameId name,
    const MeasurementParameters* measurement_params,
    size_t measurement_params_size
) {
  const EebusError err = MuMpcMonitorConstruct(self, name);
  if (err != kEebusErrorOk) {
    return err;  // Failed to construct the monitor
  }

  return AddMeasurements(self, measurement_params, measurement_params_size);
}

MuMpcMonitorObject* MuMpcMonitorCreate(
    MuMpcMonitorNameId name,
    const MeasurementParameters* measurement_params,
    size_t measurement_params_size
) {
  if (measurement_params == NULL || measurement_params_size == 0) {
    return NULL;  // Invalid parameters
  }

  MuMpcMonitor* const monitor = EEBUS_MALLOC(sizeof(MuMpcMonitor));
  if (monitor == NULL) {
    return NULL;  // Memory allocation failed
  }

  const EebusError err
      = MuMpcMonitorConstructWithMeasurements(monitor, name, measurement_params, measurement_params_size);
  if (err != kEebusErrorOk) {
    MuMpcMonitorDelete(MU_MPC_MONITOR_OBJECT(monitor));
    return NULL;  // Failed to construct the monitor with measurements
  }

  return MU_MPC_MONITOR_OBJECT(monitor);
}

void Destruct(MuMpcMonitorObject* self) {
  MuMpcMonitor* const monitor = MU_MPC_MONITOR(self);

  VectorFreeElements(&monitor->measurements);
  VectorDestruct(&monitor->measurements);
}

MuMpcMonitorNameId GetName(const MuMpcMonitorObject* self) {
  return MU_MPC_MONITOR(self)->name;
}

EebusError Configure(
    MuMpcMonitorObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType electrical_connection_id,
    MeasurementConstraintsListDataType* measurements_constraints
) {
  MuMpcMonitor* const monitor = MU_MPC_MONITOR(self);

  if ((msrv == NULL) || (ecsrv == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    EebusMeasurementObject* const measurement = (EebusMeasurementObject*)VectorGetElement(&monitor->measurements, i);

    if (measurement != NULL) {
      const EebusError err = EEBUS_MEASUREMENT_CONFIGURE(measurement, msrv, ecsrv, electrical_connection_id);
      if (err != kEebusErrorOk) {
        return err;
      }
    }

    const MeasurementConstraintsDataType* constraints = EEBUS_MEASUREMENT_GET_CONSTRAINTS(measurement);
    if (constraints != NULL) {
      const EebusError err = MeasurementConstraintsAdd(measurements_constraints, constraints);
      if (err != kEebusErrorOk) {
        return err;
      }
    }
  }

  return kEebusErrorOk;
}

EebusError
AddMeasurements(MuMpcMonitor* self, const MeasurementParameters* measurement_params, size_t measurement_params_size) {
  for (size_t i = 0; i < measurement_params_size; ++i) {
    if (measurement_params[i].cfg != NULL) {
      EebusMeasurementObject* const measurement
          = MuMpcMeasurementCreate(measurement_params[i].measurement_name, measurement_params[i].cfg);
      if (measurement == NULL) {
        return kEebusErrorInit;
      }

      VectorPushBack(&self->measurements, measurement);
    }
  }

  return kEebusErrorOk;
}

EebusMeasurementObject* GetMeasurement(const MuMpcMonitorObject* self, MuMpcMeasurementNameId measurement_name_id) {
  MuMpcMonitor* const monitor = MU_MPC_MONITOR(self);

  const MuMpcMonitorNameId monitor_name = monitor->name & (MuMpcMonitorNameId)kMpcMonitorNameIdMask;

  if (monitor_name != (MuMpcMonitorNameId)((uint8_t)measurement_name_id & (uint8_t)kMpcMonitorNameIdMask)) {
    return NULL;  // Measurement not found in this monitor
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    EebusMeasurementObject* const measurement = (EebusMeasurementObject*)VectorGetElement(&monitor->measurements, i);
    if (EEBUS_MEASUREMENT_GET_NAME(measurement) == measurement_name_id) {
      return measurement;  // Measurement found
    }
  }

  return NULL;
}

EebusError FlushMeasurementCache(MuMpcMonitorObject* self, MeasurementListDataType* measurement_data_list) {
  MuMpcMonitor* const monitor = MU_MPC_MONITOR(self);

  if (measurement_data_list == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    EebusMeasurementObject* const measurement = (EebusMeasurementObject*)VectorGetElement(&monitor->measurements, i);

    MeasurementDataType* data = EEBUS_MEASUREMENT_RELEASE_DATA_CACHE(measurement);
    if (data != NULL) {
      EebusError err = EebusDataListDataAppend(
          (void***)&measurement_data_list->measurement_data,
          &measurement_data_list->measurement_data_size,
          data
      );

      if (err != kEebusErrorOk) {
        MeasurementDataDelete(data);
        return err;
      }
    }
  }

  return kEebusErrorOk;
}

//-------------------------------------------------------------------------------------------//
//
// MuMpcMonitorPower Object Creation (Scenario 1)
//
//-------------------------------------------------------------------------------------------//
uint8_t GetConnectedPhases(const MuMpcMonitorPowerConfig* cfg) {
  ElectricalConnectionPhaseNameType connected_phases = 0;

  if (cfg->power_phase_a_cfg != NULL) {
    connected_phases |= kPhaseA;
  }

  if (cfg->power_phase_b_cfg != NULL) {
    connected_phases |= kPhaseB;
  }

  if (cfg->power_phase_c_cfg != NULL) {
    connected_phases |= kPhaseC;
  }

  return connected_phases;
}

EebusError MuMpcMonitorPowerConstruct(MuMpcMonitor* self, const MuMpcMonitorPowerConfig* cfg) {
  MuMpcMonitorConstruct(self, kMpcMonitorPower);

  if (cfg == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  const uint8_t connected_phases = GetConnectedPhases(cfg);
  EebusMeasurementObject* const power_measurement_total
      = MuMpcMeasurementPowerTotalCreate(connected_phases, &cfg->power_total_cfg);
  if (power_measurement_total == NULL) {
    return kEebusErrorInit;
  }

  VectorPushBack(&self->measurements, power_measurement_total);

  if (connected_phases != 0) {
    MeasurementParameters measurement_params[] = {
        {kMpcPowerPhaseA, cfg->power_phase_a_cfg},
        {kMpcPowerPhaseB, cfg->power_phase_b_cfg},
        {kMpcPowerPhaseC, cfg->power_phase_c_cfg},
    };

    return AddMeasurements(self, measurement_params, ARRAY_SIZE(measurement_params));
  }

  return kEebusErrorOk;
}

MuMpcMonitorObject* MuMpcMonitorPowerCreate(const MuMpcMonitorPowerConfig* cfg) {
  MuMpcMonitor* const mu_mpc_monitor = (MuMpcMonitor*)EEBUS_MALLOC(sizeof(MuMpcMonitor));
  if (mu_mpc_monitor == NULL) {
    return NULL;
  }

  EebusError err = MuMpcMonitorPowerConstruct(mu_mpc_monitor, cfg);
  if (err != kEebusErrorOk) {
    MuMpcMonitorDelete(MU_MPC_MONITOR_OBJECT(mu_mpc_monitor));
    return NULL;
  }

  return MU_MPC_MONITOR_OBJECT(mu_mpc_monitor);
}

//-------------------------------------------------------------------------------------------//
//
// MuMpcMonitorEnergy Object Creation (Scenario 2)
//
//-------------------------------------------------------------------------------------------//
MuMpcMonitorObject* MuMpcMonitorEnergyCreate(const MuMpcMonitorEnergyConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  if ((cfg->energy_consumption_cfg == NULL) && (cfg->energy_production_cfg == NULL)) {
    return NULL;
  }

  const MeasurementParameters measurement_params[] = {
      {kMpcEnergyConsumed, cfg->energy_consumption_cfg},
      {kMpcEnergyProduced,  cfg->energy_production_cfg},
  };

  return MuMpcMonitorCreate(kMpcMonitorEnergy, measurement_params, ARRAY_SIZE(measurement_params));
}

//-------------------------------------------------------------------------------------------//
//
// MuMpcMonitorCurrent Object Creation (Scenario 3)
//
//-------------------------------------------------------------------------------------------//
MuMpcMonitorObject* MuMpcMonitorCurrentCreate(const MuMpcMonitorCurrentConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  if ((cfg->current_phase_a_cfg == NULL) && (cfg->current_phase_b_cfg == NULL) && (cfg->current_phase_c_cfg == NULL)) {
    return NULL;
  }

  const MeasurementParameters measurement_params[] = {
      {kMpcCurrentPhaseA, cfg->current_phase_a_cfg},
      {kMpcCurrentPhaseB, cfg->current_phase_b_cfg},
      {kMpcCurrentPhaseC, cfg->current_phase_c_cfg},
  };

  return MuMpcMonitorCreate(kMpcMonitorCurrent, measurement_params, ARRAY_SIZE(measurement_params));
}

//-------------------------------------------------------------------------------------------//
//
// MuMpcMonitorVoltage Object Creation (Scenario 4)
//
//-------------------------------------------------------------------------------------------//
MuMpcMonitorObject* MuMpcMonitorVoltageCreate(const MuMpcMonitorVoltageConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  // AB voltage configuration check
  if ((cfg->voltage_phase_ab_cfg != NULL)
      && ((cfg->voltage_phase_a_cfg == NULL) || (cfg->voltage_phase_b_cfg == NULL))) {
    return NULL;
  }

  // BC voltage configuration check
  if ((cfg->voltage_phase_bc_cfg != NULL)
      && ((cfg->voltage_phase_b_cfg == NULL) || (cfg->voltage_phase_c_cfg == NULL))) {
    return NULL;
  }

  // AC voltage configuration check
  if ((cfg->voltage_phase_ac_cfg != NULL)
      && ((cfg->voltage_phase_c_cfg == NULL) || (cfg->voltage_phase_a_cfg == NULL))) {
    return NULL;
  }

  const MeasurementParameters measurement_params[] = {
      { kMpcVoltagePhaseA,  cfg->voltage_phase_a_cfg},
      { kMpcVoltagePhaseB,  cfg->voltage_phase_b_cfg},
      { kMpcVoltagePhaseC,  cfg->voltage_phase_c_cfg},
      {kMpcVoltagePhaseAb, cfg->voltage_phase_ab_cfg},
      {kMpcVoltagePhaseBc, cfg->voltage_phase_bc_cfg},
      {kMpcVoltagePhaseAc, cfg->voltage_phase_ac_cfg},
  };

  return MuMpcMonitorCreate(kMpcMonitorVoltage, measurement_params, ARRAY_SIZE(measurement_params));
}

//-------------------------------------------------------------------------------------------//
//
// MuMpcMonitorFrequency Object Creation (Scenario 5)
//
//-------------------------------------------------------------------------------------------//
MuMpcMonitorObject* MuMpcMonitorFrequencyCreate(const MuMpcMonitorFrequencyConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  const MeasurementParameters measurement_params[] = {
      {kMpcFrequency, &cfg->frequency_cfg},
  };

  return MuMpcMonitorCreate(kMpcMonitorFrequency, measurement_params, ARRAY_SIZE(measurement_params));
}
