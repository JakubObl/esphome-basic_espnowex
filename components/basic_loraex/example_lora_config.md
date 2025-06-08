# Przykład konfiguracji YAML dla komponentu basic_loraex

```yaml
# example_lora_config.yaml
esphome:
  name: lora-node
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "Your-WiFi-SSID"
  password: "Your-WiFi-Password"

# Enable logging
logger:
  level: DEBUG

# Enable Home Assistant API
api:
  password: "api_password"

ota:
  password: "ota_password"

# Konfiguracja SPI dla LoRa SX1278
spi:
  clk_pin: GPIO18    # SCK
  mosi_pin: GPIO23   # MOSI  
  miso_pin: GPIO19   # MISO

# Komponent basic_loraex
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5      # NSS (Chip Select)
  dio0_pin: GPIO2    # DIO0 (interrupt pin)
  rst_pin: GPIO14    # Reset pin
  
  # Podstawowe parametry komunikacji
  peer_id: [0x01, 0x02, 0x03, 0x04, 0x05, 0x06]  # ID tego urządzenia
  max_retries: 3
  timeout_ms: 5000
  
  # Parametry specyficzne dla SX1278 LoRa
  frequency: 433000000          # 433 MHz (zmień zgodnie z regionem)
  spreading_factor: 9           # SF9 (6-12)
  bandwidth: 125000            # 125 kHz
  coding_rate: 7               # 4/7
  tx_power: 14                 # 14 dBm (2-20)
  sync_word: 0x12              # Private network
  preamble_length: 8           # 8 symbols
  enable_crc: true             # Włącz CRC
  implicit_header: false       # Explicit header mode
  
  # Triggery dla różnych typów wiadomości
  on_message:
    - trigger_id: msg_trigger
      then:
        - logger.log:
            format: "Received message from %02X:%02X:%02X:%02X:%02X:%02X: %s"
            args:
              - x[0]
              - x[1] 
              - x[2]
              - x[3]
              - x[4]
              - x[5]
              - message.c_str()
        
  on_recv_cmd:
    - trigger_id: cmd_trigger
      then:
        - logger.log:
            format: "Received command from %02X:%02X:%02X:%02X:%02X:%02X: %d"
            args:
              - x[0]
              - x[1]
              - x[2] 
              - x[3]
              - x[4]
              - x[5]
              - cmd
        - if:
            condition:
              lambda: 'return cmd == 100;'
            then:
              - switch.turn_on: relay_1
        - if:
            condition:
              lambda: 'return cmd == 101;'
            then:
              - switch.turn_off: relay_1
              
  on_recv_ack:
    - trigger_id: ack_trigger
      then:
        - logger.log:
            format: "Message acknowledged by %02X:%02X:%02X:%02X:%02X:%02X"
            args:
              - x[0]
              - x[1]
              - x[2]
              - x[3] 
              - x[4]
              - x[5]

# Przykładowe sensory i switche
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Temperature"
      id: temp_sensor
      on_value:
        then:
          # Wysłanie temperatury przez LoRa co 5 minut
          - if:
              condition:
                lambda: 'return (millis() % 300000) < 1000;'  # co 5 minut
              then:
                - lambda: |-
                    std::string temp_msg = "TEMP:" + to_string(x);
                    id(lora_radio).send_broadcast_str(temp_msg);
    humidity:
      name: "Humidity"

switch:
  - platform: gpio
    pin: GPIO16
    name: "Relay 1"
    id: relay_1
    on_turn_on:
      then:
        - lambda: |-
            id(lora_radio).send_broadcast_str("RELAY1:ON");
    on_turn_off:
      then:
        - lambda: |-
            id(lora_radio).send_broadcast_str("RELAY1:OFF");

binary_sensor:
  - platform: gpio
    pin: 
      number: GPIO0
      inverted: true
      mode:
        input: true
        pullup: true
    name: "Button"
    on_press:
      then:
        # Wysłanie komendy przez LoRa do konkretnego peer
        - lambda: |-
            std::array<uint8_t, 6> target_peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x07};
            id(lora_radio).send_lora_cmd(200, target_peer);

# Automatyzacja do wysyłania heartbeat
interval:
  - interval: 60s
    then:
      - lambda: |-
          std::string heartbeat = "HEARTBEAT:" + to_string(millis());
          id(lora_radio).send_broadcast_str(heartbeat);

# Status LED
status_led:
  pin: GPIO2

# Przykład różnych regionów częstotliwości (odkomentuj odpowiedni)
# Europa 868 MHz:
# frequency: 868000000

# USA 915 MHz:  
# frequency: 915000000

# Azja 433 MHz (domyślny):
# frequency: 433000000
```

## Dodatkowe przykłady użycia w kodzie lambda:

```yaml
# Wysłanie wiadomości tekstowej do konkretnego peer
- lambda: |-
    std::array<uint8_t, 6> peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    id(lora_radio).send_lora_str("Hello World", peer);

# Wysłanie komendy do peer
- lambda: |-
    std::array<uint8_t, 6> peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    id(lora_radio).send_lora_cmd(123, peer);

# Broadcast wiadomości
- lambda: |-
    id(lora_radio).send_broadcast_str("Emergency Alert");

# Sprawdzenie liczby oczekujących wiadomości
- lambda: |-
    size_t pending = id(lora_radio).get_pending_count();
    ESP_LOGI("main", "Pending messages: %zu", pending);

# Wyczyszczenie kolejki oczekujących wiadomości
- lambda: |-
    id(lora_radio).clear_pending_messages();
```