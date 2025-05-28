#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esp_now.h"
#include "esp_timer.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>

namespace esphome {
namespace espnow {

struct PendingMessage {
  std::array<uint8_t, 6> mac;
  std::array<uint8_t, 3> message_id;
  uint8_t retry_count;
  int64_t timestamp;
  bool acked;
  std::vector<uint8_t> payload;
};

class BasicESPNowEx;


class OnMessageTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, std::string>, public Component {
 public:
  explicit OnMessageTrigger(BasicESPNowEx *parent);
};
class OnRecvAckTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, , std::array<uint8_t, 3>>, public Component {
public:
    explicit OnRecvAckTrigger(BasicESPNowEx *parent);
};
class OnRecvCmdTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, int16_t>, public Component {
public:
    explicit OnRecvCmdTrigger(BasicESPNowEx *parent);
};
class OnRecvDataTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, std::vector<uint8_t>>, public Component {
public:
    explicit OnRecvDataTrigger(BasicESPNowEx *parent);
};

class BasicESPNowEx : public Component {
 public:
  void setup() override;
  void loop() override {}

  void on_wifi_event(esp_event_base_t base, int32_t id, void* data);
     
   // C++ subscription API:
   void add_on_message_callback(std::function<void(std::array<uint8_t,6>, std::string)> &&cb) {
    this->on_message_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, std::string)> on_message_callback_;

  void add_on_recv_ack_callback(std::function<void(std::array<uint8_t,6>, , std::array<uint8_t, 3>)> &&cb) {
    this->on_recv_ack_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>)> on_recv_ack_callback_;

  void add_on_recv_cmd_callback(std::function<void(std::array<uint8_t,6>, int16_t)> &&cb) {
    this->on_recv_cmd_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, int16_t)> on_recv_cmd_callback_;
  
  void add_on_recv_data_callback(std::function<void(std::array<uint8_t,6>, std::vector<uint8_t>)> &&cb) {
    this->on_recv_data_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, std::vector<uint8_t>)> on_recv_data_callback_;

  void set_peer_mac(std::array<uint8_t, 6> mac);
  void set_max_retries(uint8_t max_retries_);
  void set_timeout_us(int64_t timeout_us_);
  void send_broadcast(const std::vector<uint8_t> &msg);
  void send_broadcast_str(const std::string &message);
  void send_to_peer(const std::vector<uint8_t> &msg);
  void send_to_peer_str(const std::string &message);
  void send_espnow_str(std::string message, const std::array<uint8_t, 6> &peer_mac);
  void send_espnow(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &peer_mac);
  void send_espnow_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac);

  void add_on_message_trigger(OnMessageTrigger *trigger);
  void add_on_recv_ack_trigger(OnRecvAckTrigger *trigger);
  void add_on_recv_cmd_trigger(OnRecvCmdTrigger *trigger);
  void add_on_recv_data_trigger(OnRecvDataTrigger *trigger);
  ~BasicESPNowEx();

 protected:
  std::array<uint8_t, 3> generate_message_id();
  void process_send_queue();
  std::vector<PendingMessage> pending_messages_;
  SemaphoreHandle_t queue_mutex_;;
  esp_timer_handle_t retry_timer_;
  static void static_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data);
  static void recv_cb(const uint8_t *mac, const uint8_t *data, int len);
  static void send_cb(const uint8_t *mac, esp_now_send_status_t status);
  void handle_msg(std::array<uint8_t, 6> &mac, std::string &msg);

  int64_t timeout_us = 200 * 1000; // 200ms
  uint8_t max_retries = 5;
  std::array<uint8_t, 6> peer_mac_{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  static BasicESPNowEx *instance_;
  std::vector<OnMessageTrigger *> msg_triggers_;

  void handle_ack(std::array<uint8_t, 6> &mac, std::array<uint8_t, 3> &msg_id);
  std::vector<OnRecvAckTrigger *> ack_triggers_; 
  void handle_cmd(std::array<uint8_t, 6> &mac, int16_t cmd);
  std::vector<OnRecvCmdTrigger *> cmd_triggers_;
  void handle_data(std::array<uint8_t, 6> &mac, std::vector<uint8_t> &dt);
  std::vector<OnRecvDataTrigger *> data_triggers_;
  

};

}  // namespace espnow
}  // namespace esphome
