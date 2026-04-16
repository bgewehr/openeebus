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
 * @brief GCP MGCP Monitor implementation declarations
 *
 * Create one monitor per scenario group:
 *   Scenario 2: GcpMgcpMonitorPowerCreate()
 *   Scenarios 3+4 (combined): GcpMgcpMonitorEnergyCreate()
 *   Scenario 5: GcpMgcpMonitorCurrentCreate()
 *   Scenario 6: GcpMgcpMonitorVoltageCreate()
 *   Scenario 7: GcpMgcpMonitorFrequencyCreate()
 *
 * Delete with GcpMgcpMonitorDelete().
 */

#ifndef SRC_USE_CASE_ACTOR_GCP_MGCP_GCP_MGCP_MONITOR_H_
#define SRC_USE_CASE_ACTOR_GCP_MGCP_GCP_MGCP_MONITOR_H_

#include "src/common/eebus_malloc.h"
#include "src/use_case/actor/gcp/mgcp/gcp_mgcp_measurement.h"
#include "src/use_case/api/gcp_mgcp_monitor_interface.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* ---- per-scenario config structs ---- */

/**
 * @brief Scenario 2: total active power
 */
typedef struct GcpMgcpMonitorPowerConfig GcpMgcpMonitorPowerConfig;
struct GcpMgcpMonitorPowerConfig {
  /** Phases connected to this meter (e.g. kElectricalConnectionPhaseNameTypeAbc for 3-phase) */
  ElectricalConnectionPhaseNameType phases;
  GcpMgcpMeasurementConfig power_total_cfg;
};

/**
 * @brief Scenarios 3 and 4: grid energy (feed-in and/or consumed)
 *
 * At least one of the two pointers MUST be non-NULL.
 */
typedef struct GcpMgcpMonitorEnergyConfig GcpMgcpMonitorEnergyConfig;
struct GcpMgcpMonitorEnergyConfig {
  /** Scenario 3: grid feed-in energy; set NULL if not supported */
  const GcpMgcpMeasurementConfig* energy_feed_in_cfg;
  /** Scenario 4: grid consumed energy; set NULL if not supported */
  const GcpMgcpMeasurementConfig* energy_consumed_cfg;
};

/**
 * @brief Scenario 5: per-phase AC current
 *
 * At least one phase pointer MUST be non-NULL.
 */
typedef struct GcpMgcpMonitorCurrentConfig GcpMgcpMonitorCurrentConfig;
struct GcpMgcpMonitorCurrentConfig {
  const GcpMgcpMeasurementConfig* current_phase_a_cfg;
  const GcpMgcpMeasurementConfig* current_phase_b_cfg;
  const GcpMgcpMeasurementConfig* current_phase_c_cfg;
};

/**
 * @brief Scenario 6: per-phase AC voltage
 *
 * Phase-to-phase entries (AB/BC/AC) may only be non-NULL when both related
 * phase-to-neutral entries are also non-NULL.
 */
typedef struct GcpMgcpMonitorVoltageConfig GcpMgcpMonitorVoltageConfig;
struct GcpMgcpMonitorVoltageConfig {
  const GcpMgcpMeasurementConfig* voltage_phase_a_cfg;
  const GcpMgcpMeasurementConfig* voltage_phase_b_cfg;
  const GcpMgcpMeasurementConfig* voltage_phase_c_cfg;
  const GcpMgcpMeasurementConfig* voltage_phase_ab_cfg;
  const GcpMgcpMeasurementConfig* voltage_phase_bc_cfg;
  const GcpMgcpMeasurementConfig* voltage_phase_ac_cfg;
};

/**
 * @brief Scenario 7: AC frequency
 */
typedef struct GcpMgcpMonitorFrequencyConfig GcpMgcpMonitorFrequencyConfig;
struct GcpMgcpMonitorFrequencyConfig {
  GcpMgcpMeasurementConfig frequency_cfg;
};

/* ---- factory functions ---- */

GcpMgcpMonitorObject* GcpMgcpMonitorPowerCreate(const GcpMgcpMonitorPowerConfig* cfg);
GcpMgcpMonitorObject* GcpMgcpMonitorEnergyCreate(const GcpMgcpMonitorEnergyConfig* cfg);
GcpMgcpMonitorObject* GcpMgcpMonitorCurrentCreate(const GcpMgcpMonitorCurrentConfig* cfg);
GcpMgcpMonitorObject* GcpMgcpMonitorVoltageCreate(const GcpMgcpMonitorVoltageConfig* cfg);
GcpMgcpMonitorObject* GcpMgcpMonitorFrequencyCreate(const GcpMgcpMonitorFrequencyConfig* cfg);

static inline void GcpMgcpMonitorDelete(GcpMgcpMonitorObject* monitor) {
  if (monitor != NULL) {
    GCP_MGCP_MONITOR_DESTRUCT(monitor);
    EEBUS_FREE(monitor);
  }
}

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_ACTOR_GCP_MGCP_GCP_MGCP_MONITOR_H_
