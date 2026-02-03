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
 * @brief MPC (MA and MU) type declarations, constants and utilities
 */

#ifndef SRC_USE_CASE_MODEL_MPC_TYPES_H_
#define SRC_USE_CASE_MODEL_MPC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

enum MuMpcMonitorNameId {
  kMpcMonitorPower      = 0x10,
  kMpcMonitorEnergy     = 0x20,
  kMpcMonitorCurrent    = 0x30,
  kMpcMonitorVoltage    = 0x40,
  kMpcMonitorFrequency  = 0x50,
  kMpcMonitorNameIdMask = 0xF0,
};

typedef enum MuMpcMonitorNameId MuMpcMonitorNameId;

enum MuMpcMeasurementNameId {
  kMpcPowerTotal     = kMpcMonitorPower | 0x01,
  kMpcPowerPhaseA    = kMpcMonitorPower | 0x02,
  kMpcPowerPhaseB    = kMpcMonitorPower | 0x03,
  kMpcPowerPhaseC    = kMpcMonitorPower | 0x04,
  kMpcEnergyConsumed = kMpcMonitorEnergy | 0x01,
  kMpcEnergyProduced = kMpcMonitorEnergy | 0x02,
  kMpcCurrentPhaseA  = kMpcMonitorCurrent | 0x01,
  kMpcCurrentPhaseB  = kMpcMonitorCurrent | 0x02,
  kMpcCurrentPhaseC  = kMpcMonitorCurrent | 0x03,
  kMpcVoltagePhaseA  = kMpcMonitorVoltage | 0x01,
  kMpcVoltagePhaseB  = kMpcMonitorVoltage | 0x02,
  kMpcVoltagePhaseC  = kMpcMonitorVoltage | 0x03,
  kMpcVoltagePhaseAb = kMpcMonitorVoltage | 0x04,
  kMpcVoltagePhaseBc = kMpcMonitorVoltage | 0x05,
  kMpcVoltagePhaseAc = kMpcMonitorVoltage | 0x06,
  kMpcFrequency      = kMpcMonitorFrequency | 0x01,
};

typedef enum MuMpcMeasurementNameId MuMpcMeasurementNameId;

/**
 * @brief Get the MU MPC state with the state name
 * @param name Name of the state
 * @return Pointer to the MU MPC state, or NULL if not found
 */
const MuMpcMeasurementNameId* MuMpcMeasurementGetNameId(const char* name);

/**
 * @brief Get the name of the MU MPC state
 * @param state MU MPC state
 * @return Name of the state
 */
const char* MuMpcMeasurementGetName(MuMpcMeasurementNameId state);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_MODEL_MPC_TYPES_H_
