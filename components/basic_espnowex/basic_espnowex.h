#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esp_now.h"
#include <array>
#include <vector>
#include <string>

namespace esphome {
namespace espnow {

class BasicESPNowEx;


class OnMessageTrigger : public ::esphome::Trigger<std::vector<uint8_t>, std::array<uint8_t, 6>>, public Component {
 public:
  explicit OnMessageTrigger(BasicESPNowEx *parent);
};
class OnRecvAckTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>>, public Component {
public:
    explicit OnRecvAckTrigger(BasicESPNowEx *parent);
};
class OnRecvCmdTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, int16_t>, public Component {
public:
    explicit OnRecvCmdTrigger(BasicESPNowEx *parent);
};

class BasicESPNowEx : public Component {
 public:
  void setup() override;
  void loop() override {}

  void on_wifi_event(esp_event_base_t base, int32_t id, void* data);
     
   // C++ subscription API:
   void add_on_message_callback(std::function<void(std::vector<uint8_t>, std::array<uint8_t,6>)> &&cb) {
    this->on_message_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::vector<uint8_t>, std::array<uint8_t,6>)> on_message_callback_;

  void add_on_recv_ack_callback(std::function<void(std::array<uint8_t,6>)> &&cb) {
    this->on_recv_ack_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>)> on_recv_ack_callback_;

  void add_on_recv_cmd_callback(std::function<void(std::array<uint8_t,6>, int16_t)> &&cb) {
    this->on_recv_cmd_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, int16_t)> on_recv_cmd_callback_;

  void set_peer_mac(std::array<uint8_t, 6> mac);
  void send_broadcast(const std::string &message);
  void send_to_peer(const std::string &message);
  void send_espnow(std::string message);  // for YAML lambda
  void send_espnow_ex(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &peer_mac);
  void send_espnow_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac);

  void add_on_message_trigger(OnMessageTrigger *trigger);
  void add_on_recv_ack_trigger(OnRecvAckTrigger *trigger);
  void add_on_recv_cmd_trigger(OnRecvCmdTrigger *trigger);

 protected:
  static void static_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data);
  static void recv_cb(const uint8_t *mac, const uint8_t *data, int len);
  static void send_cb(const uint8_t *mac, esp_now_send_status_t status);
  void handle_received(std::vector<uint8_t> &msg, std::array<uint8_t, 6> &mac);

  std::array<uint8_t, 6> peer_mac_{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  static BasicESPNowEx *instance_;
  std::vector<OnMessageTrigger *> triggers_;

  void handle_ack(std::array<uint8_t, 6> &mac);
  std::vector<OnRecvAckTrigger *> ack_triggers_; 
  void handle_cmd(std::array<uint8_t, 6> &mac, int16_t cmd);
  std::vector<OnRecvCmdTrigger *> cmd_triggers_;
  

};

}  // namespace espnow
}  // namespace esphome
