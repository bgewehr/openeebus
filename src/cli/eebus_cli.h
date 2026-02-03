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
 * @brief EEBUS CLI implementation
 *
 * Usage example:
 *
 * @code{.c}
 * EebusCli* const cli = EebusCliCreate();
 * EebusCliSetEgLpc(cli, eg_lpc_use_case);
 * // Consider EG LPC connection is established and remote entity address is already known
 * EebusCliSetRemoteEntityAddress(cli, remote_entity_address);
 * // Set power limit to 3500.5W for 12 hours, 7 seconds and activate it
 * EebusCliHandleCmd(cli, "eg_lpc set power_limit 3500.5 PT12H07S true");
 * @endcode
 *
 * Commands examples
 * EG LPC:
 * eg_lpc set power_limit 3500.5 PT12H true
 * eg_lpc get power_limit
 * eg_lpc set failsafe_limit 3500.5
 * eg_lpc get failsafe_limit
 * eg_lpc set failsafe_duration PT3H02M3S
 * eg_lpc start heartbeat
 * eg_lpc stop heartbeat
 *
 * MA MPC:
 * ma_mpc get power_total
 * ma_mpc get power_phase_a
 * ma_mpc get power_phase_b
 * ma_mpc get power_phase_c
 * ma_mpc get energy_consumed
 * ma_mpc get energy_produced
 * ma_mpc get current_phasea
 * ma_mpc get current_phaseb
 * ma_mpc get current_phasec
 * ma_mpc get voltage_phase_a
 * ma_mpc get voltage_phase_b
 * ma_mpc get voltage_phase_c
 * ma_mpc get voltage_phase_ab
 * ma_mpc get voltage_phase_bc
 * ma_mpc get voltage_phase_ac
 * ma_mpc get frequency
 */

#ifndef SRC_CLI_EEBUS_CLI_H_
#define SRC_CLI_EEBUS_CLI_H_

#include <stddef.h>

#include "src/cli/eebus_cli_interface.h"
#include "src/common/eebus_malloc.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Dynamically allocate and construct the EEBUS CLI handler instance
 * @return Pointer to the EEBUS CLI handler instance or NULL on failure
 */
EebusCliObject* EebusCliCreate(void);

/**
 * @brief Deallocate the EEBUS CLI handler instance
 * @param eebus_cli Pointer to the EEBUS CLI handler instance to be deallocated
 */
static inline void EebusCliDelete(EebusCliObject* eebus_cli) {
  if (eebus_cli != NULL) {
    EEBUS_CLI_DESTRUCT(eebus_cli);
    EEBUS_FREE(eebus_cli);
  }
}

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_CLI_EEBUS_CLI_H_
