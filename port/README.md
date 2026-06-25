# openeebus — `bg-hems` branch

**ESP32 port + ESPHome component for §14a EnWG HEMS (EEBus LPC)**

This branch adds an ESP32/ESPHome port of the [openeebus](https://github.com/NIBEGroup/openeebus)
library (SHIP/SPINE LPC use case), enabling a DIY ESP32-based HEMS to receive
power-limit commands from a Westnetz CLS-Steuerbox via EEBus — implementing
the digital interface required by §14a EnWG.

> **Status:** Work in progress. WebSocket adapter and ESPHome component are
> implemented. Integration testing against a real CLS-Steuerbox is pending.
> The relay-contact fallback (valid until 31.12.2028) remains the
> regulatory-compliant production path until this port is validated.

---

## What this branch adds

```
port/
└── esp32/
    ├── component/
    │   ├── CMakeLists.txt          ESP-IDF component wrapper
    │   └── eebus_malloc_esp32.h    PSRAM-aware allocator (ESP32-S3)
    └── websocket/
        ├── websocket_server_esp32.c/h        httpd TLS WS server
        ├── websocket_client_esp32.c/h        esp_websocket_client
        ├── websocket_server_creator_esp32.c  openeebus creator bridge
        └── websocket_client_creator_esp32.c  openeebus creator bridge

esphome/
├── components/
│   └── eebus_lpc/
│       ├── __init__.py     YAML schema
│       ├── eebus_lpc.h     ESPHome component class + C listener vtable
│       ├── eebus_lpc.cpp   setup/loop, cert generation, trigger dispatch
│       └── CMakeLists.txt
└── hems_14a_example.yaml   Complete working YAML example
```

---

## Architecture

```
Westnetz CLS-Steuerbox
    │ EEBus LPC (SHIP/SPINE over TLS WebSocket)
    ▼
ESP32 HEMS (this port)
├── openeebus core:  SHIP/SPINE/LPC — unchanged from upstream
├── WebSocket layer: httpd + esp_websocket_client  ← only new code
├── mDNS:           ship_mdns_freertos.c            ← already in upstream
├── TLS:            tls_certificate_mbedtls.c       ← already in upstream
├── Threading:      eebus_thread_freertos.c         ← already in upstream
    │
    ├── SG-Ready DO1/DO2 → Bosch Compress 5800i WP
    └── Modbus TCP       → Shell Recharge Wallbox
```

---

## Port summary

| Upstream dependency | ESP32 replacement | Notes |
|---|---|---|
| libwebsockets server | `esp_http_server` (httpd) with WS | Only new code |
| libwebsockets client | `esp_websocket_client` | Only new code |
| mDNSResponder/Bonjour | `esp_mdns` | Already in upstream `ship_mdns_freertos.c` |
| OpenSSL | mbedTLS | Already in upstream `tls_certificate_mbedtls.c` |
| pthreads | FreeRTOS | Already in upstream `eebus_thread_freertos.c` etc. |
| cJSON | ESP-IDF `json` component | Drop-in |

---

## Usage

### As ESP-IDF component

```cmake
# In your project CMakeLists.txt
set(EXTRA_COMPONENT_DIRS
    path/to/openeebus/port/esp32/component)
```

### As ESPHome external component

```yaml
external_components:
  - source: github://bgewehr/openeebus@bg-hems
    components: [eebus_lpc]

eebus_lpc:
  ship_port: 4712
  remote_ski: "aabbccddeeff..."   # SKI of CLS-Steuerbox
  failsafe_limit_w: 4200.0

  on_limit_active:
    - then:
        - lambda: |-
            float limit_w = x;
            // Distribute to WP (SG-Ready) and Wallbox (Modbus)

  on_limit_cleared:
    - then:
        - logger.log: "Returning to normal operation"
```

See [`esphome/hems_14a_example.yaml`](esphome/hems_14a_example.yaml) for a
complete working configuration.

---

## Memory footprint (estimated, plain ESP32)

| Component | RAM |
|---|---|
| openeebus SPINE/SHIP core | ~80 KB |
| mbedTLS session | ~50 KB |
| httpd + WS buffers | ~30 KB |
| FreeRTOS tasks (3×) | ~24 KB |
| ESPHome WiFi/Ethernet | ~60 KB |
| **Total** | **~244 KB** of 520 KB |

On ESP32-S3 with PSRAM: set `CONFIG_EEBUS_USE_PSRAM=y` in `sdkconfig.defaults`
to route openeebus heap allocations to SPIRAM, leaving internal SRAM free.

---

## Regulatory note

The relay-contact path (DI1 from CLS-Steuerbox → DO1/DO2 SG-Ready + Modbus)
is fully compliant until 31.12.2028 per Westnetz DRW-O-AN §6.2.1 and remains
the production fallback in the example YAML. The EEBus path is the
forward-looking digital interface preferred by Westnetz post-2028.

---

## Upstream

This branch forks [NIBEGroup/openeebus](https://github.com/NIBEGroup/openeebus)
(Apache 2.0). All upstream code is unmodified. New files are in `port/` and
`esphome/`.
