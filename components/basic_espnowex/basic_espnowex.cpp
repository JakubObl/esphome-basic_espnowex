#include "basic_espnowex.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <functional>

namespace esphome {
namespace espnow {

BasicESPNowEx *BasicESPNowEx::instance_ = nullptr;

// deklaracja funkcji pomocniczej
void BasicESPNowEx::static_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
  // przekaż dalej do instancji
  static_cast<BasicESPNowEx*>(arg)->on_wifi_event(base, id, data);
}

void BasicESPNowEx::setup() {
  //esp_netif_init();
  //esp_event_loop_create_default();
  //esp_netif_create_default_wifi_sta();

  //wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  //esp_wifi_init(&cfg);
  //esp_wifi_set_mode(WIFI_MODE_STA);
  //esp_wifi_start();
  
  esp_event_handler_instance_register(
    WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
    &BasicESPNowEx::static_wifi_event, this, nullptr);
	
  // Sprawdzić czy WiFi jest już zainicjalizowane
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err == ESP_ERR_WIFI_INIT_STATE) {
    // WiFi już zainicjalizowane - to normalne w ESPHome
    ESP_LOGI("basic_espnowex", "WiFi already initialized");
  } else if (err != ESP_OK) {
    ESP_LOGE("basic_espnowex", "WiFi init failed: %s", esp_err_to_name(err));
    return;
  }

  uint8_t wifi_channel;
  esp_wifi_get_channel(&wifi_channel, nullptr);
  esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
	
  esp_now_init();
  esp_now_register_recv_cb(&BasicESPNowEx::recv_cb);
  esp_now_register_send_cb(&BasicESPNowEx::send_cb);

  instance_ = this;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, this->peer_mac_.data(), 6);
  peer.channel = 0; //wifi_channel;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(peer.peer_addr)) {
    esp_now_add_peer(&peer);
  }
	
  // 1) Utwórz semafor
  this->queue_mutex_ = xSemaphoreCreateMutex();
  this->history_mutex_ = xSemaphoreCreateMutex();
	
  // Konfiguracja timera do okresowej weryfikacji kolejki
  const esp_timer_create_args_t timer_args = {
    .callback = [](void* arg) {
      static_cast<BasicESPNowEx*>(arg)->process_send_queue();
    },
    .arg = this,
    .name = "espnow_retry_timer"
  };
  
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &this->retry_timer_));
  ESP_ERROR_CHECK(esp_timer_start_periodic(this->retry_timer_, 400000)); // 100ms

  ESP_LOGI("basic_espnowex", "ESP-NOW initialized");
}

void BasicESPNowEx::on_wifi_event(esp_event_base_t base, int32_t id, void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
    uint8_t new_ch; 
    esp_wifi_get_channel(&new_ch, nullptr);

    //std::lock_guard<std::mutex> lock(peer_mutex_);
	  
    esp_now_deinit();
    esp_now_init();
    esp_now_register_recv_cb(&BasicESPNowEx::recv_cb);
    esp_now_register_send_cb(&BasicESPNowEx::send_cb);
  
    // Ponowna inicjalizacja peerów
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, this->peer_mac_.data(), 6);
    peer.channel = 0; // Kanał będzie aktualizowany dynamicznie
    peer.encrypt = false;
  
    if (!esp_now_is_peer_exist(peer.peer_addr)) {
      esp_now_add_peer(&peer);
    }
  }
}
void BasicESPNowEx::set_peer_mac(std::array<uint8_t, 6> mac) {
  this->peer_mac_ = mac;
}
void BasicESPNowEx::set_max_retries(uint8_t max_retries_) {
  this->max_retries = max_retries_;
}
void BasicESPNowEx::set_timeout_us(int64_t timeout_us_) {
  this->timeout_us = timeout_us_*1000;
}
void BasicESPNowEx::send_broadcast(const std::vector<uint8_t> &msg) {
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcast, msg.data(), msg.size());
  if (result != ESP_OK) {
      ESP_LOGE("basic_espnowex", "Send broadcast error: %s", esp_err_to_name(result));
  }
	
}
void BasicESPNowEx::send_broadcast_str(const std::string &message) {
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcast, (const uint8_t *)message.data(), message.size());
  if (result != ESP_OK) {
      ESP_LOGE("basic_espnowex", "Send broadcast str error: %s", esp_err_to_name(result));
  }
}

void BasicESPNowEx::send_to_peer(const std::vector<uint8_t> &msg) {
  this->send_espnow(msg, this->peer_mac_);
}

void BasicESPNowEx::send_to_peer_str(const std::string &message) {
  std::vector<uint8_t> msg(message.begin(), message.end());
  this->send_espnow(msg, this->peer_mac_);
}

void BasicESPNowEx::send_espnow_str(std::string message, const std::array<uint8_t, 6> &peer_mac) {
  std::vector<uint8_t> msg(message.begin(), message.end());
  this->send_espnow(msg, peer_mac);
}
void BasicESPNowEx::send_espnow_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac) {
    std::vector<uint8_t> msg(4);
    msg[0] = static_cast<uint8_t>((cmd >> 8) & 0xFF);
    msg[1] = static_cast<uint8_t>(cmd & 0xFF);
    msg[2] = msg[0];
    msg[3] = msg[1];
    if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
        auto it = std::find_if(
            this->pending_messages_.begin(),
            this->pending_messages_.end(),
            [&](PendingMessage& m) {
                // warunek 1: ten sam peer_mac
                if (m.mac != peer_mac)
                    return false;
                // warunek 2: niezaakceptowana wiadomość
                if (m.acked)
                    return false;
                // warunek 3: payload ma dokładnie 8 bajtów
                if (m.payload.size() != 8)
                    return false;
                // warunek 4: bajty 5-8 payloadu są identyczne jak msg
                // (zakładamy, że msg ma co najmniej 4 bajty)
                return std::equal(m.payload.begin() + 4, m.payload.begin() + 8, msg.begin());
            }
        );
	bool should_send = false;
        if (it != this->pending_messages_.end()) {
            it->peer_add_attempts = 0;
            it->retry_count = 0;
            it->timestamp = esp_timer_get_time();
            // nie wysyłamy ponownie
        } else {
            should_send = true;
        }
        xSemaphoreGive(this->queue_mutex_);
	if (should_send) {
    		this->send_espnow(msg, peer_mac);
	}
    }
}

void BasicESPNowEx::clear_pending_messages() {
    if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
        this->pending_messages_.clear(); // Usuwa wszystkie elementy z kolejki
        xSemaphoreGive(this->queue_mutex_);
    }
}

void BasicESPNowEx::send_espnow(const std::vector<uint8_t>& msg, const std::array<uint8_t, 6>& peer_mac) {
  
  if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
  
	PendingMessage pending;
	pending.mac = peer_mac;
	pending.message_id = this->generate_message_id();
	pending.peer_add_attempts = 0;
	pending.retry_count = 0;
	pending.timestamp = esp_timer_get_time();
	pending.acked = false;
	// Dodanie nagłówka 0x00 + message_id
        pending.payload.reserve(4 + msg.size());
        pending.payload.push_back(0x00);
        pending.payload.insert(pending.payload.end(), pending.message_id.begin(), pending.message_id.end());
        pending.payload.insert(pending.payload.end(), msg.begin(), msg.end());
  
  	pending_messages_.push_back(pending);
    	xSemaphoreGive(this->queue_mutex_);
  }
  process_send_queue();
}

void BasicESPNowEx::process_send_queue() {
  const int64_t now = esp_timer_get_time();

  // Lock
  if (xSemaphoreTake(this->queue_mutex_, 0) == pdTRUE) {
	  // Usuń potwierdzone lub przekroczone limity czasu
	  this->pending_messages_.erase(
	    std::remove_if(this->pending_messages_.begin(), this->pending_messages_.end(),
	      [now, this](const PendingMessage& m) {
	        return m.acked || ((now - m.timestamp) > this->timeout_us && m.retry_count >= this->max_retries);
	      }),
	    this->pending_messages_.end());
	
	  for (auto& msg : this->pending_messages_) {
	    if (!msg.acked && (now - msg.timestamp) > this->timeout_us && msg.retry_count < this->max_retries) {

		// Sprawdź czy peer istnieje
                if (!esp_now_is_peer_exist(msg.mac.data())) {
                    ESP_LOGD("basic_espnowex", "Peer not registered, adding...");
                    esp_now_peer_info_t peer_info = {};
                    memcpy(peer_info.peer_addr, msg.mac.data(), 6);
                    // Pobierz aktualny kanał WiFi
                    ///uint8_t current_channel;
                    //esp_wifi_get_channel(&current_channel, nullptr);
                    peer_info.channel = 0; //current_channel;
                    peer_info.encrypt = false;
                    
                    esp_err_t add_status = esp_now_add_peer(&peer_info);
                    if (add_status != ESP_OK) {
                        ESP_LOGE("basic_espnowex", "Failed to add peer: %s", esp_err_to_name(add_status));
			msg.peer_add_attempts++;
			if (msg.peer_add_attempts > 3) {
				msg.acked = true; // Wymuszenie usunięcia z kolejki
			}
                        continue; // Pominięcie wysyłki przy błędzie
                    }
                }
		    
	      esp_err_t result = esp_now_send(msg.mac.data(), msg.payload.data(), msg.payload.size());
	      if (result == ESP_OK) {
	        msg.retry_count++;
	        msg.timestamp = now;
	        ESP_LOGD("basic_espnowex", "Retransmit to %02X:%02X:%02X:%02X:%02X:%02X, ID %02X%02X%02X, attempt %d",
	                 msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5], msg.message_id[0], msg.message_id[1], msg.message_id[2], msg.retry_count);
	        }
	      }
	    }
	  xSemaphoreGive(this->queue_mutex_);
  }
}

std::array<uint8_t, 3> BasicESPNowEx::generate_message_id() {
    uint32_t random_part = esp_random();
    int64_t timestamp = esp_timer_get_time();
    return {
        static_cast<uint8_t>((random_part ^ timestamp) & 0xFF),
        static_cast<uint8_t>((random_part >> 8 ^ timestamp >> 8) & 0xFF),
        static_cast<uint8_t>((random_part >> 16 ^ timestamp >> 16) & 0xFF)
    };
}

void BasicESPNowEx::recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
	if (!instance_ || !mac || !data || len < 1) return;
	std::array<uint8_t, 6> sender_mac;
	std::copy_n(mac, 6, sender_mac.begin());

	// Obsługa ACK (4 bajty: 0x01 + message_id)
	if (len == 4 && data[0] == 0x01) {
        	std::array<uint8_t, 3> ack_id{data[1], data[2], data[3]};
		bool should_handle_ack = false;
        	if (xSemaphoreTake(instance_->queue_mutex_, portMAX_DELAY) == pdTRUE) {
            		auto it = std::find_if(instance_->pending_messages_.begin(), instance_->pending_messages_.end(),
	                [&](const PendingMessage& m) {
	                    return memcmp(m.mac.data(), sender_mac.data(), 6) == 0 &&
	                           m.message_id == ack_id;
	                });
	            if (it != instance_->pending_messages_.end()) {
			if (!it->acked){
	                	it->acked = true;
	                	ESP_LOGD("basic_espnowex", "ACK received for message %02X%02X%02X", ack_id[0], ack_id[1], ack_id[2]);
				should_handle_ack = true;
	            	}
		    }
	            xSemaphoreGive(instance_->queue_mutex_);
        	}
		if (should_handle_ack) {
    			this->on_recv_ack_callback_.call(sender_mac, ack_id);
		}
        	return;
    	}
	// Walidacja podstawowej wiadomości
	if (len < 5 || data[0] != 0x00) {
		ESP_LOGE("basic_espnowex", "Invalid message format");
		return;
	}	
	// Wysyłanie ACK
	std::array<uint8_t, 3> msg_id{data[1], data[2], data[3]};
	std::vector<uint8_t> ack_packet{0x01, msg_id[0], msg_id[1], msg_id[2]};
	if (!esp_now_is_peer_exist(sender_mac.data())) {
		esp_now_peer_info_t peer_info = {};
		memcpy(peer_info.peer_addr, sender_mac.data(), 6);
		peer_info.channel = 0;
		peer_info.encrypt = false;
		esp_now_add_peer(&peer_info);
	}
	esp_now_send(sender_mac.data(), ack_packet.data(), ack_packet.size());

	int64_t now = esp_timer_get_time();
	bool is_duplicate = false;
	// Mutex chroniący dostęp do historii
	if (xSemaphoreTake(instance_->history_mutex_, portMAX_DELAY) == pdTRUE) {
		// Usuń stare wpisy (>300s)
		auto &history = instance_->received_history_;
		
		history.erase(
			std::remove_if(history.begin(), history.end(),
			[now](const ReceivedMessageInfo &info) {
		          return (now - info.timestamp) > 300000000; // 300s
			}),
		      history.end());
	
		is_duplicate = std::any_of(history.begin(), history.end(),
			[&](const ReceivedMessageInfo &info) {
			return info.mac == sender_mac &&
				info.data.size() == len &&
				std::equal(info.data.begin(), info.data.end(), data);
		});
	
		if (!is_duplicate) {
			constexpr size_t MAX_HISTORY_SIZE = 1000;
			if (history.size() >= MAX_HISTORY_SIZE) {
				history.erase(history.begin());
			}
			history.push_back({sender_mac, now, std::vector<uint8_t>(data, data + len)});
		}
	
		xSemaphoreGive(instance_->history_mutex_);
	}
	
	if (is_duplicate) {
		return;
	}

	// 4. Przetwarzanie payloadu po usunięciu nagłówka
	 std::vector<uint8_t> payload(data + 4, data + len);

	// 5. Dekodowanie komendy (jeśli payload ma dokładnie 4 bajty i pierwszy dwój jest taki sam jak ostatni dwój)
	if (payload.size() == 4 && memcmp(payload.data(), payload.data() + 2, 2) == 0) {
		int16_t cmd = (payload[0] << 8) | payload[1]; // Big-endian
		this->on_recv_cmd_callback_.call(sender_mac, cmd);
		ESP_LOGD("basic_espnowex", "CMD received for message...");
	}
	
	// 6. Przekazanie danych i wiadomości tekstowej
	this->on_recv_data_callback_.call(sender_mac, payload);  
	if (!payload.empty()) {
		std::string msg(payload.begin(), payload.end());
		this->on_message_callback_.call(sender_mac, msg);
	}
}

void BasicESPNowEx::send_cb(const uint8_t *mac, esp_now_send_status_t status) {
  ESP_LOGD("basic_espnowex", "Send to %02X:%02X:%02X:%02X:%02X:%02X %s",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
           status == ESP_NOW_SEND_SUCCESS ? "succeeded" : "failed");
}



OnMessageTrigger::OnMessageTrigger(BasicESPNowEx *parent) {
     parent->add_on_message_callback([this](const std::array<uint8_t, 6> mac, const std::string message) {
          trigger(mac, message);
      });
}

OnRecvAckTrigger::OnRecvAckTrigger(BasicESPNowEx *parent) {
    parent->add_on_recv_ack_callback([this](const std::array<uint8_t, 6> mac, std::array<uint8_t, 3> msg_id) {
         trigger(mac, msg_id);
    });
}

OnRecvCmdTrigger::OnRecvCmdTrigger(BasicESPNowEx *parent) {
    parent->add_on_recv_cmd_callback([this](const std::array<uint8_t, 6> mac, const int16_t cmd) {
        trigger(mac, cmd);
    });
}
OnRecvDataTrigger::OnRecvDataTrigger(BasicESPNowEx *parent) {
    parent->add_on_recv_data_callback([this](const std::array<uint8_t, 6> mac, const std::vector<uint8_t> dt) {
        trigger(mac, dt);
    });
}
/*
void BasicESPNowEx::handle_msg(std::array<uint8_t, 6> &mac, std::string &msg) {
    //for (auto *trig : this->msg_triggers_) {
    //    trig->trigger(mac, msg);
    //}
    this->on_message_callback_.call(mac, msg);
}
void BasicESPNowEx::handle_ack(std::array<uint8_t, 6> &mac, std::array<uint8_t, 3> &msg_id) {
    //for (auto *trig : this->ack_triggers_) {
    //   trig->trigger(mac, msg_id);
    //}
    this->on_recv_ack_callback_.call(mac, msg_id);
}

void BasicESPNowEx::handle_cmd(std::array<uint8_t, 6> &mac, int16_t cmd) {
    //for (auto *trig : this->cmd_triggers_) {
    //    trig->trigger(mac, cmd);
    //}
    this->on_recv_cmd_callback_.call(mac, cmd);
}
void BasicESPNowEx::handle_data(std::array<uint8_t, 6> &mac, std::vector<uint8_t> &dt) {
    //for (auto *trig : this->data_triggers_) {
    //    trig->trigger(mac, dt);
    //}
    this->on_recv_data_callback_.call(mac, dt);
}
*/
BasicESPNowEx::~BasicESPNowEx() {
  esp_timer_stop(this->retry_timer_);
  esp_timer_delete(this->retry_timer_);
  // Usuń semafor
  vSemaphoreDelete(this->queue_mutex_);
  vSemaphoreDelete(this->history_mutex_);
}

}  // namespace espnow
}  // namespace esphome
