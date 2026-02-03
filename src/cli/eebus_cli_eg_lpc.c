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
 * @brief EEBUS CLI EG LPC commands handling implementation
 */

#include <inttypes.h>
#include <stdio.h>

#include "src/cli/eebus_cli_internal.h"
#include "src/common/string_util.h"
#include "src/use_case/model/scaled_value.h"

static void EebusCliHandleCmdEgLpcGetPowerLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcGetFailsafeLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void
EebusCliHandleCmdEgLpcGetFailsafeDuration(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcGet(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcSetPowerLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcSetFailsafeLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void
EebusCliHandleCmdEgLpcSetFailsafeDuration(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcSet(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcStart(const EebusCli* self, const char* const* tokens, size_t num_tokens);
static void EebusCliHandleCmdEgLpcStop(const EebusCli* self, const char* const* tokens, size_t num_tokens);

//-------------------------------------------------------------------------------------------//
//
// EG LPC Getters Handling
//
//-------------------------------------------------------------------------------------------//
void EebusCliHandleCmdEgLpcGetPowerLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  // Example:
  // eg_lpc get power_limit
  LoadLimit limit = {0};
  if (EgLpcGetActivePowerConsumptionLimit(self->eg_lpc, self->eg_lpc_entity_addr, &limit) != kEebusErrorOk) {
    printf("Getting power limit failed\n");
    return;
  }

  const char* const value_str = ScaledValueToString(&limit.value);
  if (value_str == NULL) {
    printf("Converting power limit to string failed\n");
    return;
  }

  printf(
      "Power Limit: value=%s, duration=%" PRId32 "h, active=%s\n",
      value_str,
      limit.duration.hours,
      limit.is_active ? "true" : "false"
  );

  StringDelete((char*)value_str);
}

void EebusCliHandleCmdEgLpcGetFailsafeLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  ScaledValue power_limit = {0};

  if (EgLpcGetFailsafeConsumptionActivePowerLimit(self->eg_lpc, self->eg_lpc_entity_addr, &power_limit)
      != kEebusErrorOk) {
    printf("Getting failsafe limit failed\n");
    return;
  }

  const char* const value_str = ScaledValueToString(&power_limit);
  if (value_str == NULL) {
    printf("Converting failsafe limit to string failed\n");
    return;
  }

  printf("Failsafe Consumption Active Power Limit: value=%s\n", value_str);

  StringDelete((char*)value_str);
}

void EebusCliHandleCmdEgLpcGetFailsafeDuration(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  DurationType duration = {0};

  if (num_tokens != 3) {
    printf("Insufficient arguments for eg_lpc get failsafe_duration command\n");
    return;
  }

  if (EgLpcGetFailsafeDurationMinimum(self->eg_lpc, self->eg_lpc_entity_addr, &duration) != kEebusErrorOk) {
    printf("Getting failsafe duration failed\n");
    return;
  }

  EebusDurationPrint("Failsafe Duration Minimum: %s\n", &duration);
}

void EebusCliHandleCmdEgLpcGet(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  if (num_tokens != 3) {
    printf("Insufficient arguments for eg_lpc get command\n");
    return;
  }

  const char* subcommand = tokens[2];
  if (strcmp(subcommand, "power_limit") == 0) {
    EebusCliHandleCmdEgLpcGetPowerLimit(self, tokens, num_tokens);
  } else if (strcmp(subcommand, "failsafe_limit") == 0) {
    EebusCliHandleCmdEgLpcGetFailsafeLimit(self, tokens, num_tokens);
  } else if (strcmp(subcommand, "failsafe_duration") == 0) {
    EebusCliHandleCmdEgLpcGetFailsafeDuration(self, tokens, num_tokens);
  } else {
    printf("Unknown subcommand for eg_lpc get: %s\n", subcommand);
  }
}

//-------------------------------------------------------------------------------------------//
//
// EG LPC Setters Handling
//
//-------------------------------------------------------------------------------------------//
void EebusCliHandleCmdEgLpcSetPowerLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  // Example:
  if (num_tokens != 6) {
    printf("Insufficient arguments for eg_lpc set power_limit command\n");
    return;
  }

  LoadLimit limit = {0};

  if (ScaledValueParse(tokens[3], &limit.value) != kEebusErrorOk) {
    printf("Invalid limit value: %s\n", tokens[3]);
    return;
  }

  if (EebusDurationParse(tokens[4], &limit.duration) != kEebusErrorOk) {
    printf("Invalid duration value: %s\n", tokens[4]);
    return;
  }

  if (strcmp(tokens[5], "true") == 0) {
    limit.is_active = true;
  } else if (strcmp(tokens[5], "false") == 0) {
    limit.is_active = false;
  } else {
    printf("Invalid active flag value: %s\n", tokens[5]);
    return;
  }

  if (EgLpcSetActivePowerConsumptionLimit(self->eg_lpc, self->eg_lpc_entity_addr, &limit) != kEebusErrorOk) {
    printf("Setting power limit failed\n");
  }
}

void EebusCliHandleCmdEgLpcSetFailsafeLimit(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  ScaledValue power_limit = {0};

  if (num_tokens != 4) {
    printf("Insufficient arguments for eg_lpc set failsafe_limit command\n");
    return;
  }

  if (ScaledValueParse(tokens[3], &power_limit) != kEebusErrorOk) {
    printf("Invalid value for failsafe_limit: %s\n", tokens[3]);
    return;
  }

  const EntityAddressType* entity_addr = self->eg_lpc_entity_addr;
  if (EgLpcSetFailsafeConsumptionActivePowerLimit(self->eg_lpc, entity_addr, &power_limit) != kEebusErrorOk) {
    printf("Setting failsafe limit failed\n");
  }
}

void EebusCliHandleCmdEgLpcSetFailsafeDuration(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  DurationType duration = {0};

  // eg_lpc set failsafe_duration 2
  if (num_tokens != 4) {
    printf("Insufficient arguments for eg_lpc set failsafe_duration command\n");
    return;
  }

  if (EebusDurationParse(tokens[3], &duration) != kEebusErrorOk) {
    printf("Invalid value for failsafe_duration: %s\n", tokens[3]);
    return;
  }

  if (EgLpcSetFailsafeDurationMinimum(self->eg_lpc, self->eg_lpc_entity_addr, &duration) != kEebusErrorOk) {
    printf("Setting failsafe duration failed\n");
  }
}

void EebusCliHandleCmdEgLpcSet(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  if (num_tokens < 3) {
    printf("Insufficient arguments for eg_lpc set command\n");
    return;
  }

  const char* subcommand = tokens[2];
  if (strcmp(subcommand, "power_limit") == 0) {
    EebusCliHandleCmdEgLpcSetPowerLimit(self, tokens, num_tokens);
  } else if (strcmp(subcommand, "failsafe_limit") == 0) {
    EebusCliHandleCmdEgLpcSetFailsafeLimit(self, tokens, num_tokens);
  } else if (strcmp(subcommand, "failsafe_duration") == 0) {
    EebusCliHandleCmdEgLpcSetFailsafeDuration(self, tokens, num_tokens);
  } else {
    printf("Unknown subcommand for eg_lpc set: %s\n", subcommand);
  }
}

//-------------------------------------------------------------------------------------------//
//
// EG LPC Start/Stop Handling
//
//-------------------------------------------------------------------------------------------//
void EebusCliHandleCmdEgLpcStart(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  const char* const subcommand = tokens[2];

  if (strcmp(subcommand, "heartbeat") == 0) {
    EgLpcStartHeartbeat(self->eg_lpc);
    printf("EG LPC Heartbeat started\n");
  } else {
    printf("Unknown subcommand for eg_lpc start: %s\n", subcommand);
  }
}

void EebusCliHandleCmdEgLpcStop(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  const char* const subcommand = tokens[2];

  if (strcmp(subcommand, "heartbeat") == 0) {
    EgLpcStopHeartbeat(self->eg_lpc);
    printf("EG LPC Heartbeat stopped\n");
  } else {
    printf("Unknown subcommand for eg_lpc stop: %s\n", subcommand);
  }
}

void EebusCliHandleCmdEgLpc(const EebusCli* self, const char* const* tokens, size_t num_tokens) {
  if (num_tokens < 2) {
    printf("Insufficient arguments for eg_lpc command\n");
    return;
  }

  if (self->eg_lpc == NULL) {
    printf("EG LPC Use Case not set in CLI handler\n");
    return;
  }

  if (strcmp(tokens[1], "set") == 0) {
    EebusCliHandleCmdEgLpcSet(self, tokens, num_tokens);
  } else if (strcmp(tokens[1], "get") == 0) {
    EebusCliHandleCmdEgLpcGet(self, tokens, num_tokens);
  } else if (strcmp(tokens[1], "start") == 0) {
    EebusCliHandleCmdEgLpcStart(self, tokens, num_tokens);
  } else if (strcmp(tokens[1], "stop") == 0) {
    EebusCliHandleCmdEgLpcStop(self, tokens, num_tokens);
  } else {
    printf("Unknown subcommand for eg_lpc: %s\n", tokens[1]);
  }
}
