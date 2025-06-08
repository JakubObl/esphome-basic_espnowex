# Dokumentacja komponentu basic_loraex

## Opis

Komponent `basic_loraex` jest implementacją ESPHome zapewniającą komunikację LoRa używając modułu SX1278. Komponent został stworzony na bazie komponentu ESP-NOW i zapewnia podobną funkcjonalność, ale z wykorzystaniem technologii LoRa do transmisji długodystansowej.

## Funkcjonalności

### Podstawowe funkcje komunikacji
- Wysyłanie i odbieranie wiadomości tekstowych
- Wysyłanie i odbieranie komend liczbowych  
- Broadcast do wszystkich urządzeń
- Komunikacja punkt-punkt z określonym peer
- System potwierdzeń (ACK) z automatycznymi retransmisjami
- Deduplicja wiadomości
- Obsługa timeoutów i retry

### Parametry konfiguracyjne SX1278
- **Częstotliwość** (frequency): 137-1020 MHz
- **Spreading Factor** (spreading_factor): 6-12
- **Szerokość pasma** (bandwidth): 7.8 kHz - 500 kHz
- **Coding Rate** (coding_rate): 4/5 - 4/8
- **Moc transmisji** (tx_power): 2-20 dBm
- **Sync Word** (sync_word): Słowo synchronizacji
- **Długość preambuły** (preamble_length): Liczba symboli
- **CRC**: Włączenie/wyłączenie kontroli CRC
- **Tryb nagłówka**: Explicit/Implicit

## Instalacja

1. Utwórz folder `custom_components/basic_loraex/` w katalogu z konfiguracją ESPHome
2. Skopiuj pliki:
   - `__init__.py`
   - `basic_loraex.h` 
   - `basic_loraex.cpp`

## Konfiguracja hardware'u

### Połączenia SX1278 z ESP32

| SX1278 Pin | ESP32 Pin | Opis |
|------------|-----------|------|
| VCC | 3.3V | Zasilanie |
| GND | GND | Masa |
| SCK | GPIO18 | SPI Clock |
| MISO | GPIO19 | SPI Master In Slave Out |
| MOSI | GPIO23 | SPI Master Out Slave In |
| NSS | GPIO5 | SPI Chip Select |
| DIO0 | GPIO2 | Przerwanie |
| RST | GPIO14 | Reset |

**Uwaga:** Piny można zmienić w konfiguracji YAML.

## Konfiguracja YAML

### Minimalna konfiguracja

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
```

### Pełna konfiguracja z wszystkimi parametrami

```yaml
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  
  # Identyfikacja urządzenia
  peer_id: [0x01, 0x02, 0x03, 0x04, 0x05, 0x06]
  
  # Parametry protokołu
  max_retries: 5
  timeout_ms: 2000
  
  # Parametry LoRa
  frequency: 433000000      # Hz
  spreading_factor: 9       # 6-12
  bandwidth: 125000        # Hz
  coding_rate: 7           # 5-8 (4/5 - 4/8)
  tx_power: 14             # dBm
  sync_word: 0x12          # hex
  preamble_length: 8       # symbols
  enable_crc: true
  implicit_header: false
```

## Częstotliwości według regionów

| Region | Częstotliwość | Uwagi |
|--------|---------------|-------|
| Europa | 433 MHz, 868 MHz | Duty cycle 1-10% |
| USA | 915 MHz | Brak ograniczeń duty cycle |
| Azja | 433 MHz | Sprawdź lokalne przepisy |
| Australia | 915 MHz | Sprawdź lokalne przepisy |

## API funkcji

### Wysyłanie wiadomości

```cpp
// Broadcast tekstowy
id(lora_radio).send_broadcast_str("Hello World");

// Broadcast binarny
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
id(lora_radio).send_broadcast(data);

// Do konkretnego peer
std::array<uint8_t, 6> peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
id(lora_radio).send_lora_str("Message", peer);

// Komenda do peer
id(lora_radio).send_lora_cmd(123, peer);

// Do domyślnego peer
id(lora_radio).send_to_peer_str("Message");
```

### Zarządzanie kolejką

```cpp
// Liczba oczekujących wiadomości
size_t pending = id(lora_radio).get_pending_count();

// Wyczyszczenie kolejki
id(lora_radio).clear_pending_messages();
```

## Triggery i automatyzacje

### on_message
Wywoływany przy odbiorze wiadomości tekstowej.

```yaml
on_message:
  - then:
      - logger.log:
          format: "Msg from %02X:%02X:%02X:%02X:%02X:%02X: %s"
          args: [x[0], x[1], x[2], x[3], x[4], x[5], message.c_str()]
```

### on_recv_cmd  
Wywoływany przy odbiorze komendy liczbowej.

```yaml
on_recv_cmd:
  - then:
      - if:
          condition:
            lambda: 'return cmd == 100;'
          then:
            - switch.turn_on: relay_1
```

### on_recv_ack
Wywoływany gdy odebrano potwierdzenie wiadomości.

```yaml
on_recv_ack:
  - then:
      - logger.log: "Message acknowledged"
```

### on_recv_data
Wywoływany przy odbiorze danych binarnych.

```yaml
on_recv_data:
  - then:
      - lambda: |-
          ESP_LOGI("lora", "Received %zu bytes", data.size());
```

## Protokół komunikacji

### Format pakietu
```
[Peer ID - 6B][Type - 1B][Message ID - 3B][Payload - NB]
```

- **Peer ID**: Identyfikator nadawcy
- **Type**: 0x00 = dane, 0x01 = ACK
- **Message ID**: Unikalny identyfikator wiadomości
- **Payload**: Treść wiadomości

### Potwierdzenia ACK
- Każda wiadomość automatycznie generuje ACK
- Retransmisje przy braku ACK
- Konfigurowalne timeout i liczba prób

### Deduplicja
- Historia odebranych wiadomości (300s)
- Filtrowanie duplikatów na podstawie peer ID i treści

## Optymalizacja wydajności

### Wybór Spreading Factor
- **SF7**: Najszybszy, najmniejszy zasięg
- **SF12**: Najwolniejszy, największy zasięg
- Kompromis między prędkością a zasięgiem

### Szerokość pasma
- **125 kHz**: Standard, dobry kompromis
- **250/500 kHz**: Wyższa prędkość, mniejszy zasięg
- **62.5 kHz i mniej**: Większy zasięg, wolniejsze

### Moc transmisji
- Dostosuj do potrzeb zasięgu
- Niższa moc = dłuższa praca na baterii
- Uwzględnij lokalne przepisy

## Rozwiązywanie problemów

### Brak komunikacji
1. Sprawdź połączenia SPI
2. Sprawdź piny DIO0 i RST
3. Sprawdź częstotliwość (zgodność z regionem)
4. Sprawdź sync_word (musi być identyczny)

### Słaba jakość sygnału
1. Sprawdź antenę
2. Dostosuj spreading factor
3. Zwiększ moc transmisji
4. Sprawdź przeszkody

### Wysokie zużycie baterii
1. Zmniejsz moc transmisji
2. Zwiększ interwał wysyłania
3. Użyj trybu sleep między transmisja
4. Optymalizuj długość wiadomości

## Przykłady zastosowań

### Sensor temperatury z długim zasięgiem
```yaml
sensor:
  - platform: dht
    temperature:
      on_value:
        - lambda: |-
            std::string msg = "TEMP:" + to_string(x);
            id(lora_radio).send_broadcast_str(msg);
```

### Zdalny przełącznik
```yaml
binary_sensor:
  - platform: gpio
    pin: GPIO0
    on_press:
      - lambda: |-
          std::array<uint8_t, 6> peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(100, peer);
```

### Gateway do MQTT
```yaml
on_message:
  - then:
      - mqtt.publish:
          topic: !lambda 'return "lora/" + to_string(x[0]) + "/data";'
          payload: !lambda 'return message;'
```

## Ograniczenia

- Maksymalna długość pakietu: 255 bajtów
- Duty cycle (EU): 1-10% depending on band
- Brak obsługi LoRaWAN (tylko peer-to-peer)
- Wymagane identyczne parametry LoRa dla komunikacji

## Bezpieczeństwo

- Brak wbudowanego szyfrowania
- Sync word jako podstawowa ochrona
- Rozważ implementację warstwy szyfrowania na poziomie aplikacji
- Unikaj transmisji wrażliwych danych w plain text