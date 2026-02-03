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
 * @brief Scaled Value declarations and utilities implementation
 */

#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "src/common/eebus_math/eebus_math.h"
#include "src/common/string_util.h"
#include "src/spine/model/common_data_types.h"
#include "src/use_case/model/scaled_value.h"

EebusError ScaledValueInitWithScaledNumber(ScaledValue* scaled_value, const ScaledNumberType* scaled_number) {
  if ((scaled_value == NULL) || (scaled_number == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  if (scaled_number->number == NULL) {
    return kEebusErrorInputArgument;
  }

  scaled_value->value = *scaled_number->number;
  scaled_value->scale = (scaled_number->scale == NULL) ? 0 : *scaled_number->scale;

  return kEebusErrorOk;
}

const char* ScaledValueToString(const ScaledValue* scaled_value) {
  if (scaled_value == NULL) {
    return NULL;
  }

  int8_t scale_tmp = scaled_value->scale;
  if (scale_tmp < 0) {
    scale_tmp = -scale_tmp;
  }

  const int64_t power_of_ten = PowerOfTen(scale_tmp);

  int64_t value    = scaled_value->value;
  int64_t fraction = 0;

  if (scaled_value->scale < 0) {
    value    = scaled_value->value / power_of_ten;
    fraction = scaled_value->value % power_of_ten;
    if (fraction < 0) {
      fraction = -fraction;
    }
  } else if (scaled_value->scale > 0) {
    value = scaled_value->value * power_of_ten;
  }

  if (fraction == 0) {
    return StringFmtSprintf("%" PRId64, scaled_value->value);
  } else {
    if ((value == 0) && (scaled_value->value < 0)) {
      return StringFmtSprintf("-%" PRId64 ".%.*" PRId64, value, scale_tmp, fraction);
    } else {
      return StringFmtSprintf("%" PRId64 ".%.*" PRId64, value, scale_tmp, fraction);
    }
  }
}

void ScaledValuePrint(const char* fmt, const ScaledValue* scaled_value) {
  if (scaled_value == NULL) {
    printf(fmt, "NULL");
    return;
  }

  char* scaled_value_str = (char*)ScaledValueToString(scaled_value);
  if (scaled_value_str != NULL) {
    printf(fmt, scaled_value_str);
  } else {
    printf(fmt, "<error converting to string>");
  }

  StringDelete(scaled_value_str);
}

EebusError ScaledValueParse(const char* s, ScaledValue* scaled_value) {
  if ((s == NULL) || (scaled_value == NULL)) {
    return kEebusErrorInputArgumentNull;
  }

  char* end_ptr          = NULL;
  const char* p          = s;
  const bool is_negative = (*p == '-');
  if ((*p == '-') || (*p == '+')) {
    p++;
  }

  if (!isdigit((int)*p)) {
    return kEebusErrorParse;
  }

  int64_t value = strtoll(p, &end_ptr, 10);
  if (end_ptr == p) {
    return kEebusErrorParse;
  }

  int8_t scale = 0;
  if (*end_ptr == '.') {
    end_ptr++;
    const char* frac_start = end_ptr;
    while (isdigit((int)*end_ptr)) {
      --scale;
      ++end_ptr;
    }

    if ((end_ptr == frac_start) || (*end_ptr != '\0')) {
      return kEebusErrorParse;
    }

    int64_t fraction = strtoll(frac_start, NULL, 10);
    if (fraction == 0) {
      scale = 0;
    } else {
      const int64_t power_of_ten = PowerOfTen(-scale);
      value *= power_of_ten;
      value += (value >= 0) ? fraction : -fraction;
    }
  }

  if (*end_ptr != '\0') {
    return kEebusErrorParse;
  }

  if (is_negative && (value > 0)) {
    value = -value;
  }

  scaled_value->value = value;
  scaled_value->scale = scale;

  return kEebusErrorOk;
}
