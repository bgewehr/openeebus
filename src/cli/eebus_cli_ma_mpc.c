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
 * @brief EEBUS CLI MA MPC commands handling
 */

#include <stdio.h>

#include "src/cli/eebus_cli_internal.h"
#include "src/common/array_util.h"
#include "src/common/string_util.h"
#include "src/use_case/model/mpc_types.h"

static void EebusCliHandleCmdMaMpcGet(const EebusCli* self, const char* const* tokens, size_t num_tokens);

//-------------------------------------------------------------------------------------------//
//
// MA MPC Getters Handling
//
//-------------------------------------------------------------------------------------------//
void EebusCliHandleCmdMaMpcGet(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  if (num_tokens != 3) {
    printf("Insufficient arguments for ma_mpc get command\n");
    return;
  }

  const char* const name = tokens[2];

  const MuMpcMeasurementNameId* name_id = MuMpcMeasurementGetNameId(name);

  if (name_id == NULL) {
    printf("Unknown measurement name for ma_mpc get: %s\n", name);
    return;
  }

  ScaledValue value = {0};
  if (MaMpcGetMeasurementData(self->ma_mpc, *name_id, self->ma_mpc_entity_addr, &value) != kEebusErrorOk) {
    printf("Getting measurement value failed\n");
    return;
  }

  const char* const value_str = ScaledValueToString(&value);
  if (value_str == NULL) {
    printf("Converting measurement to string failed\n");
    return;
  }

  printf("Measurement %s: value=%s\n", name, value_str);
  StringDelete((char*)value_str);
}

void EebusCliHandleCmdMaMpc(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  if (num_tokens < 2) {
    printf("Insufficient arguments for ma_mpc command\n");
    return;
  }

  if (self->ma_mpc == NULL) {
    printf("MA MPC Use Case not set in CLI handler\n");
    return;
  }

  if (strcmp(tokens[1], "get") == 0) {
    EebusCliHandleCmdMaMpcGet(self, tokens, num_tokens);
  } else {
    printf("Unknown subcommand for ma_mpc: %s\n", tokens[1]);
  }
}
