/*
 * Copyright 2025 bgewehr (bg-hems branch)
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

#include "eebus_lpc.h"

#include <cmath>
#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/application.h"

// NVS for certificate persistence
#include <nvs_flash.h>
#include <nvs.h>

// mbedTLS for self-signed cert generation
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/pk.h>
#include <mbedtls/oid.h>
#include <mbedtls/error.h>

// openeebus C API
extern "C" {
#include "src/service/service/eebus_service.h"
#include "src/service/service/eebus_service_config.h"
#include "src/common/eebus_device_info.h"
#include "src/ship/tls_certificate/tls_certificate.h"
#include "src/use_case/actor/cs/lpc/cs_lpc.h"
}

// ESP32 WebSocket port
#include "port/esp32/websocket/websocket_server_esp32.h"

namespace esphome {
namespace eebus_lpc {

static const char* NVS_NAMESPACE  = "eebus";
static const char* NVS_KEY_CERT   = "cert_der";
static const char* NVS_KEY_KEY    = "key_der";

/* SHIP default port */
static const uint16_t kShipDefaultPort = 4712;

/* Heartbeat watchdog: if no heartbeat within 60 s, apply failsafe */
static const uint32_t kHeartbeatTimeoutMs = 60000;

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */

void EebusLpcComponent::setup() {
  ESP_LOGI(TAG, "Setting up EEBus LPC component (§14a EnWG)");

  /* Certificate: load from NVS or generate new */
  uint8_t* cert_der = nullptr;
  size_t   cert_len = 0;
  uint8_t* key_der  = nullptr;
  size_t   key_len  = 0;

  if (!load_cert_nvs_(&cert_der, &cert_len, &key_der, &key_len)) {
    ESP_LOGI(TAG, "No cert in NVS, generating self-signed certificate...");
    if (!load_or_generate_cert_()) {
      ESP_LOGE(TAG, "Certificate generation failed — EEBus LPC disabled");
      this->mark_failed();
      return;
    }
    /* Reload from NVS after generation */
    if (!load_cert_nvs_(&cert_der, &cert_len, &key_der, &key_len)) {
      ESP_LOGE(TAG, "Failed to reload cert from NVS");
      this->mark_failed();
      return;
    }
  }

  ESP_LOGI(TAG, "Certificate loaded (DER %u bytes)", (unsigned)cert_len);

  /* Start openeebus SHIP service */
  if (!start_eebus_service_(cert_der, cert_len, key_der, key_len)) {
    ESP_LOGE(TAG, "Failed to start openeebus service");
    free(cert_der);
    free(key_der);
    this->mark_failed();
    return;
  }

  free(cert_der);
  free(key_der);

  ESP_LOGI(TAG, "EEBus LPC ready on port %d, SKI configured: %s",
           ship_port_, remote_ski_.empty() ? "auto-accept" : remote_ski_.c_str());
}

/* -------------------------------------------------------------------------
 * loop() — heartbeat watchdog
 * ---------------------------------------------------------------------- */

void EebusLpcComponent::loop() {
  if (service_ == nullptr) return;

  uint32_t now = millis();

  /* Heartbeat watchdog: apply failsafe if heartbeat lost */
  if (limit_active_ && !heartbeat_lost_) {
    if ((now - last_heartbeat_ms_) > kHeartbeatTimeoutMs) {
      ESP_LOGW(TAG, "EEBus heartbeat lost — applying failsafe %.0f W", failsafe_limit_w_);
      heartbeat_lost_ = true;
      on_power_limit_receive(failsafe_limit_w_, true);
    }
  }
}

/* -------------------------------------------------------------------------
 * dump_config()
 * ---------------------------------------------------------------------- */

void EebusLpcComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EEBus LPC Component:");
  ESP_LOGCONFIG(TAG, "  SHIP Port:       %d", ship_port_);
  ESP_LOGCONFIG(TAG, "  Remote SKI:      %s",
                remote_ski_.empty() ? "(auto-accept)" : remote_ski_.c_str());
  ESP_LOGCONFIG(TAG, "  Device:          %s / %s / %s",
                device_brand_.c_str(), device_type_.c_str(), device_model_.c_str());
  ESP_LOGCONFIG(TAG, "  Failsafe limit:  %.0f W", failsafe_limit_w_);
}

/* -------------------------------------------------------------------------
 * Callbacks from openeebus LPC listener
 * ---------------------------------------------------------------------- */

void EebusLpcComponent::on_power_limit_receive(float limit_w, bool is_active) {
  ESP_LOGI(TAG, "LPC limit received: %.0f W, active=%s",
           limit_w, is_active ? "yes" : "no");

  current_limit_w_ = limit_w;
  heartbeat_lost_  = false;
  last_heartbeat_ms_ = millis();

  if (is_active && !limit_active_) {
    limit_active_ = true;
    for (auto* t : limit_active_triggers_) {
      t->trigger(limit_w);
    }
  } else if (!is_active && limit_active_) {
    limit_active_    = false;
    current_limit_w_ = 0.0f;
    for (auto* t : limit_cleared_triggers_) {
      t->trigger();
    }
  } else if (is_active) {
    /* Limit updated while already active — re-fire with new value */
    for (auto* t : limit_active_triggers_) {
      t->trigger(limit_w);
    }
  }
}

void EebusLpcComponent::on_failsafe_limit_receive(float limit_w) {
  ESP_LOGW(TAG, "Failsafe limit received: %.0f W", limit_w);
  failsafe_limit_w_ = limit_w;
}

void EebusLpcComponent::on_heartbeat_receive(uint64_t counter) {
  last_heartbeat_ms_ = millis();
  heartbeat_lost_    = false;
  ESP_LOGV(TAG, "Heartbeat %" PRIu64, counter);
}

/* -------------------------------------------------------------------------
 * Certificate generation (mbedTLS self-signed)
 * ---------------------------------------------------------------------- */

bool EebusLpcComponent::load_or_generate_cert_() {
  mbedtls_pk_context        pk;
  mbedtls_x509write_cert    cert;
  mbedtls_entropy_context   entropy;
  mbedtls_ctr_drbg_context  ctr_drbg;
  mbedtls_mpi               serial;

  mbedtls_pk_init(&pk);
  mbedtls_x509write_crt_init(&cert);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_mpi_init(&serial);

  bool ok = false;
  uint8_t* cert_der = nullptr;
  uint8_t* key_der  = nullptr;

  do {
    /* Seed RNG */
    const char* pers = "eebus_hems";
    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char*)pers, strlen(pers)) != 0) {
      ESP_LOGE(TAG, "ctr_drbg_seed failed");
      break;
    }

    /* Generate EC key (P-256) */
    if (mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
        mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk),
                            mbedtls_ctr_drbg_random, &ctr_drbg) != 0) {
      ESP_LOGE(TAG, "EC key generation failed");
      break;
    }

    /* Write key to DER */
    uint8_t key_buf[2048];
    int key_len = mbedtls_pk_write_key_der(&pk, key_buf, sizeof(key_buf));
    if (key_len <= 0) {
      ESP_LOGE(TAG, "pk_write_key_der failed: %d", key_len);
      break;
    }
    /* mbedtls writes at end of buffer */
    uint8_t* key_p = key_buf + sizeof(key_buf) - key_len;

    /* Build self-signed certificate */
    std::string subject = "CN=ESP32-HEMS-14a,O=DIY,C=DE";
    mbedtls_x509write_crt_set_subject_key(&cert, &pk);
    mbedtls_x509write_crt_set_issuer_key(&cert, &pk);
    mbedtls_x509write_crt_set_subject_name(&cert, subject.c_str());
    mbedtls_x509write_crt_set_issuer_name(&cert, subject.c_str());
    mbedtls_x509write_crt_set_version(&cert, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&cert, MBEDTLS_MD_SHA256);

    mbedtls_mpi_read_string(&serial, 10, "1");
    mbedtls_x509write_crt_set_serial(&cert, &serial);

    /* Valid for 10 years */
    mbedtls_x509write_crt_set_validity(&cert, "20250101000000", "20350101000000");

    /* Subject Key Identifier extension (required by SHIP spec) */
    mbedtls_x509write_crt_set_subject_key_identifier(&cert);
    mbedtls_x509write_crt_set_authority_key_identifier(&cert);

    uint8_t cert_buf[4096];
    int cert_len = mbedtls_x509write_crt_der(&cert, cert_buf, sizeof(cert_buf),
                                              mbedtls_ctr_drbg_random, &ctr_drbg);
    if (cert_len <= 0) {
      ESP_LOGE(TAG, "x509write_crt_der failed: %d", cert_len);
      break;
    }
    uint8_t* cert_p = cert_buf + sizeof(cert_buf) - cert_len;

    /* Store to NVS */
    if (!store_cert_nvs_(cert_p, (size_t)cert_len, key_p, (size_t)key_len)) {
      ESP_LOGE(TAG, "Failed to store cert in NVS");
      break;
    }

    ESP_LOGI(TAG, "Self-signed cert generated and stored (cert=%d B, key=%d B)",
             cert_len, key_len);
    ok = true;
  } while (false);

  mbedtls_pk_free(&pk);
  mbedtls_x509write_crt_free(&cert);
  mbedtls_entropy_free(&entropy);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_mpi_free(&serial);
  return ok;
}

bool EebusLpcComponent::store_cert_nvs_(
    const uint8_t* cert, size_t cert_len,
    const uint8_t* key,  size_t key_len)
{
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = (nvs_set_blob(h, NVS_KEY_CERT, cert, cert_len) == ESP_OK) &&
            (nvs_set_blob(h, NVS_KEY_KEY,  key,  key_len)  == ESP_OK) &&
            (nvs_commit(h) == ESP_OK);
  nvs_close(h);
  return ok;
}

bool EebusLpcComponent::load_cert_nvs_(
    uint8_t** cert, size_t* cert_len,
    uint8_t** key,  size_t* key_len)
{
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;

  size_t cl = 0, kl = 0;
  bool ok = false;

  do {
    if (nvs_get_blob(h, NVS_KEY_CERT, nullptr, &cl) != ESP_OK || cl == 0) break;
    if (nvs_get_blob(h, NVS_KEY_KEY,  nullptr, &kl) != ESP_OK || kl == 0) break;

    *cert = (uint8_t*)malloc(cl);
    *key  = (uint8_t*)malloc(kl);
    if (!*cert || !*key) { free(*cert); free(*key); break; }

    if (nvs_get_blob(h, NVS_KEY_CERT, *cert, &cl) != ESP_OK) break;
    if (nvs_get_blob(h, NVS_KEY_KEY,  *key,  &kl) != ESP_OK) break;

    *cert_len = cl;
    *key_len  = kl;
    ok = true;
  } while (false);

  nvs_close(h);
  if (!ok) { free(*cert); free(*key); *cert = *key = nullptr; }
  return ok;
}

/* -------------------------------------------------------------------------
 * Start openeebus SHIP service
 * ---------------------------------------------------------------------- */

bool EebusLpcComponent::start_eebus_service_(
    const uint8_t* cert_der, size_t cert_len,
    const uint8_t* key_der,  size_t key_len)
{
  /* Parse TLS certificate via openeebus mbedtls wrapper */
  TlsCertificateObject* tls_cert = TlsCertificateParseX509KeyPair(
      (const char*)cert_der, cert_len,
      (const char*)key_der,  key_len);

  if (!tls_cert) {
    ESP_LOGE(TAG, "TlsCertificateParseX509KeyPair failed");
    return false;
  }

  const char* ski = TLS_CERTIFICATE_GET_SKI(tls_cert);
  ESP_LOGI(TAG, "Local SKI: %s", ski ? ski : "(null)");

  /* Device info */
  EebusDeviceInfo device_info = {
    .brand = device_brand_.c_str(),
    .type  = device_type_.c_str(),
    .model = device_model_.c_str(),
  };

  /* Create WebSocket server creator (ESP32 port) */
  WebsocketCreatorObject* ws_creator = WebsocketServerCreatorEsp32Create(
      ship_port_,
      cert_der, cert_len,
      key_der,  key_len);

  if (!ws_creator) {
    ESP_LOGE(TAG, "WebsocketServerCreatorEsp32Create failed");
    TLS_CERTIFICATE_DESTRUCT(tls_cert);
    return false;
  }

  /* Build service config */
  EebusServiceConfig cfg;
  EebusServiceConfigInit(&cfg);
  cfg.tls_certificate        = tls_cert;
  cfg.device_info            = &device_info;
  cfg.websocket_creator      = ws_creator;
  cfg.port                   = ship_port_;
  cfg.remote_ski             = remote_ski_.empty() ? nullptr : remote_ski_.c_str();

  /* Create service */
  service_ = EebusServiceCreate(&cfg);
  if (!service_) {
    ESP_LOGE(TAG, "EebusServiceCreate failed");
    return false;
  }

  /* Create CS LPC use case actor */
  /* Initialize listener vtable */
  CS_LP_LISTENER_INTERFACE(&listener_) = &kLpcListenerMethods;
  listener_.self = this;

  cs_lpc_ = CsLpcCreate(service_, CS_LP_LISTENER_OBJECT(&listener_));
  if (!cs_lpc_) {
    ESP_LOGE(TAG, "CsLpcCreate failed");
    return false;
  }

  /* Start the service (begins SHIP handshake / mDNS announcement) */
  EebusError err = EEBUS_SERVICE_START(service_);
  if (err != kEebusErrorOk) {
    ESP_LOGE(TAG, "EebusServiceStart failed: %d", err);
    return false;
  }

  last_heartbeat_ms_ = millis();
  return true;
}

}  // namespace eebus_lpc
}  // namespace esphome
