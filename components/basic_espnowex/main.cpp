#include "esphome.h"
#include "basic_espnowex.h"

using namespace esphome;
using namespace esphome::espnow;

// Deklaracje triggerów jako wskaźniki
OnRecvCmdTrigger*   cmd_trigger = nullptr;
OnRecvAckTrigger*   ack_trigger = nullptr;
OnMessageTrigger*   msg_trigger = nullptr;

class MyESPNowComponent : public Component, public espnow::ESPNowReceiver {
 public:
  void setup() override {
    // Rejestracja triggerów
    cmd_trigger = new OnRecvCmdTrigger(espnow_comm);
    ack_trigger = new OnRecvAckTrigger(espnow_comm);
    msg_trigger = new OnMessageTrigger(espnow_comm);

    // Przykład subskrypcji zdarzeń
    cmd_trigger->add_listener([this](const std::array<uint8_t, 6>& mac, int16_t cmd) {
      ESP_LOGD("main", "Odebrano CMD od %02X:%02X:%02X:%02X:%02X:%02X: %d",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], cmd);
    });

    ack_trigger->add_listener([this](const std::array<uint8_t, 6>& mac, int16_t ack) {
      ESP_LOGD("main", "Odebrano ACK od %02X:%02X:%02X:%02X:%02X:%02X: %d",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ack);
    });

    msg_trigger->add_listener([this](const std::array<uint8_t, 6>& mac, const std::vector<uint8_t>& data) {
      ESP_LOGD("main", "Odebrano MESSAGE od %02X:%02X:%02X:%02X:%02X:%02X, rozmiar: %d",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], data.size());
    });
  }

  void loop() override {
    // Możesz tu dodawać wysyłanie komunikatów lub inne zadania
  }
};

// Rejestracja komponentu
MyESPNowComponent espnow_component;
esphome::espnow::ESPNowCommunication* espnow_comm;

void setup() {
  // Inicjalizacja ESPHome
  App.setup();

  // Utworzenie instancji komunikacji ESPNow
  espnow_comm = new ESPNowCommunication();
  App.register_component(espnow_comm);

  // Uruchomienie komponentu
  App.register_component(&espnow_component);
  App.begin();
}

void loop() {
  App.loop();
}
