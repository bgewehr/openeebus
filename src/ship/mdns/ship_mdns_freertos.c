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
 * @brief FreeRTOS specific Mdns implementation
 *
 * Supported netifs: WiFi station (WIFI_STA_DEF) and Ethernet
 * (ETH_DEF — covers W5500 SPI, RMII Ethernet, and similar drivers).
 *
 * Advertisement-only: this backend registers the SHIP service in the shared
 * ESP-IDF mDNS daemon but does not browse for remote services. The ESP-IDF
 * one-shot query API (mdns_query_async_new) carries no per-query context, so
 * a browse loop can only serve a single instance — with several EEBus
 * services on one device (e.g. CS + multiple EG instances) all but the last
 * created instance would block forever waiting for a completion signal.
 * Browsing is also not required for connection establishment: SHIP remotes
 * discover us via our advertisement and connect inbound.
 * Multiple instances are supported: each instance owns one service entry
 * (distinct instance name + port) in the shared daemon.
 */

#include "mdns.h"

#include "src/common/array_util.h"
#include "src/common/debug.h"
#include "src/common/eebus_arguments.h"
#include "src/common/eebus_device_info.h"
#include "src/ship/api/ship_mdns_interface.h"
#include "src/ship/mdns/mdns_debug.h"
#include "src/ship/mdns/ship_mdns.h"

/** mDNS debug printf(), enabled whith MDNS_DEBUG = 1 */
#if MDNS_DEBUG
#define MDNS_DEBUG_PRINTF(fmt, ...) DebugPrintf(fmt, ##__VA_ARGS__)
#else
#define MDNS_DEBUG_PRINTF(fmt, ...)
#endif  // MDNS_DEBUG

static const char* kShipServiceType     = "_ship";
static const char* kShipServiceProtocol = "_tcp";
static const char* kShipServicePath     = "/ship/";
static const char* kShipServiceTxtVer   = "1";

typedef struct Mdns Mdns;

struct Mdns {
  /** Implements the Mdns Interface */
  ShipMdnsObject obj;

  // Kept for interface compatibility; never invoked (advertisement-only backend)
  OnMdnsEntriesFoundCallback on_entries_found_cb;
  void* context;

  const char* ski;
  EebusDeviceInfo* device_info;
  const char* service_name;
  int port;
  bool autoaccept;
};

#define MDNS(obj) ((Mdns*)(obj))

static void Destruct(ShipMdnsObject* self);
static EebusError Start(ShipMdnsObject* self);
static void Stop(ShipMdnsObject* self);
static EebusError RegisterService(ShipMdnsObject* self);
static void DeregisterService(ShipMdnsObject* self);
static void SetAutoaccept(ShipMdnsObject* self, bool autoaccept);

static const ShipMdnsInterface mdns_methods = {
    .destruct           = Destruct,
    .start              = Start,
    .stop               = Stop,
    .register_service   = RegisterService,
    .deregister_service = DeregisterService,
    .set_autoaccept     = SetAutoaccept,
};

static EebusError MdnsConstruct(
    Mdns* self,
    const char* ski,
    const EebusDeviceInfo* device_info,
    const char* service_name,
    int port,
    OnMdnsEntriesFoundCallback cb,
    void* ctx
);

EebusError MdnsConstruct(
    Mdns* self,
    const char* ski,
    const EebusDeviceInfo* device_info,
    const char* service_name,
    int port,
    OnMdnsEntriesFoundCallback cb,
    void* ctx
) {
  // Override "virtual functions table"
  SHIP_MDNS_INTERFACE(self) = &mdns_methods;

  self->ski                 = StringCopy(ski);
  self->on_entries_found_cb = cb;
  self->context             = ctx;
  self->device_info         = EebusDeviceInfoCopy(device_info);
  self->service_name        = StringCopy(service_name);
  self->port                = port;
  self->autoaccept          = false;

  return kEebusErrorOk;
}

ShipMdnsObject* ShipMdnsCreate(
    const char* ski,
    const EebusDeviceInfo* device_info,
    const char* service_name,
    int port,
    OnMdnsEntriesFoundCallback cb,
    void* ctx
) {
  Mdns* const mdns = (Mdns*)EEBUS_MALLOC(sizeof(Mdns));
  if (mdns == NULL) {
    return NULL;
  }

  const EebusError err = MdnsConstruct(mdns, ski, device_info, service_name, port, cb, ctx);

  if (err != kEebusErrorOk) {
    MdnsDelete(SHIP_MDNS_OBJECT(mdns));
    return NULL;
  }

  return SHIP_MDNS_OBJECT(mdns);
}

void Destruct(ShipMdnsObject* self) {
  Mdns* const mdns = MDNS(self);

  SHIP_MDNS_STOP(self);

  EebusDeviceInfoDelete(mdns->device_info);
  mdns->device_info = NULL;

  StringDelete((char*)mdns->ski);
  mdns->ski = NULL;

  StringDelete((char*)mdns->service_name);
  mdns->service_name = NULL;
}

EebusError RegisterService(ShipMdnsObject* self) {
  Mdns* const mdns = MDNS(self);

  // Note: do not call mdns_instance_name_set() here — it sets the
  // daemon-global instance name and multiple EEBus instances (and the host
  // framework) would overwrite each other. The per-service instance name is
  // passed to mdns_service_add() below.

  const char* register_str = mdns->autoaccept ? "true" : "false";
  esp_err_t err;

  // Structure with TXT records
  mdns_txt_item_t service_txt_data[] = {
      { "txtvers",       kShipServiceTxtVer},
      {      "id",       mdns->service_name},
      {    "path",         kShipServicePath},
      {     "ski",                mdns->ski},
      {"register",             register_str},
      {   "brand", mdns->device_info->brand},
      {    "type",  mdns->device_info->type},
      {   "model", mdns->device_info->model},
  };

  // Initialize service
  err = mdns_service_add(
      mdns->service_name,
      kShipServiceType,
      kShipServiceProtocol,
      mdns->port,
      service_txt_data,
      ARRAY_SIZE(service_txt_data)
  );

  if (err != ESP_OK) {
    MDNS_DEBUG_PRINTF("mdns_service_add() failed: %d\n", err);
    return kEebusErrorInit;
  }

  return kEebusErrorOk;
}

static esp_netif_t* MdnsGetEspNetif(Mdns* self) {
  UNUSED(self);

  // Locate the active netif. Try WiFi STA first (original assumption),
  // then fall back to the default Ethernet interface (ETH_DEF) which is
  // what the W5500 SPI Ethernet driver registers. If neither key matches
  // (custom netif keys), walk all registered netifs for the first UP one.
  static const char* const kNetifKeys[] = {
      "WIFI_STA_DEF",  // WiFi station
      "ETH_DEF",       // SPI / RMII Ethernet (W5500 / LAN87xx / ...)
  };

  esp_netif_t* netif = NULL;
  for (size_t i = 0; i < ARRAY_SIZE(kNetifKeys); i++) {
    netif = esp_netif_get_handle_from_ifkey(kNetifKeys[i]);
    if (netif != NULL) {
      return netif;
    }
  }

  // Last resort: walk the netif list for any UP interface.
  netif = NULL;
  while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
    if (esp_netif_is_netif_up(netif)) {
      return netif;
    }
  }

  return netif;
}

EebusError Start(ShipMdnsObject* self) {
  Mdns* const mdns = MDNS(self);

  // mdns_init() may return ESP_ERR_INVALID_STATE when the host framework has
  // already initialized mDNS — that is fine, reuse it.
  esp_err_t err = mdns_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    MDNS_DEBUG_PRINTF("mdns_init() failed: %d\n", err);
    return kEebusErrorInit;
  }

  esp_netif_t* netif = MdnsGetEspNetif(mdns);
  if (netif == NULL) {
    MDNS_DEBUG_PRINTF("No suitable netif found (WIFI_STA_DEF, ETH_DEF, any UP)\n");
    return kEebusErrorInit;
  }

  const char* host_name = NULL;

  err = esp_netif_get_hostname(netif, &host_name);
  if ((err != ESP_OK) || (StringIsEmpty(host_name))) {
    MDNS_DEBUG_PRINTF("esp_netif_get_hostname() failed\n");
    return kEebusErrorInit;
  }

  mdns_hostname_set(host_name);

  EebusError ret = RegisterService(self);
  if (ret != kEebusErrorOk) {
    MDNS_DEBUG_PRINTF("RegisterService() returned error %d\n", ret);
    return ret;
  }

  // No browser thread: advertisement-only backend (see file header)

  return kEebusErrorOk;
}

void DeregisterService(ShipMdnsObject* self) {
  Mdns* const mdns = MDNS(self);

  // Remove only this instance's service entry. Do not call mdns_free():
  // the daemon is shared with the host framework and other EEBus instances.
  esp_err_t err
      = mdns_service_remove_for_host(mdns->service_name, kShipServiceType, kShipServiceProtocol, NULL);
  if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
    MDNS_DEBUG_PRINTF("mdns_service_remove_for_host() failed: %d\n", err);
  }
}

void Stop(ShipMdnsObject* self) {
  DeregisterService(self);
}

void SetAutoaccept(ShipMdnsObject* self, bool autoaccept) {
  Mdns* const mdns = MDNS(self);
  mdns->autoaccept = autoaccept;
}
