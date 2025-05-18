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

class OnMessageTrigger : public Trigger<std::vector<uint8_t>, std::array<uint8_t, 6>> {
 public:
  explicit OnMessageTrigger(BasicESPNowEx *parent);
};
class OnRecvAckTrigger : public Trigger<std::array<uint8_t, 6>> {
public:
    explicit OnRecvAckTrigger(BasicESPNowEx *parent);
};
class OnRecvCmdTrigger : public Trigger<std::array<uint8_t, 6>, int16_t> {
public:
    explicit OnRecvCmdTrigger(BasicESPNowEx *parent);
};

class BasicESPNowEx : public Component {
 public:
  void setup() override;
  void loop() override {}

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
  static void recv_cb(const uint8_t *mac, const uint8_t *data, int len);
  static void send_cb(const uint8_t *mac, esp_now_send_status_t status);
  void handle_received(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &mac);

  std::array<uint8_t, 6> peer_mac_{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  static BasicESPNowEx *instance_;
  std::vector<OnMessageTrigger *> triggers_;

  void handle_ack(const std::array<uint8_t, 6> &mac);
  std::vector<OnRecvAckTrigger *> ack_triggers_; 
  void handle_cmd(const std::array<uint8_t, 6> &mac, int16_t cmd);
  std::vector<OnRecvCmdTrigger *> cmd_triggers_;
};

}  // namespace espnow
}  // namespace esphome
