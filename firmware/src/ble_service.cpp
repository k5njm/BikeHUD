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
    if (value.size() != BIKE_HUD_PACKET_V1_SIZE) {
      Serial.printf("[ble] bad write len %u (want %u)\n",
                    (unsigned)value.size(), (unsigned)BIKE_HUD_PACKET_V1_SIZE);
      return;
    }

    BikeHudPacketV1 pkt;
    memcpy(&pkt, value.data(), sizeof(pkt));

    if (!bike_hud_packet_version_ok(&pkt)) {
      Serial.printf("[ble] unknown version %u\n", pkt.version);
      return;
    }

    g_telemetry.apply(pkt, millis());
    Serial.printf("[ble] pkt #%lu spd=%u cm/s hr=%u flags=0x%02x\n",
                  (unsigned long)g_telemetry.write_count, pkt.speed_cm_s,
                  pkt.hr_bpm, pkt.flags);
  }
} g_telem_cbs;

} // namespace

void ble_service_begin() {
  if (!NimBLEDevice::init(BIKE_HUD_DEVICE_NAME)) {
    Serial.println("[ble] NimBLEDevice::init FAILED");
    return;
  }

  // ~+3 dBm; avoid max TX on a small battery pack.
  NimBLEDevice::setPower(3);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(&g_server_cbs);

  NimBLEService *svc = g_server->createService(BIKE_HUD_UUID_SERVICE_STR);

  g_telem_char = svc->createCharacteristic(
      BIKE_HUD_UUID_TELEMETRY_STR,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::WRITE);

  g_telem_char->setCallbacks(&g_telem_cbs);

  BikeHudPacketV1 empty{};
  empty.version = BIKE_HUD_PROTOCOL_VERSION;
  empty.cadence_rpm = BIKE_HUD_UNKNOWN_U8;
  empty.batt_pct = BIKE_HUD_UNKNOWN_U8;
  empty.gps_acc_m = BIKE_HUD_UNKNOWN_U8;
  g_telem_char->setValue((uint8_t *)&empty, sizeof(empty));
  // NimBLE 2.x starts services when advertising begins; no svc->start().

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(BIKE_HUD_DEVICE_NAME);
  adv->addServiceUUID(BIKE_HUD_UUID_SERVICE_STR);
  adv->enableScanResponse(true);
  adv->start();

  Serial.println("[ble] advertising as " BIKE_HUD_DEVICE_NAME);
}

void ble_service_loop() {
  // NimBLE is callback-driven; nothing required.
}

bool ble_service_is_connected() { return g_connected; }
