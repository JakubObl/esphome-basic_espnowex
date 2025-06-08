# Lista utworzonych plików dla komponentu basic_loraex

## Pliki komponentu (do skopiowania do custom_components/basic_loraex/)

### 1. `__init__.py`
**Źródło:** `basic_loraex_init.md`
**Opis:** Plik konfiguracji Python dla ESPHome z definicjami schematów YAML, walidacją parametrów i code generation.

**Kluczowe elementy:**
- Definicje triggerów (OnMessageTrigger, OnRecvAckTrigger, etc.)
- Walidacja parametrów LoRa (spreading factor, bandwidth, coding rate)
- Konfiguracja pinów SPI (DIO0, RST, CS)
- Parametry specyficzne dla SX1278

### 2. `basic_loraex.h`
**Źródło:** `basic_loraex_header.md`
**Opis:** Plik nagłówkowy C++ z definicjami klas, stałych i interfejsów.

**Kluczowe elementy:**
- Definicje rejestrów SX1278
- Struktury PendingMessage i ReceivedMessageInfo
- Klasa BasicLoRaEx dziedzicząca z Component i SPIDevice
- Klasy triggerów (OnMessageTrigger, OnRecvAckTrigger, etc.)
- API publiczne dla wysyłania wiadomości

### 3. `basic_loraex.cpp` 
**Źródło:** `basic_loraex_cpp_part1.md` + `basic_loraex_cpp_part2.md`
**Opis:** Główna implementacja C++ komponentu LoRa.

**Kluczowe elementy:**
- Inicjalizacja modułu SX1278 przez SPI
- Konfiguracja parametrów LoRa (frequency, SF, BW, CR, etc.)
- Protokół komunikacji z ACK i retransmisjami
- Obsługa przerwań DIO0
- Thread-safe kolejki wiadomości
- Funkcje wysyłania/odbierania pakietów

## Pliki dokumentacji

### 4. `example_lora_config.yaml`
**Źródło:** `example_lora_config.md`
**Opis:** Przykład kompletnej konfiguracji YAML pokazującej wszystkie funkcje komponentu.

**Zawiera:**
- Konfigurację hardware (piny SPI)
- Wszystkie parametry LoRa z wyjaśnieniami
- Przykłady triggerów i automatyzacji
- Integrację z sensorami i przełącznikami
- Komendy lambda dla różnych funkcji

### 5. `basic_loraex_docs.md`
**Źródło:** `basic_loraex_docs.md`
**Opis:** Kompletna dokumentacja komponentu.

**Sekcje:**
- Opis funkcjonalności
- Instrukcje instalacji i konfiguracji hardware
- Tabela połączeń SX1278-ESP32
- Częstotliwości według regionów
- Pełny opis API
- Triggery i automatyzacje
- Protokół komunikacji
- Optymalizacja wydajności
- Rozwiązywanie problemów
- Przykłady zastosowań

### 6. `comparison_espnow_vs_lora.md`
**Źródło:** `comparison_espnow_vs_lora.md`
**Opis:** Szczegółowe porównanie oryginalnego komponentu ESP-NOW z nowym komponentem LoRa.

**Zawiera:**
- Porównanie technologii (zasięg, prędkość, zużycie energii)
- Różnice w konfiguracji hardware i software
- Przewodnik migracji z ESP-NOW do LoRa
- Przypadki użycia dla każdej technologii
- Kompatybilność API

## Instrukcje instalacji

1. **Utwórz folder komponentu:**
   ```
   custom_components/basic_loraex/
   ```

2. **Skopiuj pliki główne:**
   - Skopiuj kod z `basic_loraex_init.md` do `__init__.py`
   - Skopiuj kod z `basic_loraex_header.md` do `basic_loraex.h`
   - Połącz kody z `basic_loraex_cpp_part1.md` i `basic_loraex_cpp_part2.md` do `basic_loraex.cpp`

3. **Konfiguracja hardware:**
   - Podłącz moduł SX1278 zgodnie z tabelą w dokumentacji
   - Dostosuj piny w konfiguracji YAML

4. **Konfiguracja YAML:**
   - Użyj przykładu z `example_lora_config.yaml` jako bazy
   - Dostosuj parametry LoRa do swojego regionu i zastosowania

## Główne cechy komponentu

### ✅ Zachowane z ESP-NOW:
- Identyczne API dla wysyłania wiadomości
- Te same triggery i automatyzacje  
- System ACK z retransmisjami
- Deduplicja wiadomości
- Thread-safe operacje
- Zarządzanie kolejkami

### 🆕 Nowe dla LoRa:
- Komunikacja długodystansowa (do 10 km)
- Niskie zużycie energii
- Konfiguracyjne parametry RF
- Obsługa różnych częstotliwości regionalnych
- Integracja SPI z ESP32
- Obsługa przerwań hardware

### 🔧 Parametry specyficzne dla SX1278:
- `frequency`: 433/868/915 MHz
- `spreading_factor`: 6-12 (kompromis prędkość/zasięg)
- `bandwidth`: 7.8 kHz - 500 kHz
- `coding_rate`: 4/5 - 4/8 (korekcja błędów)
- `tx_power`: 2-20 dBm (moc transmisji)
- `sync_word`: 0x12 (identyfikator sieci)
- `enable_crc`: true/false (kontrola błędów)

## Zastosowania

- **Monitorowanie środowiska** na dużych obszarach
- **Systemy alarmowe** z długim zasięgiem
- **IoT w rolnictwie** (czujniki na polach)
- **Backup komunikacja** gdy WiFi niedostępne
- **Sensory batteryjne** z rzadkim wysyłaniem danych