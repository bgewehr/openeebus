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
 * @brief GCP MGCP Monitor implementation
 */

#include "src/use_case/actor/gcp/mgcp/gcp_mgcp_monitor.h"

#include "src/common/array_util.h"
#include "src/common/eebus_malloc.h"
#include "src/common/vector.h"
#include "src/use_case/api/gcp_mgcp_monitor_interface.h"

typedef struct GcpMgcpMonitor GcpMgcpMonitor;

struct GcpMgcpMonitor {
  GcpMgcpMonitorObject obj;
  GcpMonitorNameId name;
  Vector measurements; /**< Vector of GcpMgcpMeasurementObject* */
};

#define GCP_MGCP_MONITOR(obj) ((GcpMgcpMonitor*)(obj))

static void Destruct(GcpMgcpMonitorObject* self);
static GcpMonitorNameId GetName(const GcpMgcpMonitorObject* self);
static EebusError Configure(
    GcpMgcpMonitorObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id,
    MeasurementConstraintsListDataType* constraints
);
static GcpMgcpMeasurementObject* GetMeasurement(const GcpMgcpMonitorObject* self, GcpMeasurementNameId name_id);
static EebusError FlushMeasurementCache(GcpMgcpMonitorObject* self, MeasurementListDataType* list);

static const GcpMgcpMonitorInterface gcp_mgcp_monitor_methods = {
    .destruct                = Destruct,
    .get_name                = GetName,
    .configure               = Configure,
    .get_measurement         = GetMeasurement,
    .flush_measurement_cache = FlushMeasurementCache,
};

typedef struct MeasurementParam MeasurementParam;
struct MeasurementParam {
  GcpMeasurementNameId name;
  const GcpMgcpMeasurementConfig* cfg;
};

static void MeasurementDeallocator(void* p) {
  GcpMgcpMeasurementDelete((GcpMgcpMeasurementObject*)p);
}

static EebusError MonitorConstruct(GcpMgcpMonitor* self, GcpMonitorNameId name) {
  GCP_MGCP_MONITOR_INTERFACE(self) = &gcp_mgcp_monitor_methods;
  self->name                       = name;
  VectorConstructWithDeallocator(&self->measurements, MeasurementDeallocator);
  return kEebusErrorOk;
}

static EebusError AddMeasurements(GcpMgcpMonitor* self, const MeasurementParam* params, size_t params_size) {
  for (size_t i = 0; i < params_size; ++i) {
    if (params[i].cfg == NULL) {
      continue; // optional — skip
    }
    GcpMgcpMeasurementObject* const m = GcpMgcpMeasurementCreate(params[i].name, params[i].cfg);
    if (m == NULL) {
      return kEebusErrorInit;
    }
    VectorPushBack(&self->measurements, m);
  }
  return kEebusErrorOk;
}

static GcpMgcpMonitorObject* MonitorCreate(GcpMonitorNameId name, const MeasurementParam* params, size_t params_size) {
  GcpMgcpMonitor* const monitor = (GcpMgcpMonitor*)EEBUS_MALLOC(sizeof(GcpMgcpMonitor));
  if (monitor == NULL) {
    return NULL;
  }

  if (MonitorConstruct(monitor, name) != kEebusErrorOk) {
    GcpMgcpMonitorDelete(GCP_MGCP_MONITOR_OBJECT(monitor));
    return NULL;
  }

  if (AddMeasurements(monitor, params, params_size) != kEebusErrorOk) {
    GcpMgcpMonitorDelete(GCP_MGCP_MONITOR_OBJECT(monitor));
    return NULL;
  }

  return GCP_MGCP_MONITOR_OBJECT(monitor);
}

static void Destruct(GcpMgcpMonitorObject* self) {
  GcpMgcpMonitor* const monitor = GCP_MGCP_MONITOR(self);
  VectorFreeElements(&monitor->measurements);
  VectorDestruct(&monitor->measurements);
}

static GcpMonitorNameId GetName(const GcpMgcpMonitorObject* self) {
  return GCP_MGCP_MONITOR(self)->name;
}

static EebusError Configure(
    GcpMgcpMonitorObject* self,
    MeasurementServer* msrv,
    ElectricalConnectionServer* ecsrv,
    ElectricalConnectionIdType ec_id,
    MeasurementConstraintsListDataType* constraints
) {
  GcpMgcpMonitor* const monitor = GCP_MGCP_MONITOR(self);

  if ((msrv == NULL) || (ecsrv == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    GcpMgcpMeasurementObject* const m = (GcpMgcpMeasurementObject*)VectorGetElement(&monitor->measurements, i);

    EebusError err = GCP_MGCP_MEASUREMENT_CONFIGURE(m, msrv, ecsrv, ec_id);
    if (err != kEebusErrorOk) {
      return err;
    }

    const MeasurementConstraintsDataType* const c = GCP_MGCP_MEASUREMENT_GET_CONSTRAINTS(m);
    if (c != NULL) {
      err = MeasurementConstraintsAdd(constraints, c);
      if (err != kEebusErrorOk) {
        return err;
      }
    }
  }

  return kEebusErrorOk;
}

static GcpMgcpMeasurementObject* GetMeasurement(const GcpMgcpMonitorObject* self, GcpMeasurementNameId name_id) {
  const GcpMgcpMonitor* const monitor = GCP_MGCP_MONITOR(self);
  const GcpMonitorNameId monitor_name = (GcpMonitorNameId)((uint8_t)name_id & (uint8_t)kGcpMonitorNameIdMask);

  if (monitor->name != monitor_name) {
    return NULL;
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    GcpMgcpMeasurementObject* const m = (GcpMgcpMeasurementObject*)VectorGetElement(&monitor->measurements, i);
    if (GCP_MGCP_MEASUREMENT_GET_NAME(m) == name_id) {
      return m;
    }
  }

  return NULL;
}

static EebusError FlushMeasurementCache(GcpMgcpMonitorObject* self, MeasurementListDataType* list) {
  GcpMgcpMonitor* const monitor = GCP_MGCP_MONITOR(self);

  if (list == NULL) {
    return kEebusErrorInputArgumentNull;
  }

  for (size_t i = 0; i < VectorGetSize(&monitor->measurements); ++i) {
    GcpMgcpMeasurementObject* const m = (GcpMgcpMeasurementObject*)VectorGetElement(&monitor->measurements, i);

    MeasurementDataType* const data = GCP_MGCP_MEASUREMENT_RELEASE_DATA_CACHE(m);
    if (data != NULL) {
      EebusError err = EebusDataListDataAppend((void***)&list->measurement_data, &list->measurement_data_size, data);
      if (err != kEebusErrorOk) {
        MeasurementDataDelete(data);
        return err;
      }
    }
  }

  return kEebusErrorOk;
}

GcpMgcpMonitorObject* GcpMgcpMonitorPowerCreate(const GcpMgcpMonitorPowerConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  // Power total uses the configured phases for the parameter description
  GcpMgcpMonitor* const monitor = (GcpMgcpMonitor*)EEBUS_MALLOC(sizeof(GcpMgcpMonitor));
  if (monitor == NULL) {
    return NULL;
  }

  if (MonitorConstruct(monitor, kGcpMonitorPower) != kEebusErrorOk) {
    GcpMgcpMonitorDelete(GCP_MGCP_MONITOR_OBJECT(monitor));
    return NULL;
  }

  // Create the total power measurement with the configured phases
  GcpMgcpMeasurementObject* const m = GcpMgcpMeasurementPowerTotalCreate(cfg->phases, &cfg->power_total_cfg);
  if (m == NULL) {
    GcpMgcpMonitorDelete(GCP_MGCP_MONITOR_OBJECT(monitor));
    return NULL;
  }

  VectorPushBack(&monitor->measurements, m);
  return GCP_MGCP_MONITOR_OBJECT(monitor);
}

GcpMgcpMonitorObject* GcpMgcpMonitorEnergyCreate(const GcpMgcpMonitorEnergyConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }
  if ((cfg->energy_feed_in_cfg == NULL) && (cfg->energy_consumed_cfg == NULL)) {
    return NULL;
  }

  const MeasurementParam params[] = {
      {  kGcpEnergyFeedIn,  cfg->energy_feed_in_cfg},
      {kGcpEnergyConsumed, cfg->energy_consumed_cfg},
  };

  return MonitorCreate(kGcpMonitorEnergy, params, ARRAY_SIZE(params));
}

GcpMgcpMonitorObject* GcpMgcpMonitorCurrentCreate(const GcpMgcpMonitorCurrentConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }
  if ((cfg->current_phase_a_cfg == NULL) && (cfg->current_phase_b_cfg == NULL) && (cfg->current_phase_c_cfg == NULL)) {
    return NULL;
  }

  const MeasurementParam params[] = {
      {kGcpCurrentPhaseA, cfg->current_phase_a_cfg},
      {kGcpCurrentPhaseB, cfg->current_phase_b_cfg},
      {kGcpCurrentPhaseC, cfg->current_phase_c_cfg},
  };

  return MonitorCreate(kGcpMonitorCurrent, params, ARRAY_SIZE(params));
}

GcpMgcpMonitorObject* GcpMgcpMonitorVoltageCreate(const GcpMgcpMonitorVoltageConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  // Cross-phase voltage requires both contributing phases
  if ((cfg->voltage_phase_ab_cfg != NULL)
      && ((cfg->voltage_phase_a_cfg == NULL) || (cfg->voltage_phase_b_cfg == NULL))) {
    return NULL;
  }
  if ((cfg->voltage_phase_bc_cfg != NULL)
      && ((cfg->voltage_phase_b_cfg == NULL) || (cfg->voltage_phase_c_cfg == NULL))) {
    return NULL;
  }
  if ((cfg->voltage_phase_ac_cfg != NULL)
      && ((cfg->voltage_phase_a_cfg == NULL) || (cfg->voltage_phase_c_cfg == NULL))) {
    return NULL;
  }

  const MeasurementParam params[] = {
      { kGcpVoltagePhaseA,  cfg->voltage_phase_a_cfg},
      { kGcpVoltagePhaseB,  cfg->voltage_phase_b_cfg},
      { kGcpVoltagePhaseC,  cfg->voltage_phase_c_cfg},
      {kGcpVoltagePhaseAb, cfg->voltage_phase_ab_cfg},
      {kGcpVoltagePhaseBc, cfg->voltage_phase_bc_cfg},
      {kGcpVoltagePhaseAc, cfg->voltage_phase_ac_cfg},
  };

  return MonitorCreate(kGcpMonitorVoltage, params, ARRAY_SIZE(params));
}

GcpMgcpMonitorObject* GcpMgcpMonitorFrequencyCreate(const GcpMgcpMonitorFrequencyConfig* cfg) {
  if (cfg == NULL) {
    return NULL;
  }

  const MeasurementParam params[] = {
      {kGcpFrequency, &cfg->frequency_cfg},
  };

  return MonitorCreate(kGcpMonitorFrequency, params, ARRAY_SIZE(params));
}
