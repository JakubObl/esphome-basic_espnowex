# Przykład konfiguracji YAML dla komponentu basic_loraex

## Pełna konfiguracja basic_loraex.yaml

```yaml
# Konfiguracja SPI dla komunikacji z modułem SX1278
spi:
  clk_pin: GPIO18    # SCK
  mosi_pin: GPIO23   # MOSI
  miso_pin: GPIO19   # MISO

# Komponent basic_loraex z pełną konfiguracją
basic_loraex:
  id: lora_radio
  
  # Piny SPI i kontrolne
  cs_pin: GPIO5      # NSS (Chip Select)
  dio0_pin: GPIO2    # DIO0 (przerwanie RX/TX Done)
  dio1_pin: GPIO15   # DIO1 (opcjonalnie)
  rst_pin: GPIO14    # RST (reset modułu)
  
  # Parametry RF LoRa
  frequency: 433000000      # 433 MHz (Europa: 868MHz, USA: 915MHz)
  spreading_factor: 9       # SF7-SF12 (9 = dobry kompromis)
  bandwidth: 125000         # 125 kHz (standardowa szerokość)
  coding_rate: 7           # 4/7 (dobra korekcja błędów)
  tx_power: 14             # 14 dBm (maksymalnie 20)
  sync_word: 0x12          # Słowo synchronizacji sieci
  preamble_length: 8       # Długość preambuły
  enable_crc: true         # Włącz kontrolę CRC
  implicit_header: false   # Explicit header mode
  
  # Parametry komunikacji
  peer_mac: "01:02:03:04:05:06"  # Domyślny adres peer
  max_retries: 5                  # Maksymalne retransmisje
  timeout_us: 200000             # Timeout w mikrosekundach
  
  # Triggery automatyzacji
  on_message:
    - then:
        - logger.log: 
            format: "Received message from %s: %s"
            args: 
              - 'x.c_str()'
              - 'message.c_str()'
            level: INFO
  
  on_recv_cmd:
    - then:
        - logger.log:
            format: "Received command %d from %s"
            args:
              - 'cmd'
              - 'x.c_str()'
            level: INFO
            
  on_recv_ack:
    - then:
        - logger.log: "ACK received"
        
  on_recv_data:
    - then:
        - logger.log: "Binary data received"

# Przykładowe automatyzacje
automation:
  - alias: "Send periodic message"
    trigger:
      - platform: interval
        seconds: 30
    action:
      - lambda: |-
          id(lora_radio).send_broadcast_str("Hello LoRa World!");
          
  - alias: "Send command to specific peer"  
    trigger:
      - platform: homeassistant
        event: lora_send_command
    action:
      - lambda: |-
          std::array<uint8_t, 6> peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(123, peer);

# Sensor do monitorowania stanu
sensor:
  - platform: template
    name: "LoRa Pending Messages"
    lambda: |-
      return id(lora_radio).get_pending_count();
    update_interval: 10s
    
text_sensor:
  - platform: template
    name: "LoRa Status"
    lambda: |-
      return "Connected";
    update_interval: 60s
```

## Minimalna konfiguracja

```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23  
  miso_pin: GPIO19

basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 433000000
```

## Konfiguracja zaawansowana dla dużego zasięgu

```yaml
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  
  # Parametry dla maksymalnego zasięgu
  frequency: 433000000
  spreading_factor: 12      # Najwolszy, największy zasięg
  bandwidth: 125000         # Standardowa szerokość
  coding_rate: 8           # 4/8 - maksymalna korekcja błędów
  tx_power: 20             # Maksymalna moc
  enable_crc: true
  
  # Dłuższe timeouty dla powolnej transmisji
  timeout_us: 2000000      # 2 sekundy
  max_retries: 3
```