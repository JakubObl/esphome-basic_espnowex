#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esp_timer.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace esphome {
namespace lora {

// Rejestry SX1278
static const uint8_t REG_FIFO = 0x00;
static const uint8_t REG_OP_MODE = 0x01;
static const uint8_t REG_FRF_MSB = 0x06;
static const uint8_t REG_FRF_MID = 0x07;
static const uint8_t REG_FRF_LSB = 0x08;
static const uint8_t REG_PA_CONFIG = 0x09;
static const uint8_t REG_LNA = 0x0C;
static const uint8_t REG_FIFO_ADDR_PTR = 0x0D;
static const uint8_t REG_FIFO_TX_BASE_ADDR = 0x0E;
static const uint8_t REG_FIFO_RX_BASE_ADDR = 0x0F;
static const uint8_t REG_FIFO_RX_CURRENT_ADDR = 0x10;
static const uint8_t REG_IRQ_FLAGS_MASK = 0x11;
static const uint8_t REG_IRQ_FLAGS = 0x12;
static const uint8_t REG_RX_NB_BYTES = 0x13;
static const uint8_t REG_PKT_RSSI_VALUE = 0x1A;
static const uint8_t REG_PKT_SNR_VALUE = 0x1B;
static const uint8_t REG_MODEM_CONFIG_1 = 0x1D;
static const uint8_t REG_MODEM_CONFIG_2 = 0x1E;
static const uint8_t REG_PREAMBLE_MSB = 0x20;
static const uint8_t REG_PREAMBLE_LSB = 0x21;
static const uint8_t REG_PAYLOAD_LENGTH = 0x22;
static const uint8_t REG_MODEM_CONFIG_3 = 0x26;
static const uint8_t REG_RSSI_VALUE = 0x1C;
static const uint8_t REG_DIO_MAPPING_1 = 0x40;
static const uint8_t REG_DIO_MAPPING_2 = 0x41;
static const uint8_t REG_VERSION = 0x42;
static const uint8_t REG_SYNC_WORD = 0x39;

// Tryby pracy
static const uint8_t MODE_LONG_RANGE_MODE = 0x80;
static const uint8_t MODE_SLEEP = 0x00;
static const uint8_t MODE_STDBY = 0x01;
static const uint8_t MODE_TX = 0x03;
static const uint8_t MODE_RX_CONTINUOUS = 0x05;
static const uint8_t MODE_RX_SINGLE = 0x06;

// IRQ flagi
static const uint8_t IRQ_TX_DONE_MASK = 0x08;
static const uint8_t IRQ_PAYLOAD_CRC_ERROR_MASK = 0x20;
static const uint8_t IRQ_RX_DONE_MASK = 0x40;

struct PendingMessage {
  std::array<uint8_t, 6> mac;
  std::array<uint8_t, 3> message_id;
  uint8_t retry_count;
  int64_t timestamp;
  bool acked;
  std::vector<uint8_t> payload;
};

struct ReceivedMessageInfo {
    std::array<uint8_t, 6> mac;
    int64_t timestamp;
    std::vector<uint8_t> data;
};

class BasicLoRaEx;

class OnMessageTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, std::string>, public Component {
  public:
    explicit OnMessageTrigger(BasicLoRaEx *parent);
};

class OnRecvAckTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, std::array<uint8_t, 3>>, public Component {
  public:
    explicit OnRecvAckTrigger(BasicLoRaEx *parent);
};

class OnRecvCmdTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, int16_t>, public Component {
  public:
    explicit OnRecvCmdTrigger(BasicLoRaEx *parent);
};

class OnRecvDataTrigger : public ::esphome::Trigger<std::array<uint8_t, 6>, std::vector<uint8_t>>, public Component {
  public:
    explicit OnRecvDataTrigger(BasicLoRaEx *parent);
};

class BasicLoRaEx : public Component, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, 
                                                           spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Konfiguracja pinów
  void set_cs_pin(GPIOPin *cs_pin) { this->cs_pin_ = cs_pin; }
  void set_dio0_pin(GPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_dio1_pin(GPIOPin *dio1_pin) { this->dio1_pin_ = dio1_pin; }
  void set_dio2_pin(GPIOPin *dio2_pin) { this->dio2_pin_ = dio2_pin; }
  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }

  // Konfiguracja parametrów LoRa
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_spreading_factor(uint8_t sf) { this->spreading_factor_ = sf; }
  void set_bandwidth(uint32_t bandwidth) { this->bandwidth_ = bandwidth; }
  void set_coding_rate(uint8_t cr) { this->coding_rate_ = cr; }
  void set_tx_power(uint8_t power) { this->tx_power_ = power; }
  void set_sync_word(uint8_t sync_word) { this->sync_word_ = sync_word; }
  void set_preamble_length(uint16_t length) { this->preamble_length_ = length; }
  void set_enable_crc(bool enable) { this->enable_crc_ = enable; }
  void set_implicit_header(bool implicit) { this->implicit_header_ = implicit; }

  // Konfiguracja komunikacji
  void set_peer_mac(std::array<uint8_t, 6> mac) { this->peer_mac_ = mac; }
  void set_max_retries(uint8_t max_retries) { this->max_retries_ = max_retries; }
  void set_timeout_us(int64_t timeout_us) { this->timeout_us_ = timeout_us * 1000; }

  // API komunikacji (zgodność z ESP-NOW)
  void send_broadcast(const std::vector<uint8_t> &msg);
  void send_broadcast_str(const std::string &message);
  void send_to_peer(const std::vector<uint8_t> &msg);
  void send_to_peer_str(const std::string &message);
  void send_lora_str(std::string message, const std::array<uint8_t, 6> &peer_mac);
  void send_lora(const std::vector<uint8_t> &msg, const std::array<uint8_t, 6> &peer_mac);
  void send_lora_cmd(int16_t cmd, const std::array<uint8_t, 6> &peer_mac);
  void clear_pending_messages();
  size_t get_pending_count();

  // Callbacki automatyzacji
  void add_on_message_callback(std::function<void(std::array<uint8_t,6>, std::string)> &&cb) {
    this->on_message_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, std::string)> on_message_callback_;

  void add_on_recv_ack_callback(std::function<void(std::array<uint8_t,6>, std::array<uint8_t, 3>)> &&cb) {
    this->on_recv_ack_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, std::array<uint8_t, 3>)> on_recv_ack_callback_;

  void add_on_recv_cmd_callback(std::function<void(std::array<uint8_t,6>, int16_t)> &&cb) {
    this->on_recv_cmd_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, int16_t)> on_recv_cmd_callback_;
  
  void add_on_recv_data_callback(std::function<void(std::array<uint8_t,6>, std::vector<uint8_t>)> &&cb) {
    this->on_recv_data_callback_.add(std::move(cb));
  }
  CallbackManager<void(std::array<uint8_t,6>, std::vector<uint8_t>)> on_recv_data_callback_;

  ~BasicLoRaEx();

 protected:
  // Hardware
  GPIOPin *cs_pin_{nullptr};
  GPIOPin *dio0_pin_{nullptr};
  GPIOPin *dio1_pin_{nullptr};
  GPIOPin *dio2_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};

  // Parametry LoRa  
  uint32_t frequency_{433000000};
  uint8_t spreading_factor_{9};
  uint32_t bandwidth_{125000};
  uint8_t coding_rate_{7};
  uint8_t tx_power_{14};
  uint8_t sync_word_{0x12};
  uint16_t preamble_length_{8};
  bool enable_crc_{true};
  bool implicit_header_{false};

  // Parametry komunikacji
  std::array<uint8_t, 6> peer_mac_{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  uint8_t max_retries_{5};
  int64_t timeout_us_{200000000}; // 200ms w mikrosekundach

  // Zarządzanie wiadomościami
  std::vector<PendingMessage> pending_messages_;
  std::vector<ReceivedMessageInfo> received_history_;
  SemaphoreHandle_t queue_mutex_;
  SemaphoreHandle_t history_mutex_;
  esp_timer_handle_t retry_timer_;

  // Stan LoRa
  bool lora_initialized_{false};
  bool receiving_{false};
  int last_rssi_{0};
  float last_snr_{0.0};

  // Funkcje niskopoziomowe SX1278
  void reset_module();
  bool init_lora();
  void set_mode(uint8_t mode);
  uint8_t read_register(uint8_t reg);
  void write_register(uint8_t reg, uint8_t value);
  void set_frequency_internal();
  void set_spreading_factor_internal();
  void set_bandwidth_internal();
  void set_coding_rate_internal();
  void set_tx_power_internal();
  void handle_dio0_interrupt();
  
  // Funkcje komunikacji
  std::array<uint8_t, 3> generate_message_id();
  void process_send_queue();
  void transmit_packet(const std::vector<uint8_t> &data);
  bool receive_packet(std::vector<uint8_t> &data);
  void handle_received_data(const std::vector<uint8_t> &data);
  void send_ack(const std::array<uint8_t, 6> &peer_mac, const std::array<uint8_t, 3> &msg_id);

  // Przerwania i timery
  static void IRAM_ATTR dio0_isr(void *arg);
  static void retry_timer_callback(void *arg);

  static BasicLoRaEx *instance_;
};

}  // namespace lora
}  // namespace esphome
