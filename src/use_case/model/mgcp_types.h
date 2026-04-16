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
 * @brief MGCP (MA and GCP) type declarations, constants and utilities
 */

#ifndef SRC_USE_CASE_MODEL_MGCP_TYPES_H_
#define SRC_USE_CASE_MODEL_MGCP_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief MGCP Monitor group identifiers (high nibble of GcpMeasurementNameId)
 */
enum GcpMonitorNameId {
  kGcpMonitorPower      = 0x10, /**< Scenario 2: momentary power */
  kGcpMonitorEnergy     = 0x20, /**< Scenarios 3 and 4: energy feed-in / consumed */
  kGcpMonitorCurrent    = 0x30, /**< Scenario 5: per-phase AC current */
  kGcpMonitorVoltage    = 0x40, /**< Scenario 6: per-phase AC voltage */
  kGcpMonitorFrequency  = 0x50, /**< Scenario 7: AC frequency */
  kGcpMonitorNameIdMask = 0xF0,
};

typedef enum GcpMonitorNameId GcpMonitorNameId;

/**
 * @brief MGCP measurement name identifiers
 *
 * High nibble identifies the monitor group; low nibble identifies the specific
 * measurement within that group.
 */
enum GcpMeasurementNameId {
  /* Scenario 2 — momentary power */
  kGcpPowerTotal = kGcpMonitorPower | 0x01, /**< Total active power (W) */
  /* Scenario 3 — grid feed-in energy */
  kGcpEnergyFeedIn = kGcpMonitorEnergy | 0x01, /**< Cumulative feed-in energy (Wh) */
  /* Scenario 4 — grid consumed energy */
  kGcpEnergyConsumed = kGcpMonitorEnergy | 0x02, /**< Cumulative consumed energy (Wh) */
  /* Scenario 5 — per-phase AC current */
  kGcpCurrentPhaseA = kGcpMonitorCurrent | 0x01, /**< Phase A RMS current (A) */
  kGcpCurrentPhaseB = kGcpMonitorCurrent | 0x02, /**< Phase B RMS current (A) */
  kGcpCurrentPhaseC = kGcpMonitorCurrent | 0x03, /**< Phase C RMS current (A) */
  /* Scenario 6 — per-phase AC voltage */
  kGcpVoltagePhaseA  = kGcpMonitorVoltage | 0x01, /**< Phase A to neutral RMS voltage (V) */
  kGcpVoltagePhaseB  = kGcpMonitorVoltage | 0x02, /**< Phase B to neutral RMS voltage (V) */
  kGcpVoltagePhaseC  = kGcpMonitorVoltage | 0x03, /**< Phase C to neutral RMS voltage (V) */
  kGcpVoltagePhaseAb = kGcpMonitorVoltage | 0x04, /**< Phase A to B RMS voltage (V) */
  kGcpVoltagePhaseBc = kGcpMonitorVoltage | 0x05, /**< Phase B to C RMS voltage (V) */
  kGcpVoltagePhaseAc = kGcpMonitorVoltage | 0x06, /**< Phase C to A RMS voltage (V) */
  /* Scenario 7 — AC frequency */
  kGcpFrequency = kGcpMonitorFrequency | 0x01, /**< Grid frequency (Hz) */
};

typedef enum GcpMeasurementNameId GcpMeasurementNameId;

/**
 * @brief Look up a GcpMeasurementNameId by its string name
 * @param name String name of the measurement (e.g. "power_total", "current_phase_a")
 * @return Pointer to the matching GcpMeasurementNameId, or NULL if not found
 */
const GcpMeasurementNameId* GcpMgcpMeasurementGetNameId(const char* name);

/**
 * @brief Get the string name of a GcpMeasurementNameId
 * @param id The measurement name identifier
 * @return String name, or NULL if the identifier is unknown
 */
const char* GcpMgcpMeasurementGetName(GcpMeasurementNameId id);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_MODEL_MGCP_TYPES_H_
