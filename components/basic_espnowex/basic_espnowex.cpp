#include "basic_espnowex.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

namespace esphome {
namespace espnow {

BasicESPNowEx *BasicESPNowEx::instance_ = nullptr;

void BasicESPNowEx::setup() {
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();
  esp_now_register_recv_cb(&BasicESPNowEx::recv_cb);
  esp_now_register_send_cb(&BasicESPNowEx::send_cb);

  instance_ = this;

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, this->peer_mac_.data(), 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(peer.peer_addr)) {
    esp_now_add_peer(&peer);
  }

  ESP_LOGI("basic_espnowex", "ESP-NOW initialized");
}

void BasicESPNowEx::send_broadcast(const std::string &message) {
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcast, (const uint8_t *)message.data(), message.size());
}

void BasicESPNowEx::send_to_peer(const std::string &message) {
  esp_now_send(this->peer_mac_.data(), (const uint8_t *)message.data(), message.size());
}

void BasicESPNowEx::send_espnow(std::string message) {
  this->send_to_peer(message);  // callable from YAML lambdas
}
void BasicESPNowEx::send_espnow_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac) {
    std::vector<uint8_t> msg(4);
    msg[0] = static_cast<uint8_t>(cmd & 0xFF);static_cast<uint8_t>((cmd >> 8) & 0xFF);
    msg[1] = static_cast<uint8_t>((cmd >> 8) & 0xFF);
    msg[2] = msg[0];
    msg[3] = msg[1];


    this->send_espnow_ex(msg, peer_mac);
}

void BasicESPNowEx::send_espnow_ex(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &peer_mac) {

    if (!esp_now_is_peer_exist(peer_mac.data())) {
        esp_now_peer_info_t peer_info{};
        memcpy(peer_info.peer_addr, peer_mac.data(), 6);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        
        esp_err_t status = esp_now_add_peer(&peer_info);
        if (status != ESP_OK) {
            ESP_LOGE("basic_espnowex", "Failed to add peer: %s", esp_err_to_name(status));
            return;
        }
    }


    esp_err_t result = esp_now_send(peer_mac.data(), msg.data(), msg.size());
    if (result != ESP_OK) {
        ESP_LOGE("basic_espnowex", "Send error: %s", esp_err_to_name(result));
    }
}

void BasicESPNowEx::set_peer_mac(std::array<uint8_t, 6> mac) {
  this->peer_mac_ = mac;
}

void BasicESPNowEx::recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
    if (instance_) {
		std::array<uint8_t, 6> mac_array;
		std::copy_n(mac, 6, mac_array.begin());
		

		if (len == 4 && memcmp(data, "\x00\x01\x00\x01", 4) == 0) {
			instance_->handle_ack(mac_array);
			return; 
		}

        if (len == 4) {
            std::vector<uint8_t> ack = {0x00, 0x01, 0x00, 0x01};
            instance_->send_espnow_ex(ack, mac_array);
        }
		if (len == 4 && memcmp(data, data + 2, 2) == 0) {
			int16_t cmd;
			memcpy(&cmd, data, sizeof(cmd));
			instance_->handle_cmd(mac_array, cmd);
		}
		
        std::vector<uint8_t> msg(data, data + len);
        instance_->handle_received(msg, mac_array);
    }
}

void BasicESPNowEx::handle_received(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &mac) {
    for (auto *trig : this->triggers_) {
        trig->trigger(msg, mac);
    }
}

void BasicESPNowEx::send_cb(const uint8_t *mac, esp_now_send_status_t status) {
  ESP_LOGD("basic_espnowex", "Send to %02X:%02X:%02X:%02X:%02X:%02X %s",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
           status == ESP_NOW_SEND_SUCCESS ? "succeeded" : "failed");
}

void BasicESPNowEx::add_on_message_trigger(OnMessageTrigger *trigger) {
  this->triggers_.push_back(trigger);
}

OnMessageTrigger::OnMessageTrigger(BasicESPNowEx *parent) {
    parent->add_on_message_trigger(this);
}

OnRecvAckTrigger::OnRecvAckTrigger(BasicESPNowEx *parent) {
    parent->add_on_recv_ack_trigger(this);
}

OnRecvCmdTrigger::OnRecvCmdTrigger(BasicESPNowEx *parent) {
    parent->add_on_recv_cmd_trigger(this);
}


void BasicESPNowEx::handle_ack(const std::array<uint8_t, 6> &mac) {
    for (auto *trig : this->ack_triggers_) {
        trig->trigger(mac);
    }
}

void BasicESPNowEx::add_on_recv_ack_trigger(OnRecvAckTrigger *trigger) {
    this->ack_triggers_.push_back(trigger);
}
void BasicESPNowEx::handle_cmd(const std::array<uint8_t, 6> &mac, int16_t cmd) {
    for (auto *trig : this->cmd_triggers_) {
        trig->trigger(mac, cmd);
    }
    this->on_recv_cmd_callback_.call(mac, cmd);
}

void BasicESPNowEx::add_on_recv_cmd_trigger(OnRecvCmdTrigger *trigger) {
    this->cmd_triggers_.push_back(trigger);
}

}  // namespace espnow
}  // namespace esphome
