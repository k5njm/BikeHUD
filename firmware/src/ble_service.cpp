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
      Serial.printf("[ble] bad write len %u (want 16)\n",
                    (unsigned)value.size());
      return;
    }

    const uint8_t *raw = value.data();
    if (bike_hud_is_time_sync(raw, (uint16_t)value.size())) {
      BikeHudTimeSync ts;
      memcpy(&ts, raw, sizeof(ts));
      g_telemetry.applyTimeSync(ts, millis());
      Serial.printf("[ble] time sync %04u-%02u-%02u %02u:%02u:%02u dow=%u\n",
                    ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second,
                    ts.day_of_week);
      return;
    }

    if (!bike_hud_is_telemetry(raw, (uint16_t)value.size())) {
      Serial.printf("[ble] unknown msg type 0x%02x\n", raw[0]);
      return;
    }

    BikeHudPacketV1 pkt;
    memcpy(&pkt, raw, sizeof(pkt));
    g_telemetry.applyTelemetry(pkt, millis());
    Serial.printf("[ble] pkt #%lu spd=%u hr=%u cad=%u\n",
                  (unsigned long)g_telemetry.write_count, pkt.speed_cm_s,
                  pkt.hr_bpm, pkt.cadence_rpm);
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

  BikeHudPacketV1 empty{};
  empty.version = BIKE_HUD_PROTOCOL_VERSION;
  empty.cadence_rpm = BIKE_HUD_UNKNOWN_U8;
  empty.batt_pct = BIKE_HUD_UNKNOWN_U8;
  empty.gps_acc_m = BIKE_HUD_UNKNOWN_U8;
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
