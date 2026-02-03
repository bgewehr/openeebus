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
 * @brief Scaled Value declarations and utilities
 */

#ifndef SRC_USE_CASE_MODEL_SCALED_VALUE_H_
#define SRC_USE_CASE_MODEL_SCALED_VALUE_H_

#include <stdint.h>

#include "src/common/eebus_errors.h"
#include "src/spine/model/common_data_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Scaled Value is similar to SPINE ScaledNumber but uses values instead of pointers internally.
 * Transformation of Scaled Value to folating point number is: n = value * 10 ^ scale
 */
typedef struct ScaledValue ScaledValue;

/**
 * @brief Scaled Value structure
 */
struct ScaledValue {
  int64_t value; /**< Value */
  int8_t scale;  /**< Scale */
};

/**
 * @brief Initialize Scaled Value with Scaled Number
 * @param scaled_value Pointer to Scaled Value to initialize
 * @param scaled_number Pointer to Scaled Number to initialize from
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError ScaledValueInitWithScaledNumber(ScaledValue* scaled_value, const ScaledNumberType* scaled_number);

/**
 * @brief Convert Scaled Value to string representation
 * @param scaled_value Pointer to Scaled Value to convert
 * @return Pointer to string representation, NULL on error.
 * Return value must be deleted with StringDelete() after being used.
 */
const char* ScaledValueToString(const ScaledValue* scaled_value);

/**
 * @brief Print Scaled Value using specified format
 * @param fmt Format string to use for printing.
 * Must not be null and shall contain a single %s format specifier
 * @param scaled_value Pointer to Scaled Value to print
 */
void ScaledValuePrint(const char* fmt, const ScaledValue* scaled_value);

/**
 * @brief Parse Scaled Value from string representation
 * @param s String representation to parse
 * @param scaled_value Pointer to Scaled Value to store the result
 * @return kEebusErrorOk on success, error code otherwise
 */
EebusError ScaledValueParse(const char* s, ScaledValue* scaled_value);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_USE_CASE_MODEL_SCALED_VALUE_H_
