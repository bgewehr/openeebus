# openeebus-esphome

**EEBus SHIP/SPINE für ESP32 — §14a EnWG HEMS als ESPHome-Komponente**

Dieses Repository ist ein Fork von [NIBEGroup/openeebus](https://github.com/NIBEGroup/openeebus)
(Apache 2.0). Der Branch `bg-hems` ergänzt den C-Protokollkern um einen vollständigen
**ESP32-Port** sowie zwei **ESPHome External Components**, die ein DIY-HEMS
(Heimenergie-Management-System) nach §14a EnWG auf einem einzelnen ESP32-Hutschienen-Gerät
realisieren.

> **Status:** Implementierung abgeschlossen, Runtime-Test gegen echte CLS-Steuerbox ausstehend
> (Westnetz iMSys-Installation noch nicht erfolgt). Relay-Kontakt-Fallback (DI1) ist vollständig
> getestet und regulatorisch bis 31.12.2028 ausreichend.

---

## Überblick

```
Westnetz CLS-Steuerbox
    │  EEBus LPC  (SHIP/SPINE, TLS, Port 4712)
    │  Steuerbox = EG  →  ESP32 = CS
    ▼
ESP32 HEMS  (dieser Fork, Branch bg-hems)
    │  EEBus LPC  (SHIP/SPINE, TLS, Port 4713)
    │  ESP32 = EG  →  K40RF = CS
    ▼
Bosch EEBus Gateway K40RF
    │  EMS-BUS intern
    ▼
Bosch Compress 5800i
    +
Shell Recharge Wallbox  ←  Modbus TCP (ESP32 direkt)
```

### Steuerbare Verbrauchseinrichtungen (§14a EnWG)

| Gerät | Steuerung | Protokoll |
|---|---|---|
| Bosch Compress 5800i | via K40RF Gateway | EEBus LPC (EG-Rolle) |
| Shell Recharge Wallbox | direkt | Modbus TCP |
| Fallback WP | SG-Ready DO1/DO2 | GPIO Relais |
| Fallback VNB-Signal | Relaiskontakt DI1 | GPIO Eingang |

---

## Hardware

Empfohlen: ESP32 mit Ethernet und 8× DI/DO im REG-Hutschienen-Gehäuse.

Getestet mit / geeignet für:
- **Olimex ESP32-POE-ISO** (LAN8720, optokoppler-fähige I/O)
- **LILYGO T-Internet-POE** (LAN8720)
- Eigene Platine mit ESP32 + W5500/LAN8720 + 8× DI/DO Optokoppler

> Ethernet wird gegenüber WiFi empfohlen — EEBus erfordert stabilen TCP-Betrieb.

---

## Was dieser Fork hinzufügt

```
port/
└── esp32/
    ├── component/
    │   ├── CMakeLists.txt          ESP-IDF idf_component_register
    │   └── eebus_malloc_esp32.h    PSRAM-aware Allocator (ESP32-S3)
    └── websocket/
        ├── websocket_server_esp32.c/h        httpd TLS WS Server
        ├── websocket_client_esp32.c/h        esp_websocket_client Wrapper
        ├── websocket_server_creator_esp32.c  openeebus Creator Bridge
        └── websocket_client_creator_esp32.c  openeebus Creator Bridge

esphome/
├── components/
│   ├── eebus_lpc/          CS-Rolle: empfängt LPC-Limits von CLS-Steuerbox
│   │   ├── __init__.py     YAML-Schema
│   │   ├── eebus_lpc.h/cpp Komponente + Pairing-Flow + NVS-Persistenz
│   │   └── CMakeLists.txt
│   └── eebus_wp/           EG-Rolle: sendet LPC-Limits an Bosch K40RF
│       ├── __init__.py     YAML-Schema
│       ├── eebus_wp.h/cpp  mDNS-Discovery + Heartbeat + Failsafe
│       └── CMakeLists.txt
└── hems_14a_example.yaml   Vollständige YAML-Konfiguration
```

### Port-Analyse: Warum nur der WebSocket-Layer neu ist

openeebus ist bereits für Embedded-Targets designed. Alle anderen Dependencies
waren bereits portiert:

| Abhängigkeit | Upstream openeebus | ESP32 |
|---|---|---|
| Threading | `eebus_thread_freertos.c` ✅ | unverändert |
| Mutex/Queue | `eebus_mutex_freertos.c` ✅ | unverändert |
| mDNS | `ship_mdns_freertos.c` ✅ | unverändert (`esp_mdns`) |
| TLS/X.509 | `tls_certificate_mbedtls.c` ✅ | unverändert |
| JSON | cJSON → ESP-IDF built-in ✅ | kein Code nötig |
| **WebSocket** | libwebsockets ❌ | **neu: httpd + esp_websocket_client** |

---

## ESPHome Integration

### eebus_lpc — LPC-Empfänger (CS)

Empfängt Leistungslimits von der Westnetz CLS-Steuerbox.

```yaml
external_components:
  - source: github://bgewehr/openeebus@bg-hems
    components: [eebus_lpc]

eebus_lpc:
  id: hems_lpc
  ship_port: 4712
  remote_ski: ""            # Leer = Pairing-Modus; nach Pairing automatisch gespeichert
  failsafe_limit_w: 4200.0  # §14a Mindestbezugsleistung

  on_limit_active:
    - then:
        - lambda: |-
            float limit_w = x;   // Watt-Limit vom VNB
            id(hems_wp).set_limit(limit_w - 3900.0f);
            id(wallbox_ladestrom).set_value(6.0f);

  on_limit_cleared:
    - then:
        - lambda: |-
            id(hems_wp).clear_limit();
            id(wallbox_ladestrom).set_value(16.0f);
```

### eebus_wp — LPC-Sender (EG) → Bosch K40RF

Sendet Leistungslimits an die Bosch Compress 5800i via K40RF-Gateway.

```yaml
external_components:
  - source: github://bgewehr/openeebus@bg-hems
    components: [eebus_wp]

eebus_wp:
  id: hems_wp
  ship_port: 4713
  remote_ski: ""            # Leer = automatische mDNS-Discovery
  failsafe_limit_w: 4200.0
  failsafe_duration_s: 7200 # 2h (SHIP-Vorgabe: 2–24h)

  on_wp_connected:
    - logger.log: "K40RF verbunden"
  on_wp_disconnected:
    - logger.log: "K40RF getrennt"
```

### Vollständige YAML

[`esphome/hems_14a_example.yaml`](esphome/hems_14a_example.yaml) enthält:
- EEBus LPC (CS) + EEBus WP (EG)
- SG-Ready Fallback (DO1/DO2)
- Relaiskontakt-Fallback (DI1)
- Modbus TCP Wallbox
- Web Server v3 mit vollständigem HEMS-Status-UI
- Pairing-Buttons im Web UI

---

## Pairing-Flow

### CLS-Steuerbox (eebus_lpc)

1. ESP32 bootet → eigene SKI erscheint im Web UI unter **„Eigene SKI (lokal)"**
2. Steuerbox verbindet sich → **„Pairing SKI (ausstehend)"** erscheint
3. **„Pairing akzeptieren ✓"** drücken → SKI wird in NVS gespeichert
4. Nach Reboot: automatisches Re-Pairing ohne Nutzerinteraktion

### K40RF Gateway (eebus_wp)

1. K40RF erscheint automatisch per mDNS
2. Am WP-Bedienfeld: *System Settings → EEBus → Search EEBus devices → ESP32 auswählen*
3. SKI des K40RF erscheint in **„K40RF SKI"** im Web UI

---

## Speicherbedarf (ESP32, geschätzt)

| Komponente | RAM |
|---|---|
| openeebus SHIP/SPINE-Kern | ~80 KB |
| mbedTLS Session (×2 für LPC+WP) | ~100 KB |
| httpd + WS-Buffer | ~30 KB |
| FreeRTOS Tasks | ~24 KB |
| ESPHome + Ethernet-Stack | ~60 KB |
| **Gesamt** | **~294 KB** von 520 KB |

Für **ESP32-S3 mit PSRAM**: `CONFIG_EEBUS_USE_PSRAM=y` in `sdkconfig.defaults`
routet openeebus-Allokationen in den externen Speicher.

---

## Regulatorischer Kontext

| Pfad | Rechtsgrundlage | Status |
|---|---|---|
| Relaiskontakt (DI1 → DO1/DO2) | Westnetz DRW-O-AN §6.2.1 Übergangsfrist | ✅ sofort konform bis 31.12.2028 |
| EEBus digital (dieser Fork) | Westnetz DRW-O-AN §6.1 bevorzugte Lösung | ⚠️ implementiert, Test ausstehend |

---

## Upstream

- Protokollkern: [NIBEGroup/openeebus](https://github.com/NIBEGroup/openeebus) (Apache 2.0)
- Ursprüngliche NIBE-README: [README_NIBE.md](README_NIBE.md)
- Alle Upstream-Dateien sind unverändert. Neue Dateien ausschließlich in `port/` und `esphome/`.

---

## Lizenz

Apache License 2.0 — identisch mit dem Upstream-Repository.
