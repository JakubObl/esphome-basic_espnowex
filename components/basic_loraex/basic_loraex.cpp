#include "basic_loraex.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <functional>

namespace esphome {
namespace lora {

static const char *const TAG = "basic_loraex";

BasicLoRaEx *BasicLoRaEx::instance_ = nullptr;

void BasicLoRaEx::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BasicLoRaEx...");
  
  instance_ = this;

  // Inicjalizacja pinów
  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->setup();
    this->cs_pin_->digital_write(true); // CS idle high
  }
  
  if (this->rst_pin_ != nullptr) {
    this->rst_pin_->setup();
  }
  
  if (this->dio0_pin_ != nullptr) {
    this->dio0_pin_->setup();
    this->dio0_pin_->attach_interrupt(BasicLoRaEx::dio0_isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  // Inicjalizacja mutexów
  this->queue_mutex_ = xSemaphoreCreateMutex();
  this->history_mutex_ = xSemaphoreCreateMutex();
  
  if (!this->queue_mutex_ || !this->history_mutex_) {
    ESP_LOGE(TAG, "Failed to create mutexes");
    return;
  }

  // Reset modułu i inicjalizacja LoRa
  this->reset_module();
  
  if (!this->init_lora()) {
    ESP_LOGE(TAG, "Failed to initialize LoRa module");
    return;
  }

  // Konfiguracja timera retry
  const esp_timer_create_args_t timer_args = {
    .callback = BasicLoRaEx::retry_timer_callback,
    .arg = this,
    .name = "lora_retry_timer"
  };
  
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &this->retry_timer_));
  ESP_ERROR_CHECK(esp_timer_start_periodic(this->retry_timer_, 400000)); // 400ms

  // Przejście do trybu odbiorczego
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
  this->receiving_ = true;

  ESP_LOGCONFIG(TAG, "LoRa initialized successfully");
  ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz", this->frequency_ / 1000000.0);
  ESP_LOGCONFIG(TAG, "  Spreading Factor: %d", this->spreading_factor_);
  ESP_LOGCONFIG(TAG, "  Bandwidth: %.1f kHz", this->bandwidth_ / 1000.0);
  ESP_LOGCONFIG(TAG, "  Coding Rate: 4/%d", this->coding_rate_);
  ESP_LOGCONFIG(TAG, "  TX Power: %d dBm", this->tx_power_);
}

void BasicLoRaEx::reset_module() {
  if (this->rst_pin_ != nullptr) {
    this->rst_pin_->digital_write(false);
    delay(10);
    this->rst_pin_->digital_write(true);
    delay(10);
  }
}

bool BasicLoRaEx::init_lora() {
  // Sprawdź wersję chipa
  uint8_t version = this->read_register(REG_VERSION);
  ESP_LOGD(TAG, "SX1278 version: 0x%02X", version);
  
  if (version != 0x12) {
    ESP_LOGE(TAG, "Invalid SX1278 version: 0x%02X (expected 0x12)", version);
    return false;
  }

  // Przejście do trybu sleep
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_SLEEP);
  delay(10);

  // Przejście do trybu standby
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
  delay(10);

  // Konfiguracja parametrów LoRa
  this->set_frequency_internal();
  this->set_spreading_factor_internal();
  this->set_bandwidth_internal();
  this->set_coding_rate_internal();
  this->set_tx_power_internal();

  // Konfiguracja preambuły
  this->write_register(REG_PREAMBLE_MSB, (this->preamble_length_ >> 8) & 0xFF);
  this->write_register(REG_PREAMBLE_LSB, this->preamble_length_ & 0xFF);

  // Sync word
  this->write_register(REG_SYNC_WORD, this->sync_word_);

  // Konfiguracja FIFO
  this->write_register(REG_FIFO_TX_BASE_ADDR, 0x00);
  this->write_register(REG_FIFO_RX_BASE_ADDR, 0x00);

  // Konfiguracja DIO0 dla RX/TX Done
  this->write_register(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone/TxDone

  // Konfiguracja CRC
  uint8_t modem_config_2 = this->read_register(REG_MODEM_CONFIG_2);
  if (this->enable_crc_) {
    modem_config_2 |= 0x04; // CRC On
  } else {
    modem_config_2 &= ~0x04; // CRC Off
  }
  this->write_register(REG_MODEM_CONFIG_2, modem_config_2);

  this->lora_initialized_ = true;
  return true;
}

void BasicLoRaEx::set_frequency_internal() {
  uint64_t frf = ((uint64_t)this->frequency_ << 19) / 32000000;
  this->write_register(REG_FRF_MSB, (uint8_t)(frf >> 16));
  this->write_register(REG_FRF_MID, (uint8_t)(frf >> 8));
  this->write_register(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void BasicLoRaEx::set_spreading_factor_internal() {
  uint8_t modem_config_2 = this->read_register(REG_MODEM_CONFIG_2);
  modem_config_2 = (modem_config_2 & 0x0F) | ((this->spreading_factor_ << 4) & 0xF0);
  this->write_register(REG_MODEM_CONFIG_2, modem_config_2);

  // Optimalizacja dla SF6
  if (this->spreading_factor_ == 6) {
    this->write_register(0x31, 0xC5);  // SF6 optimization
    this->write_register(0x37, 0x0C);  // SF6 optimization
  } else {
    this->write_register(0x31, 0xC3);  // Normal mode
    this->write_register(0x37, 0x0A);  // Normal mode
  }
}

void BasicLoRaEx::set_bandwidth_internal() {
  uint8_t bw_value;
  switch (this->bandwidth_) {
    case 7800:   bw_value = 0; break;
    case 10400:  bw_value = 1; break; 
    case 15600:  bw_value = 2; break;
    case 20800:  bw_value = 3; break;
    case 31250:  bw_value = 4; break;
    case 41700:  bw_value = 5; break;
    case 62500:  bw_value = 6; break;
    case 125000: bw_value = 7; break;
    case 250000: bw_value = 8; break;
    case 500000: bw_value = 9; break;
    default:     bw_value = 7; break; // 125kHz default
  }
  
  uint8_t modem_config_1 = this->read_register(REG_MODEM_CONFIG_1);
  modem_config_1 = (modem_config_1 & 0x0F) | (bw_value << 4);
  this->write_register(REG_MODEM_CONFIG_1, modem_config_1);
}

void BasicLoRaEx::set_coding_rate_internal() {
  uint8_t cr_value = this->coding_rate_ - 4; // 4/5 -> 1, 4/6 -> 2, etc.
  uint8_t modem_config_1 = this->read_register(REG_MODEM_CONFIG_1);
  modem_config_1 = (modem_config_1 & 0xF1) | (cr_value << 1);
  this->write_register(REG_MODEM_CONFIG_1, modem_config_1);
}

void BasicLoRaEx::set_tx_power_internal() {
  uint8_t pa_config;
  if (this->tx_power_ > 17) {
    // High power mode (PA_BOOST)
    pa_config = 0x80 | (this->tx_power_ - 2);
  } else {
    // Regular power mode (RFO)
    pa_config = 0x70 | this->tx_power_;
  }
  this->write_register(REG_PA_CONFIG, pa_config);
}

uint8_t BasicLoRaEx::read_register(uint8_t reg) {
  this->enable();
  this->write_byte(reg & 0x7F); // MSB = 0 for read
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void BasicLoRaEx::write_register(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg | 0x80); // MSB = 1 for write
  this->write_byte(value);
  this->disable();
}

void BasicLoRaEx::set_mode(uint8_t mode) {
  this->write_register(REG_OP_MODE, mode);
  delay(2); // Czekaj na zmianę trybu
}
// Kontynuacja basic_loraex.cpp...

void BasicLoRaEx::loop() {
  // Sprawdź czy są dane do odebrania
  if (this->receiving_) {
    std::vector<uint8_t> received_data;
    if (this->receive_packet(received_data)) {
      this->handle_received_data(received_data);
    }
  }
}

// API komunikacji - zgodność z ESP-NOW
void BasicLoRaEx::send_broadcast(const std::vector<uint8_t> &msg) {
  std::array<uint8_t, 6> broadcast_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  this->send_lora(msg, broadcast_mac);
}

void BasicLoRaEx::send_broadcast_str(const std::string &message) {
  std::vector<uint8_t> msg(message.begin(), message.end());
  this->send_broadcast(msg);
}

void BasicLoRaEx::send_to_peer(const std::vector<uint8_t> &msg) {
  this->send_lora(msg, this->peer_mac_);
}

void BasicLoRaEx::send_to_peer_str(const std::string &message) {
  std::vector<uint8_t> msg(message.begin(), message.end());
  this->send_to_peer(msg);
}

void BasicLoRaEx::send_lora_str(std::string message, const std::array<uint8_t, 6> &peer_mac) {
  std::vector<uint8_t> msg(message.begin(), message.end());
  this->send_lora(msg, peer_mac);
}

void BasicLoRaEx::send_lora_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac) {
  std::vector<uint8_t> msg(4);
  msg[0] = static_cast<uint8_t>((cmd >> 8) & 0xFF);
  msg[1] = static_cast<uint8_t>(cmd & 0xFF);
  msg[2] = msg[0]; // Duplikacja dla weryfikacji
  msg[3] = msg[1];
  
  if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
    // Sprawdź czy identyczna komenda już nie czeka w kolejce
    auto it = std::find_if(
      this->pending_messages_.begin(),
      this->pending_messages_.end(),
      [&](PendingMessage& m) {
        return m.mac == peer_mac && !m.acked && 
               m.payload.size() == 8 && // header(4) + cmd(4)
               std::equal(m.payload.begin() + 4, m.payload.begin() + 8, msg.begin());
      }
    );
    
    bool should_send = false;
    if (it != this->pending_messages_.end()) {
      // Reset retry counter dla istniejącej komendy
      it->retry_count = 0;
      it->timestamp = esp_timer_get_time();
    } else {
      should_send = true;
    }
    xSemaphoreGive(this->queue_mutex_);
    
    if (should_send) {
      this->send_lora(msg, peer_mac);
    }
  }
}

void BasicLoRaEx::send_lora(const std::vector<uint8_t>& msg, const std::array<uint8_t, 6>& peer_mac) {
  if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
    PendingMessage pending;
    pending.mac = peer_mac;
    pending.message_id = this->generate_message_id();
    pending.retry_count = 0;
    pending.timestamp = esp_timer_get_time();
    pending.acked = false;
    
    // Tworzenie pakietu: peer_mac(6) + type(1) + msg_id(3) + payload
    pending.payload.reserve(10 + msg.size());
    pending.payload.insert(pending.payload.end(), peer_mac.begin(), peer_mac.end());
    pending.payload.push_back(0x00); // Type: DATA
    pending.payload.insert(pending.payload.end(), pending.message_id.begin(), pending.message_id.end());
    pending.payload.insert(pending.payload.end(), msg.begin(), msg.end());
    
    this->pending_messages_.push_back(pending);
    xSemaphoreGive(this->queue_mutex_);
  }
  
  this->process_send_queue();
}

std::array<uint8_t, 3> BasicLoRaEx::generate_message_id() {
  uint32_t random_part = esp_random();
  int64_t timestamp = esp_timer_get_time();
  return {
    static_cast<uint8_t>((random_part ^ timestamp) & 0xFF),
    static_cast<uint8_t>((random_part >> 8 ^ timestamp >> 8) & 0xFF),
    static_cast<uint8_t>((random_part >> 16 ^ timestamp >> 16) & 0xFF)
  };
}

void BasicLoRaEx::process_send_queue() {
  const int64_t now = esp_timer_get_time();

  if (xSemaphoreTake(this->queue_mutex_, 0) == pdTRUE) {
    // Usuń potwierdzone lub przekroczone wiadomości
    this->pending_messages_.erase(
      std::remove_if(this->pending_messages_.begin(), this->pending_messages_.end(),
        [now, this](const PendingMessage& m) {
          return m.acked || ((now - m.timestamp) > this->timeout_us_ && 
                            m.retry_count >= this->max_retries_);
        }),
      this->pending_messages_.end()
    );

    for (auto& msg : this->pending_messages_) {
      if (!msg.acked && (now - msg.timestamp) > this->timeout_us_ && 
          msg.retry_count < this->max_retries_) {
        
        // Ponowna transmisja
        ESP_LOGD(TAG, "Retransmitting message ID %02X%02X%02X, attempt %d",
                 msg.message_id[0], msg.message_id[1], msg.message_id[2], 
                 msg.retry_count + 1);
        
        this->transmit_packet(msg.payload);
        msg.retry_count++;
        msg.timestamp = now;
      }
    }
    xSemaphoreGive(this->queue_mutex_);
  }
}

void BasicLoRaEx::transmit_packet(const std::vector<uint8_t> &data) {
  if (!this->lora_initialized_ || data.empty() || data.size() > 255) {
    return;
  }

  // Przejście do trybu standby
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
  this->receiving_ = false;

  // Reset flag IRQ
  this->write_register(REG_IRQ_FLAGS, 0xFF);

  // Ustaw adres FIFO na początek
  this->write_register(REG_FIFO_ADDR_PTR, 0);

  // Zapisz dane do FIFO
  this->enable();
  this->write_byte(REG_FIFO | 0x80);
  for (uint8_t byte : data) {
    this->write_byte(byte);
  }
  this->disable();

  // Ustaw długość payload
  this->write_register(REG_PAYLOAD_LENGTH, data.size());

  // Przejście do trybu TX
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_TX);
  
  ESP_LOGD(TAG, "Transmitting %d bytes", data.size());
}

bool BasicLoRaEx::receive_packet(std::vector<uint8_t> &data) {
  // Sprawdź flagę RxDone
  uint8_t irq_flags = this->read_register(REG_IRQ_FLAGS);
  
  if (!(irq_flags & IRQ_RX_DONE_MASK)) {
    return false; // Brak nowych danych
  }

  // Sprawdź błąd CRC
  if (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
    ESP_LOGW(TAG, "CRC error in received packet");
    this->write_register(REG_IRQ_FLAGS, IRQ_PAYLOAD_CRC_ERROR_MASK);
    return false;
  }

  // Odczytaj długość pakietu
  uint8_t packet_length = this->read_register(REG_RX_NB_BYTES);
  if (packet_length == 0) {
    this->write_register(REG_IRQ_FLAGS, IRQ_RX_DONE_MASK);
    return false;
  }

  // Odczytaj adres ostatniego pakietu w FIFO
  uint8_t fifo_addr = this->read_register(REG_FIFO_RX_CURRENT_ADDR);
  this->write_register(REG_FIFO_ADDR_PTR, fifo_addr);

  // Odczytaj dane z FIFO
  data.clear();
  data.reserve(packet_length);
  
  this->enable();
  this->write_byte(REG_FIFO & 0x7F);
  for (int i = 0; i < packet_length; i++) {
    data.push_back(this->read_byte());
  }
  this->disable();

  // Odczytaj RSSI i SNR
  this->last_rssi_ = this->read_register(REG_PKT_RSSI_VALUE) - 164;
  int8_t snr_raw = this->read_register(REG_PKT_SNR_VALUE);
  this->last_snr_ = snr_raw * 0.25;

  // Wyczyść flagę RxDone
  this->write_register(REG_IRQ_FLAGS, IRQ_RX_DONE_MASK);

  ESP_LOGD(TAG, "Received %d bytes, RSSI: %d dBm, SNR: %.1f dB", 
           packet_length, this->last_rssi_, this->last_snr_);

  return true;
}

void BasicLoRaEx::handle_received_data(const std::vector<uint8_t> &data) {
  if (data.size() < 10) { // Minimum: peer_mac(6) + type(1) + msg_id(3)
    ESP_LOGW(TAG, "Received packet too short: %d bytes", data.size());
    return;
  }

  // Wyciągnij adres nadawcy
  std::array<uint8_t, 6> sender_mac;
  std::copy_n(data.begin(), 6, sender_mac.begin());

  uint8_t packet_type = data[6];
  std::array<uint8_t, 3> msg_id{data[7], data[8], data[9]};

  // Obsługa ACK (10 bajtów: peer_mac + type(0x01) + msg_id)
  if (packet_type == 0x01 && data.size() == 10) {
    bool should_handle_ack = false;
    
    if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
      auto it = std::find_if(this->pending_messages_.begin(), this->pending_messages_.end(),
        [&](const PendingMessage& m) {
          return m.mac == sender_mac && m.message_id == msg_id && !m.acked;
        });
      
      if (it != this->pending_messages_.end()) {
        it->acked = true;
        should_handle_ack = true;
        ESP_LOGD(TAG, "ACK received for message %02X%02X%02X", msg_id[0], msg_id[1], msg_id[2]);
      }
      xSemaphoreGive(this->queue_mutex_);
    }
    
    if (should_handle_ack) {
      this->on_recv_ack_callback_.call(sender_mac, msg_id);
    }
    return;
  }

  // Obsługa danych (type = 0x00)
  if (packet_type != 0x00) {
    ESP_LOGW(TAG, "Unknown packet type: 0x%02X", packet_type);
    return;
  }

  // Sprawdź duplikaty
  int64_t now = esp_timer_get_time();
  bool is_duplicate = false;
  
  if (xSemaphoreTake(this->history_mutex_, portMAX_DELAY) == pdTRUE) {
    auto &history = this->received_history_;
    
    // Usuń stare wpisy (>300s)
    history.erase(
      std::remove_if(history.begin(), history.end(),
        [now](const ReceivedMessageInfo &info) {
          return (now - info.timestamp) > 300000000; // 300s
        }),
      history.end());

    // Sprawdź czy wiadomość już była odebrana
    is_duplicate = std::any_of(history.begin(), history.end(),
      [&](const ReceivedMessageInfo &info) {
        return info.mac == sender_mac && info.data == data;
      });

    if (!is_duplicate) {
      constexpr size_t MAX_HISTORY_SIZE = 1000;
      if (history.size() >= MAX_HISTORY_SIZE) {
        history.erase(history.begin());
      }
      history.push_back({sender_mac, now, data});
    }
    
    xSemaphoreGive(this->history_mutex_);
  }

  if (is_duplicate) {
    ESP_LOGD(TAG, "Duplicate message ignored");
    return;
  }

  // Wyślij ACK
  this->send_ack(sender_mac, msg_id);

  // Przetwórz payload
  std::vector<uint8_t> payload(data.begin() + 10, data.end());

  // Dekodowanie komendy (payload 4 bajty, pierwszy dwój == ostatni dwój)
  if (payload.size() == 4 && memcmp(payload.data(), payload.data() + 2, 2) == 0) {
    int16_t cmd = (payload[0] << 8) | payload[1];
    this->on_recv_cmd_callback_.call(sender_mac, cmd);
    ESP_LOGD(TAG, "Command received: %d", cmd);
  }

  // Przekaż dane do callbacków
  this->on_recv_data_callback_.call(sender_mac, payload);
  
  if (!payload.empty()) {
    std::string message(payload.begin(), payload.end());
    this->on_message_callback_.call(sender_mac, message);
  }
}

void BasicLoRaEx::send_ack(const std::array<uint8_t, 6> &peer_mac, const std::array<uint8_t, 3> &msg_id) {
  std::vector<uint8_t> ack_packet;
  ack_packet.reserve(10);
  
  // peer_mac(6) + type(1) + msg_id(3)
  ack_packet.insert(ack_packet.end(), peer_mac.begin(), peer_mac.end());
  ack_packet.push_back(0x01); // Type: ACK
  ack_packet.insert(ack_packet.end(), msg_id.begin(), msg_id.end());
  
  this->transmit_packet(ack_packet);
  
  // Powróć do trybu odbioru
  delay(10);
  this->set_mode(MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
  this->receiving_ = true;
}

// Obsługa przerwań
void IRAM_ATTR BasicLoRaEx::dio0_isr(void *arg) {
  BasicLoRaEx *instance = static_cast<BasicLoRaEx*>(arg);
  instance->handle_dio0_interrupt();
}

void BasicLoRaEx::handle_dio0_interrupt() {
  // Przerwanie DIO0 - RxDone lub TxDone
  uint8_t irq_flags = this->read_register(REG_IRQ_FLAGS);
  
  if (irq_flags & IRQ_TX_DONE_MASK) {
    // Transmisja zakończona
    this->write_register(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    
    // Powróć do trybu odbioru
    this->set_mode(MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
    this->receiving_ = true;
    
    ESP_LOGD(TAG, "TX Done");
  }
}

// Timer callback
void BasicLoRaEx::retry_timer_callback(void *arg) {
  BasicLoRaEx *instance = static_cast<BasicLoRaEx*>(arg);
  instance->process_send_queue();
}

void BasicLoRaEx::clear_pending_messages() {
  if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
    this->pending_messages_.clear();
    xSemaphoreGive(this->queue_mutex_);
  }
}

size_t BasicLoRaEx::get_pending_count() {
  size_t count = 0;
  if (xSemaphoreTake(this->queue_mutex_, portMAX_DELAY) == pdTRUE) {
    count = this->pending_messages_.size();
    xSemaphoreGive(this->queue_mutex_);
  }
  return count;
}

// Implementacja triggerów
OnMessageTrigger::OnMessageTrigger(BasicLoRaEx *parent) {
  parent->add_on_message_callback([this](const std::array<uint8_t, 6> mac, const std::string message) {
    trigger(mac, message);
  });
}

OnRecvAckTrigger::OnRecvAckTrigger(BasicLoRaEx *parent) {
  parent->add_on_recv_ack_callback([this](const std::array<uint8_t, 6> mac, std::array<uint8_t, 3> msg_id) {
    trigger(mac, msg_id);
  });
}

OnRecvCmdTrigger::OnRecvCmdTrigger(BasicLoRaEx *parent) {
  parent->add_on_recv_cmd_callback([this](const std::array<uint8_t, 6> mac, const int16_t cmd) {
    trigger(mac, cmd);
  });
}

OnRecvDataTrigger::OnRecvDataTrigger(BasicLoRaEx *parent) {
  parent->add_on_recv_data_callback([this](const std::array<uint8_t, 6> mac, const std::vector<uint8_t> data) {
    trigger(mac, data);
  });
}

BasicLoRaEx::~BasicLoRaEx() {
  if (this->retry_timer_) {
    esp_timer_stop(this->retry_timer_);
    esp_timer_delete(this->retry_timer_);
  }
  
  if (this->queue_mutex_) {
    vSemaphoreDelete(this->queue_mutex_);
  }
  
  if (this->history_mutex_) {
    vSemaphoreDelete(this->history_mutex_);
  }
}

}  // namespace lora
}  // namespace esphome

