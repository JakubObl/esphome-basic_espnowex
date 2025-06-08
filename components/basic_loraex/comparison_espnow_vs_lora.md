# Porównanie komponentów basic_espnowex vs basic_loraex

## Przegląd zmian

Komponent `basic_loraex` został stworzony na bazie `basic_espnowex`, zachowując tą samą funkcjonalność ale zmieniając medium transmisji z ESP-NOW na LoRa SX1278.

## Główne różnice

### 1. Medium transmisji

| Aspekt | basic_espnowex | basic_loraex |
|--------|----------------|--------------|
| Technologia | ESP-NOW (2.4 GHz) | LoRa SX1278 (433/868/915 MHz) |
| Zasięg | ~200m | ~10 km (w optymalnych warunkach) |
| Prędkość | Wysoka (do 1 Mbps) | Niska (0.3-50 kbps) |
| Zużycie energii | Średnie | Bardzo niskie |
| Przepuszczość | 250 bajtów/pakiet | 255 bajtów/pakiet |

### 2. Konfiguracja hardware

#### ESP-NOW (basic_espnowex)
```yaml
# Brak dodatkowego hardware - używa wbudowanego WiFi
basic_espnowex:
  peer_mac: "AA:BB:CC:DD:EE:FF"
```

#### LoRa (basic_loraex)
```yaml
# Wymaga modułu SX1278 podłączonego przez SPI
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

basic_loraex:
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  peer_id: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]
```

### 3. Parametry konfiguracyjne

#### ESP-NOW
```yaml
basic_espnowex:
  peer_mac: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]
  max_retries: 5
  timeout_us: 200000  # mikrosekundy
```

#### LoRa
```yaml
basic_loraex:
  peer_id: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]
  max_retries: 5
  timeout_ms: 2000    # milisekundy
  
  # Dodatkowe parametry specyficzne dla LoRa
  frequency: 433000000
  spreading_factor: 9
  bandwidth: 125000
  coding_rate: 7
  tx_power: 14
  sync_word: 0x12
  preamble_length: 8
  enable_crc: true
  implicit_header: false
```

### 4. Identyfikacja urządzeń

| Komponent | Identyfikator | Format | Przykład |
|-----------|---------------|--------|----------|
| ESP-NOW | MAC Address | 6 bajtów hex | `AA:BB:CC:DD:EE:FF` |
| LoRa | Peer ID | 6 bajtów array | `[0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]` |

### 5. API - funkcje wysyłania

#### ESP-NOW
```cpp
// API funkcje
esp_now.send_broadcast_str("message");
esp_now.send_to_peer_str("message");
esp_now.send_espnow_str("message", peer_mac);
esp_now.send_espnow_cmd(123, peer_mac);
```

#### LoRa  
```cpp
// Analogiczne API funkcje
lora.send_broadcast_str("message");
lora.send_to_peer_str("message");
lora.send_lora_str("message", peer_id);
lora.send_lora_cmd(123, peer_id);
```

### 6. Triggery

Oba komponenty mają identyczne triggery:

```yaml
on_message:      # Odbiór wiadomości tekstowej
on_recv_cmd:     # Odbiór komendy liczbowej  
on_recv_ack:     # Odbiór potwierdzenia
on_recv_data:    # Odbiór danych binarnych
```

### 7. Protokół komunikacji

#### Podobieństwa:
- System ACK z retransmisjami
- Generowanie Message ID
- Deduplicja wiadomości
- Thread-safe kolejki z mutex

#### Różnice:

| Aspekt | ESP-NOW | LoRa |
|--------|---------|------|
| Format pakietu | `[0x00][msg_id][payload]` | `[peer_id][0x00][msg_id][payload]` |
| Mutex | FreeRTOS semaphore | FreeRTOS semaphore |
| Timer | esp_timer (400ms) | esp_timer (1000ms) |
| Historia | 300s cache | 300s cache |

### 8. Implementacja hardware

#### ESP-NOW (basic_espnowex)
```cpp
// Używa WiFi stack
esp_wifi_init(&cfg);
esp_now_init();
esp_now_register_recv_cb(&recv_cb);
esp_now_send(peer_addr, data, len);
```

#### LoRa (basic_loraex)
```cpp
// Bezpośrednia komunikacja SPI z SX1278
this->write_register(REG_OP_MODE, MODE_TX);
this->enable();
this->write_byte(REG_FIFO | 0x80);
this->write_byte(data);
this->disable();
```

### 9. Zużycie zasobów

#### ESP-NOW
- RAM: ~8KB (WiFi stack)
- Flash: ~40KB (WiFi libraries)
- CPU: Niskie (sprzętowa akceleracja)

#### LoRa
- RAM: ~4KB (custom protocol)
- Flash: ~20KB (SPI + custom code)
- CPU: Średnie (software protocol handling)

### 10. Przypadki użycia

#### ESP-NOW - idealny dla:
- Szybka komunikacja local network
- Real-time control
- Dense sensor networks
- Indoor applications

#### LoRa - idealny dla:
- Long-range communication
- Battery-powered remote sensors
- Agricultural/environmental monitoring
- Emergency/backup communication

### 11. Migracja z ESP-NOW do LoRa

#### Kroki migracji:

1. **Hardware**: Dodaj moduł SX1278
2. **Konfiguracja**: Zamień `basic_espnowex` na `basic_loraex`
3. **Parametry**: Dodaj parametry LoRa (frequency, SF, etc.)
4. **Identyfikatory**: Zamień `peer_mac` na `peer_id`
5. **API**: Zamień prefiksy funkcji `espnow` na `lora`

#### Przykład migracji:

**Przed (ESP-NOW):**
```yaml
basic_espnowex:
  peer_mac: "AA:BB:CC:DD:EE:FF"
  max_retries: 3
  timeout_us: 500000
  on_message:
    - logger.log: "ESP-NOW message: ${message}"
```

**Po (LoRa):**
```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

basic_loraex:
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  peer_id: [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]
  max_retries: 3
  timeout_ms: 5000
  frequency: 433000000
  spreading_factor: 9
  bandwidth: 125000
  on_message:
    - logger.log: "LoRa message: ${message}"
```

### 12. Kompatybilność wsteczna

Komponent LoRa zachowuje 100% kompatybilność API z ESP-NOW na poziomie:
- Triggerów automatyzacji
- Funkcji wysyłania w lambda
- Callback handlery
- Zarządzania kolejkami

Jedyne zmiany wymagane:
- Dodanie konfiguracji SPI
- Podłączenie modułu SX1278  
- Aktualizacja nazw funkcji (`espnow` → `lora`)
- Dodanie parametrów specyficznych dla LoRa

## Podsumowanie

Komponent `basic_loraex` oferuje identyczną funkcjonalność jak `basic_espnowex`, ale z użyciem technologii LoRa zamiast ESP-NOW. To pozwala na znacznie większe zasięgi kosztem prędkości transmisji i konieczności dodatkowego hardware'u.