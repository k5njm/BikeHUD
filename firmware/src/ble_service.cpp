#include "ble_service.h"

#include "bike_hud_protocol.h"
#include "telemetry.h"

#include <NimBLEDevice.h>
#include <string.h>

namespace {

NimBLEServer *g_server = nullptr;
NimBLECharacteristic *g_telem_char = nullptr;
bool g_connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer * /*server*/, NimBLEConnInfo & /*connInfo*/) override {
    g_connected = true;
    Serial.println("[ble] central connected");
  }

  void onDisconnect(NimBLEServer * /*server*/, NimBLEConnInfo & /*connInfo*/,
                    int /*reason*/) override {
    g_connected = false;
    Serial.println("[ble] central disconnected — restart advertising");
    NimBLEDevice::startAdvertising();
  }
} g_server_cbs;

class TelemetryCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo & /*connInfo*/) override {
    const NimBLEAttValue &value = c->getValue();
    const size_t n = value.size();

    if (n != BIKE_HUD_PACKET_V1_SIZE && n != BIKE_HUD_PACKET_V2_SIZE) {
      Serial.printf("[ble] bad write len %u (want %u or %u)\n", (unsigned)n,
                    (unsigned)BIKE_HUD_PACKET_V1_SIZE,
                    (unsigned)BIKE_HUD_PACKET_V2_SIZE);
      return;
    }

    if (n == BIKE_HUD_PACKET_V2_SIZE) {
      BikeHudPacketV2 pkt;
      memcpy(&pkt, value.data(), sizeof(pkt));
      if (!bike_hud_packet_version_ok(&pkt.body)) {
        Serial.printf("[ble] unknown version %u\n", pkt.body.version);
        return;
      }
      // Prefer v2 clock only when version says so.
      if (pkt.body.version == BIKE_HUD_PROTOCOL_VERSION_V2) {
        g_telemetry.apply_v2(pkt, millis());
      } else {
        g_telemetry.apply(pkt.body, millis());
      }
      Serial.printf("[ble] pkt #%lu v%u spd=%u hr=%u\n",
                    (unsigned long)g_telemetry.write_count, pkt.body.version,
                    pkt.body.speed_cm_s, pkt.body.hr_bpm);
      return;
    }

    BikeHudPacketV1 pkt;
    memcpy(&pkt, value.data(), sizeof(pkt));
    if (!bike_hud_packet_version_ok(&pkt)) {
      Serial.printf("[ble] unknown version %u\n", pkt.version);
      return;
    }
    g_telemetry.apply(pkt, millis());
    Serial.printf("[ble] pkt #%lu v1 spd=%u hr=%u\n",
                  (unsigned long)g_telemetry.write_count, pkt.speed_cm_s,
                  pkt.hr_bpm);
  }
} g_telem_cbs;

} // namespace

void ble_service_begin() {
  if (!NimBLEDevice::init(BIKE_HUD_DEVICE_NAME)) {
    Serial.println("[ble] NimBLEDevice::init FAILED");
    return;
  }

  NimBLEDevice::setPower(3);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(&g_server_cbs);

  NimBLEService *svc = g_server->createService(BIKE_HUD_UUID_SERVICE_STR);

  g_telem_char = svc->createCharacteristic(
      BIKE_HUD_UUID_TELEMETRY_STR,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::WRITE);

  g_telem_char->setCallbacks(&g_telem_cbs);

  BikeHudPacketV2 empty{};
  empty.body.version = BIKE_HUD_PROTOCOL_VERSION_V2;
  empty.body.cadence_rpm = BIKE_HUD_UNKNOWN_U8;
  empty.body.batt_pct = BIKE_HUD_UNKNOWN_U8;
  empty.body.gps_acc_m = BIKE_HUD_UNKNOWN_U8;
  g_telem_char->setValue((uint8_t *)&empty, sizeof(empty));

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(BIKE_HUD_DEVICE_NAME);
  adv->addServiceUUID(BIKE_HUD_UUID_SERVICE_STR);
  adv->enableScanResponse(true);
  adv->start();

  Serial.println("[ble] advertising as " BIKE_HUD_DEVICE_NAME);
}

void ble_service_loop() {}

bool ble_service_is_connected() { return g_connected; }
