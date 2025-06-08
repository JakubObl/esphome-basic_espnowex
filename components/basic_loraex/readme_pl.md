# BasicLoRaEx – Komponent ESPHome dla LoRa SX1278

BasicLoRaEx to niestandardowy komponent ESPHome umożliwiający niezawodną, dalekozasięgową komunikację bezprzewodową między urządzeniami ESP32 przy użyciu modułu LoRa SX1278 [1]. Zachowuje pełną kompatybilność API z [basic_espnowex](https://github.com/JakubObl/esphome-basic_espnowex), ale wykorzystuje technologię LoRa zamiast ESP-NOW do transmisji, zapewniając znacznie zwiększony zasięg i możliwości penetracji przeszkód [2][3].

**Źródło:** `github://JakubObl/esphome-basic_espnowex`

---

## Funkcje

- Komunikacja dalekiego zasięgu (do 10 km w otwartej przestrzeni) [4]
- Niezawodny protokół wiadomości z automatycznymi potwierdzeniami i retransmisjami [1]
- Deduplikacja wiadomości zapobiegająca przetwarzaniu duplikatów [2]
- Tryby komunikacji broadcast i peer-to-peer [3]
- Cztery wyzwalacze automatyzacji: `on_message`, `on_recv_cmd`, `on_recv_ack`, `on_recv_data` [1]
- Konfigurowalne parametry LoRa (częstotliwość, współczynnik rozpraszania, szerokość pasma, stopień kodowania itd.) [4][5]
- Operacje bezpieczne wątkowo z implementacją mutex FreeRTOS [2]
- Kompatybilność z automatyzacjami ESPHome i standardową konfiguracją YAML [6]

---

## Wymagania sprzętowe

- Mikrokontroler ESP32 (ESP8266 nie jest obsługiwany) [7]
- Moduł LoRa SX1278 [8]
- Przewody połączeniowe

---

## Połączenia sprzętowe

| Pin SX1278 | Pin ESP32   | Opis              |
|:----------:|:-----------:|:------------------|
| VCC        | 3.3V        | Zasilanie         |
| GND        | GND         | Masa              |
| SCK        | GPIO18      | SPI Clock         |
| MISO       | GPIO19      | SPI MISO          |
| MOSI       | GPIO23      | SPI MOSI          |
| NSS        | GPIO5       | SPI Chip Select   |
| DIO0       | GPIO2       | Przerwanie (RX/TX)|
| RST        | GPIO14      | Reset             |

Uwaga: Możesz dowolnie przypisać inne piny GPIO w konfiguracji YAML [1][9].

---

## Instalacja

1. Utwórz katalog `custom_components` w folderze konfiguracyjnym ESPHome, jeśli jeszcze nie istnieje [10]
2. Wewnątrz tego katalogu utwórz katalog `basic_loraex` [10]
3. Skopiuj pliki komponentu do tego katalogu [10]
4. Dodaj odpowiednią konfigurację do pliku YAML ESPHome [7]

---

## Podstawowy przykład YAML

```yaml
esphome:
  name: lora_node

esp32:
  board: esp32dev

# Konfiguracja magistrali SPI dla SX1278
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Podstawowa konfiguracja LoRa
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 433000000   # 433/868/915 MHz w zależności od regionu
  spreading_factor: 9
  bandwidth: 125000
  coding_rate: 7
  tx_power: 14

logger:
```

Ta podstawowa konfiguracja tworzy węzeł LoRa o umiarkowanym zasięgu i szybkości transmisji danych [4][11].

---

## Zaawansowany przykład YAML (z automatyzacjami)

```yaml
esphome:
  name: lora_gateway
  friendly_name: LoRa Gateway

esp32:
  board: esp32dev

# Konfiguracja magistrali SPI
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Zaawansowana konfiguracja LoRa
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 868000000    # Częstotliwość EU
  spreading_factor: 12    # Maksymalny zasięg
  bandwidth: 125000       # Standardowa szerokość pasma
  coding_rate: 8          # Maksymalna korekcja błędów
  tx_power: 20            # Maksymalna moc (sprawdź lokalne przepisy)
  sync_word: 0x12         # Identyfikator sieci
  preamble_length: 8      # Standardowa preambuła
  enable_crc: true        # Włącz sprawdzanie błędów
  implicit_header: false  # Użyj jawnych nagłówków
  max_retries: 8          # Próby retransmisji
  timeout_us: 500000      # Timeout 500ms

  # Wyzwalacze automatyzacji
  on_message:
    - then:
        - logger.log:
            format: "Odebrano od %02X:%02X:%02X:%02X:%02X:%02X: %s"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]', 'message.c_str()' ]

  on_recv_cmd:
    - then:
        - logger.log:
            format: "Odebrano komendę %d od %02X:%02X:%02X:%02X:%02X:%02X"
            args: [ 'cmd', 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]' ]

  on_recv_ack:
    - then:
        - logger.log:
            format: "Odebrano ACK od %02X:%02X:%02X:%02X:%02X:%02X dla ID %02X%02X%02X"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]', 'msg_id[0]', 'msg_id[1]', 'msg_id[2]' ]

  on_recv_data:
    - then:
        - logger.log:
            format: "Odebrano dane od %02X:%02X:%02X:%02X:%02X:%02X"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]' ]

# Dodaj czujnik, który będzie transmitowany przez LoRa
sensor:
  - platform: dht
    model: DHT22
    pin: GPIO4
    temperature:
      name: "Temperatura"
      on_value:
        - lambda: |-
            // Wyślij odczyt temperatury przez LoRa
            id(lora_radio).send_broadcast_str(to_string(id(temperature).state));
    humidity:
      name: "Wilgotność"
    update_interval: 60s
```

Ta zaawansowana konfiguracja maksymalizuje zasięg i niezawodność z odpowiednimi automatyzacjami [5][6][12].

---

## Przykład stacji pogodowej

```yaml
esphome:
  name: lora_weather
  friendly_name: Stacja Pogodowa LoRa

esp32:
  board: esp32dev
  
# Głęboki sen dla oszczędzania baterii
deep_sleep:
  run_duration: 30s
  sleep_duration: 15min
  wakeup_pin: GPIO33
  
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Konfiguracja LoRa zoptymalizowana pod kątem baterii
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 433000000
  spreading_factor: 10    # Dobry zasięg przy rozsądnym czasie transmisji
  bandwidth: 125000
  coding_rate: 5          # Niższa korekcja błędów dla szybszych transmisji
  tx_power: 10            # Zredukowana moc dla oszczędzania baterii
  max_retries: 2          # Ograniczone powtórzenia dla oszczędzania energii

# Czujnik środowiskowy BME280
i2c:
  sda: GPIO21
  scl: GPIO22
  
sensor:
  - platform: bme280
    temperature:
      name: "Temperatura zewnętrzna"
      id: outdoor_temp
    pressure:
      name: "Ciśnienie atmosferyczne"
      id: pressure
    humidity:
      name: "Wilgotność zewnętrzna"
      id: outdoor_humidity
    address: 0x76
    update_interval: 30s
    
  - platform: adc
    pin: GPIO34
    name: "Napięcie baterii"
    id: battery_voltage
    update_interval: 30s
    attenuation: 11db
    filters:
      - multiply: 2 # Korekta dzielnika napięcia
    
# Wyślij wszystkie dane czujników jako połączony pakiet
interval:
  - interval: 25s
    then:
      - lambda: |-
          char data[64];
          sprintf(data, "T:%.1f,H:%.1f,P:%.1f,B:%.1f", 
                  id(outdoor_temp).state, 
                  id(outdoor_humidity).state, 
                  id(pressure).state, 
                  id(battery_voltage).state);
          id(lora_radio).send_broadcast_str(data);
      - deep_sleep.enter
```

Ten przykład tworzy stację pogodową zasilaną bateryjnie, która budzi się okresowo, zbiera dane środowiskowe, przesyła je przez LoRa i powraca do głębokiego snu [1][4][13].

---

## Przykład przycisku z ukierunkowanymi wiadomościami

```yaml
esphome:
  name: lora_button
  friendly_name: Przycisk Zdalny LoRa

esp32:
  board: esp32dev

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 868000000
  spreading_factor: 7     # Szybka transmisja dla przycisku
  bandwidth: 250000       # Szersze pasmo dla szybszej transmisji danych
  coding_rate: 5
  tx_power: 14

# Fizyczne przyciski
binary_sensor:
  - platform: gpio
    pin: 
      number: GPIO12
      mode: INPUT_PULLUP
    name: "Przycisk 1"
    on_press:
      - lambda: |-
          // Wyślij do konkretnego odbiornika o adresie 01:02:03:04:05:06
          std::array receiver = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_str("PRZYCISK_1_WCISNIETY", receiver);
          
  - platform: gpio
    pin: 
      number: GPIO14
      mode: INPUT_PULLUP
    name: "Przycisk 2"
    on_press:
      - lambda: |-
          // Wyślij komendę numeryczną zamiast ciągu znaków
          std::array receiver = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(42, receiver);
```

Ten przykład pokazuje, jak wysyłać zarówno wiadomości tekstowe, jak i komendy numeryczne do określonego urządzenia LoRa, zamiast nadawać w trybie broadcast [2][3][14].

---

## Wyjaśnienie parametrów LoRa

| Parametr          | Opis                                     | Przykładowe wartości | Uwagi |
|-------------------|------------------------------------------|----------------------|-------|
| frequency         | Częstotliwość nośna (Hz)                 | 433000000, 868000000, 915000000 | Zależne od regionu [15] |
| spreading_factor  | Współczynnik kodowania danych (SF7-SF12) | 7-12                | Wyższy = większy zasięg, wolniejszy [16][17] |
| bandwidth         | Szerokość pasma kanału (Hz)              | 125000, 250000, 500000 | Szersze = szybciej, mniejszy zasięg [16] |
| coding_rate       | Poziom korekcji błędów (5-8)             | 5-8                 | Wyższy = bardziej odporny [5] |
| tx_power          | Moc transmisji (dBm)                     | 2-20                | Sprawdź regionalne przepisy [15] |
| sync_word         | Identyfikator sieci                      | 0x12                | Separuje sieci LoRa [3] |
| preamble_length   | Liczba symboli preambuły LoRa            | 6-65535             | Zazwyczaj 8 jest standardem [5] |
| enable_crc        | Cykliczna kontrola nadmiarowa            | true/false          | Wykrywanie błędów [4] |
| implicit_header   | Wybór trybu nagłówka                     | true/false          | Zazwyczaj false [5] |
| max_retries       | Próby retransmisji                       | 0-255               | Wyższy zużywa więcej energii [2] |
| timeout_us        | Timeout na wiadomość (mikrosekundy)      | 100000-1000000      | Zazwyczaj 200000-500000 [2] |

---

## Przepisy dotyczące cyklu pracy w UE

Podczas pracy w Unii Europejskiej należy pamiętać o ograniczeniach cyklu pracy dla transmisji LoRa [15][26]:

- Pasmo 433 MHz: 10% cyklu pracy (można transmitować 36 sekund na godzinę)
- 868,0-868,6 MHz: 1% cyklu pracy (można transmitować 36 sekund na godzinę)
- 868,7-869,2 MHz: 0,1% cyklu pracy (można transmitować 3,6 sekundy na godzinę)
- 869,4-869,65 MHz: 10% cyklu pracy (można transmitować 360 sekund na godzinę)

Dostosuj odpowiednio interwały transmisji, aby zachować zgodność z tymi przepisami [26].

---

## Wysyłanie danych w ESPHome Lambda

### Broadcast do wszystkich urządzeń:

```yaml
button:
  - platform: template
    name: "Wyślij Broadcast LoRa"
    on_press:
      - lambda: |-
          id(lora_radio).send_broadcast_str("Witaj, świecie!");
```

### Wysyłanie do określonego peer:

```yaml
button:
  - platform: template
    name: "Wyślij do określonego urządzenia"
    on_press:
      - lambda: |-
          std::array peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_str("Prywatna wiadomość", peer);
```

### Wysyłanie komendy numerycznej:

```yaml
button:
  - platform: template
    name: "Wyślij komendę"
    on_press:
      - lambda: |-
          std::array peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(123, peer);  // Wyślij wartość komendy 123
```

Te przykłady pokazują, jak programowo wywołać transmisje LoRa [1][2][3].

[1] https://github.com/JakubObl/esphome-basic_espnowex/blob/main/components/basic_loraex/__init__.py
[2] https://github.com/JakubObl/esphome-basic_espnowex/blob/main/components/basic_loraex/basic_espnowex.cpp
[3] https://github.com/JakubObl/esphome-basic_espnowex/blob/main/components/basic_loraex/basic_espnowex.h
[4] https://developers.esphome.io/blog/2025/02/19/about-the-removal-of-support-for-custom-components/
[5] https://esphome.io/components/index.html
[6] https://community.home-assistant.io/t/esphome-custom-component-for-beginner/544665
[7] https://esphome.io/index.html
[8] https://github.com/thegroove/esphome-custom-component-examples
[9] https://esphome.io/guides/getting_started_hassio.html
[10] https://shop.kincony.com/products/esp32-lora-sx1278-gateway-kincony-alr
[11] https://www.youtube.com/watch?v=7PoUWszwaFk
[12] https://esphome.io/components/esphome.html
[13] https://esphome.io/guides/getting_started_command_line.html
[14] https://docs.m5stack.com/en/unit/Unit%20LoRaWAN-US915
[15] https://forum.arduino.cc/t/why-is-everybody-using-868-915-mhz-for-lora-and-not-433-mhz/702157
[16] https://www.nicerf.com/company/lora-module-sx1278.html
[17] https://www.radiocontrolli.com/sx1278-module-433mhz?l=en
[18] https://www.thethingsnetwork.org/docs/lorawan/spreading-factors/
[19] https://esphome.io/components/external_components.html
[20] https://determined.ai/blog/lora-parameters
[21] https://www.radiocontrolli.com/en/lora-module-spi-controllable
[22] https://blog.ttulka.com/lora-spreading-factor-explained/
[23] https://developers.esphome.io/architecture/overview/
[24] https://developers.esphome.io/architecture/components/
[25] https://wiki.seeedstudio.com/Grove_Wio_E5_P2P/
[26] https://tektelic.com/what-it-is/maximum-duty-cycle/
[27] https://esphome.io/guides/yaml
[28] https://homething.io/ESPHome-Component-From-Scratch
[29] https://www.youtube.com/watch?v=niBWh0Xq9Zg
[30] https://pl.aliexpress.com/item/1005006176213952.html
[31] https://community.home-assistant.io/t/guidelines-for-choosing-hardware-components-to-work-with-esphome/811874
