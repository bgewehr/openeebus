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
 * @brief mDNS debug logging specific functionality declarations
 */

#ifndef SRC_SHIP_MDNS_MDNS_DEBUG_H_
#define SRC_SHIP_MDNS_MDNS_DEBUG_H_

#include <stdint.h>

/** Set MDNS_DEBUG 1 to enable debug prints */
#ifndef MDNS_DEBUG
#define MDNS_DEBUG 0
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Print every key=value pair from a raw DNS-SD TXT record to the debug output.
 *
 * The TXT record uses the length-prefixed wire format: each entry is a 1-byte
 * length field followed by that many bytes of "key=value" text, as returned by
 * DNSServiceResolve() or the ESP-IDF mdns API.
 *
 * @param txt_record      Pointer to the first length byte of the TXT record.
 *                        Passing NULL is safe — prints a single "(null)" line.
 * @param txt_record_size Total byte length of the TXT record buffer.
 *                        When 0, the function walks entries until a zero-length
 *                        byte is encountered (use only when the buffer is
 *                        null-terminated by a zero-length sentinel).
 */
void MdnsTxtRecordPrint(const uint8_t* txt_record, uint16_t txt_record_size);

#if MDNS_DEBUG
#define MDNS_TXT_RECORD_PRINT(txt_record, size) MdnsTxtRecordPrint((txt_record), (size))
#else
#define MDNS_TXT_RECORD_PRINT(txt_record, size) ((void)(txt_record))
#endif  // MDNS_DEBUG

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SRC_SHIP_MDNS_MDNS_DEBUG_H_
