# BasicESPNowEx - Rozszerzony komponent ESP-NOW dla ESPHome

Ten dokument opisuje zaawansowany komponent ESP-NOW dla ESPHome, który rozszerza standardową funkcjonalność ESP-NOW o niezawodny system komunikacji z potwierdzeniami, ponowieniem wysyłania oraz deduplikacją wiadomości. Komponent BasicESPNowEx został zaprojektowany do zapewnienia stabilnej komunikacji bezprzewodowej między urządzeniami ESP32/ESP8266 w środowisku automatyki domowej[1][2][3].

## Główne funkcjonalności

### System niezawodnej komunikacji

BasicESPNowEx implementuje zaawansowany protokół komunikacji, który znacznie przewyższa standardowe możliwości ESP-NOW. Komponent automatycznie dodaje nagłówki do wiadomości zawierające unikalne identyfikatory, które umożliwiają śledzenie statusu dostarczenia oraz eliminację duplikatów[2]. System wykorzystuje mechanizm ACK (potwierdzenia) do weryfikacji, czy wiadomość dotarła do odbiorcy, co jest kluczowe w zastosowaniach wymagających wysokiej niezawodności.

Każda wysłana wiadomość otrzymuje trzybajtowy identyfikator generowany na podstawie znacznika czasowego i liczby losowej, co gwarantuje unikalność przez długi okres czasu[2]. Wiadomości są automatycznie retransmitowane w przypadku braku potwierdzenia w określonym czasie, przy czym liczba prób i timeout są konfigurowalne przez użytkownika.

### Deduplikacja i historia wiadomości

Komponent prowadzi historię odebranych wiadomości przez okres 300 sekund, co umożliwia identyfikację i odrzucenie duplikatów[2]. System porównuje kombinację adresu MAC nadawcy oraz całej zawartości wiadomości, zapewniając skuteczną ochronę przed wielokrotnym przetwarzaniem tej samej informacji. Historia jest automatycznie czyszczona z przestarzałych wpisów, co optymalizuje wykorzystanie pamięci.

## Konfiguracja komponentu

### Podstawowa konfiguracja

```yaml
external_components:
  - source: github://JakubObl/esphome-basic_espnowex

esphome:
  name: esp-device
  platform: esp32
  board: esp32dev

wifi:
  ssid: "TwojaSiec"
  password: "TwojeHaslo"

api:
  encryption:
    key: "twoj-klucz-api"

ota:
  password: "haslo-ota"

basicespnowex:
  id: espnow_component
  peer_mac: "AA:BB:CC:DD:EE:FF"
  max_retries: 5
  timeout_us: 200000
```

### Parametry konfiguracyjne

Komponent BasicESPNowEx oferuje szeroki zakres opcji konfiguracyjnych dostosowanych do różnych scenariuszy użycia[1]. Parametr `peer_mac` definiuje domyślny adres MAC urządzenia docelowego dla komunikacji punkt-punkt, podczas gdy `max_retries` określa maksymalną liczbę prób retransmisji wiadomości w przypadku braku potwierdzenia. Wartość `timeout_us` ustawia czas oczekiwania na potwierdzenie w mikrosekundach, przy czym domyślna wartość 200000 odpowiada 200 milisekundom.

Wszystkie parametry są opcjonalne i mają sensowne wartości domyślne. Jeśli `peer_mac` nie zostanie określone, komponent będzie używał adresu broadcast (FF:FF:FF:FF:FF:FF) jako domyślnego celu[2]. Parametry timeout i retry można dostosować w zależności od warunków sieciowych i wymagań aplikacji.

## Zdarzenia i triggery

### Trigger on_message

```yaml
basicespnowex:
  id: espnow_component
  on_message:
    - then:
        - logger.log:
            format: "Odebrano wiadomość od %s: %s"
            args: ['id(mac_to_string(mac))', 'message.c_str()']
```

Trigger `on_message` jest aktywowany przy odbiorze każdej prawidłowej wiadomości tekstowej[1][2]. Dostarcza dwa parametry: `mac` zawierający sześciobajtowy adres MAC nadawcy oraz `message` będący stringiem z treścią wiadomości. Ten trigger jest szczególnie przydatny do implementacji prostej komunikacji tekstowej między urządzeniami.

### Trigger on_recv_data

```yaml
basicespnowex:
  id: espnow_component
  on_recv_data:
    - then:
        - lambda: |-
            ESP_LOGI("data", "Odebrano %d bajtów od urządzenia", data.size());
            for(auto byte : data) {
              ESP_LOGI("data", "Bajt: 0x%02X", byte);
            }
```

Trigger `on_recv_data` obsługuje wszystkie typy danych binarnych[2][3]. Parametr `data` to wektor bajtów zawierający surowe dane po usunięciu nagłówka protokołu. Ten trigger jest wywoływany dla każdej odebranej wiadomości, niezależnie od jej typu, co czyni go uniwersalnym mechanizmem przetwarzania danych.

### Trigger on_recv_cmd

```yaml
basicespnowex:
  id: espnow_component
  on_recv_cmd:
    - then:
        - if:
            condition:
              lambda: 'return cmd == 1001;'
            then:
              - switch.toggle: relay1
        - if:
            condition:
              lambda: 'return cmd == 1002;'
            then:
              - light.toggle: led_strip
```

Trigger `on_recv_cmd` jest specjalizowanym mechanizmem obsługi komend numerycznych[2]. Aktywowany jest tylko wtedy, gdy odebrana wiadomość ma dokładnie 4 bajty i pierwsza para bajtów jest identyczna z drugą parą, co wskazuje na komendę. Parametr `cmd` zawiera 16-bitową wartość komendy w formacie big-endian.

### Trigger on_recv_ack

```yaml
basicespnowex:
  id: espnow_component
  on_recv_ack:
    - then:
        - logger.log:
            format: "Potwierdzenie od %s dla wiadomości %02X%02X%02X"
            args: ['id(mac_to_string(mac))', 'msg_id[0]', 'msg_id[1]', 'msg_id[2]']
```

Trigger `on_recv_ack` informuje o odebraniu potwierdzeń wysłanych wiadomości[2][3]. Dostarcza adres MAC urządzenia potwierdzającego oraz trzybajtowy identyfikator wiadomości. Jest przydatny do implementacji mechanizmów monitorowania statusu komunikacji oraz diagnostyki sieci.

## Metody wysyłania wiadomości

### Komunikacja broadcast

```yaml
button:
  - platform: template
    name: "Wyślij broadcast"
    on_press:
      - basicespnowex.send_broadcast_str:
          id: espnow_component
          message: "Wiadomość do wszystkich urządzeń"
      
  - platform: template
    name: "Wyślij dane broadcast"
    on_press:
      - basicespnowex.send_broadcast:
          id: espnow_component
          data: [0x01, 0x02, 0x03, 0x04]
```

Metody `send_broadcast` i `send_broadcast_str` umożliwiają wysyłanie wiadomości do wszystkich urządzeń w zasięgu[2]. W przeciwieństwie do komunikacji punkt-punkt, wiadomości broadcast nie wymagają potwierdzeń i nie są ponawiane, co czyni je idealnym rozwiązaniem do wysyłania informacji o statusie lub ogłoszeń systemowych.

### Komunikacja punkt-punkt

```yaml
button:
  - platform: template
    name: "Wyślij do peera"
    on_press:
      - basicespnowex.send_to_peer_str:
          id: espnow_component
          message: "Wiadomość do konkretnego urządzenia"

  - platform: template
    name: "Wyślij do konkretnego MAC"
    on_press:
      - basicespnowex.send_espnow_str:
          id: espnow_component
          message: "Test komunikacji"
          peer_mac: "11:22:33:44:55:66"
```

Metody punkt-punkt automatycznie implementują mechanizm potwierdzeń i ponownych prób[2]. Każda wiadomość jest śledzona do momentu otrzymania potwierdzenia lub wyczerpania limitu prób. System automatycznie zarządza kolejką oczekujących wiadomości i wykonuje retransmisje zgodnie z konfiguracją timeout i retry.

## System komend (CMD)

### Mechanizm działania komend

System komend BasicESPNowEx wykorzystuje specjalny format wiadomości składający się z czterech bajtów, gdzie pierwsze dwa bajty zawierają wartość komendy, a kolejne dwa bajty stanowią duplikat dla weryfikacji integralności[2]. Komenda jest rozpoznawana po stronie odbiorcy tylko wtedy, gdy oba segmenty są identyczne, co zapewnia dodatkowy poziom ochrony przed błędami transmisji.

```yaml
button:
  - platform: template
    name: "Włącz światło (CMD 1001)"
    on_press:
      - basicespnowex.send_espnow_cmd:
          id: espnow_component
          cmd: 1001
          peer_mac: "AA:BB:CC:DD:EE:FF"

  - platform: template
    name: "Wyłącz światło (CMD 1002)"
    on_press:
      - basicespnowex.send_espnow_cmd:
          id: espnow_component
          cmd: 1002
          peer_mac: "AA:BB:CC:DD:EE:FF"
```

### Zaawansowane zastosowania komend

Komendy mogą być wykorzystywane do implementacji złożonych protokołów komunikacyjnych między urządzeniami. System automatycznie zapobiega duplikowaniu komend przez sprawdzanie czy identyczna komenda do tego samego urządzenia nie jest już w kolejce oczekujących[2]. Jeśli taka komenda zostanie znaleziona, jej liczniki retry są resetowane, ale ponowna transmisja nie następuje natychmiast, co optymalizuje wykorzystanie medium bezprzewodowego.

## System ponawiania i niezawodność

### Algorytm retransmisji

BasicESPNowEx implementuje zaawansowany algorytm retransmisji oparty na timerze okresowym działającym co 400 milisekund[2]. System sprawdza wszystkie wiadomości w kolejce i identyfikuje te, które przekroczyły określony timeout bez otrzymania potwierdzenia. Dla takich wiadomości, o ile nie osiągnięto maksymalnej liczby prób, wykonywana jest retransmisja z aktualizacją znacznika czasowego.

```yaml
basicespnowex:
  id: espnow_component
  max_retries: 3      # Maksymalnie 3 próby
  timeout_us: 500000  # 500ms timeout między próbami
```

Algorytm automatycznie usuwa z kolejki wiadomości, które otrzymały potwierdzenie lub przekroczyły maksymalną liczbę prób. Każda retransmisja jest logowana na poziomie DEBUG z informacjami o adresie docelowym, identyfikatorze wiadomości oraz numerze próby, co ułatwia diagnostykę problemów komunikacyjnych.

### Zarządzanie peerami i obsługa błędów

Komponent automatycznie zarządza listą znanych urządzeń (peerów) w systemie ESP-NOW[2]. Jeśli podczas wysyłania wiadomości okaże się, że docelowe urządzenie nie jest zarejestrowane, system automatycznie próbuje je dodać. W przypadku niepowodzenia dodania peera, wiadomość może być ponowiona maksymalnie 3 razy, po czym jest usuwana z kolejki aby zapobiec nieskończonemu zapętleniu.

System obsługuje również zmiany kanału WiFi poprzez nasłuchiwanie zdarzeń WiFi i ponowną inicjalizację ESP-NOW z aktualizacją listy peerów[2]. Zapewnia to stabilność komunikacji nawet w środowiskach o dynamicznie zmieniających się warunkach sieciowych.

## Bezpieczeństwo wątkowe i wydajność

### Mechanizmy synchronizacji

Komponent wykorzystuje dwa semafory mutex do zapewnienia bezpieczeństwa wątkowego[2][3]. Pierwszy (`queue_mutex_`) chroni dostęp do kolejki oczekujących wiadomości, a drugi (`history_mutex_`) zabezpiecza historię odebranych wiadomości. Wszystkie operacje na tych strukturach danych są wykonywane w sekcjach krytycznych, co eliminuje ryzyko wystąpienia warunków wyścigu (race conditions).

Implementacja używa nieblokujących operacji mutex tam, gdzie to możliwe, aby uniknąć zawieszenia pętli głównej ESPHome. W przypadku niemożności uzyskania dostępu do zasobów współdzielonych, operacje są odkładane do następnego cyklu, zachowując responsywność systemu.

### Optymalizacja pamięci

Historia odebranych wiadomości jest ograniczona do maksymalnie 1000 wpisów i automatycznie czyszczona z przestarzałych elementów starszych niż 300 sekund[2]. Kolejka oczekujących wiadomości nie ma sztywnego limitu, ale jest dynamicznie zarządzana poprzez usuwanie potwierdzonych i przeterminowanych wiadomości. Te mechanizmy zapewniają stabilne wykorzystanie pamięci nawet przy intensywnej komunikacji.

## Przykłady zaawansowanych zastosowań

### System monitorowania środowiska

```yaml
sensor:
  - platform: dht
    pin: GPIO22
    temperature:
      name: "Temperatura"
      id: temp_sensor
      on_value:
        then:
          - basicespnowex.send_espnow_str:
              id: espnow_component
              message: !lambda |-
                return "TEMP:" + to_string(x);
              peer_mac: "AA:BB:CC:DD:EE:FF"

basicespnowex:
  id: espnow_component
  on_message:
    - then:
        - if:
            condition:
              lambda: 'return message.substr(0, 5) == "TEMP:";'
            then:
              - logger.log:
                  format: "Odebrano temperaturę: %s°C"
                  args: ['message.substr(5).c_str()']
```

### Zdalne sterowanie urządzeniami

```yaml
basicespnowex:
  id: espnow_component
  on_recv_cmd:
    - then:
        - lambda: |-
            switch(cmd) {
              case 100: // Włącz wszystkie światła
                id(light1).turn_on();
                id(light2).turn_on();
                break;
              case 101: // Wyłącz wszystkie światła
                id(light1).turn_off();
                id(light2).turn_off();
                break;
              case 200: // Alarm ON
                id(buzzer).turn_on();
                id(red_led).turn_on();
                break;
              case 201: // Alarm OFF
                id(buzzer).turn_off();
                id(red_led).turn_off();
                break;
            }

button:
  - platform: template
    name: "Emergency Stop"
    on_press:
      - basicespnowex.send_espnow_cmd:
          id: espnow_component
          cmd: 999
          peer_mac: "FF:FF:FF:FF:FF:FF"  # Broadcast emergency
```

## Debugowanie i diagnostyka

### Logi systemowe

Komponent generuje szczegółowe logi na różnych poziomach, które pomagają w diagnostyce problemów komunikacyjnych[2]. Logi DEBUG zawierają informacje o retransmisjach, dodawaniu peerów oraz statusie wysyłania. Logi ERROR informują o krytycznych problemach takich jak niemożność inicjalizacji WiFi lub ESP-NOW.

```yaml
logger:
  level: DEBUG
  logs:
    basic_espnowex: DEBUG
```

### Monitorowanie stanu kolejki

```yaml
sensor:
  - platform: template
    name: "Pending Messages Count"
    id: pending_count
    update_interval: 5s
    lambda: |-
      // Ta funkcjonalność wymaga rozszerzenia komponentu
      // o metodę zwracającą rozmiar kolejki
      return {}; // placeholder

button:
  - platform: template
    name: "Clear Pending Messages"
    on_press:
      - basicespnowex.clear_pending_messages:
          id: espnow_component
```

## Rozwiązywanie problemów

### Częste problemy i rozwiązania

Najczęstszym problemem jest brak komunikacji spowodowany niewłaściwą konfiguracją kanału WiFi lub nieprawidłowymi adresami MAC[2]. Należy upewnić się, że wszystkie urządzenia w sieci ESP-NOW działają na tym samym kanale radiowym. Komponent automatycznie próbuje zsynchronizować kanały, ale w niektórych przypadkach może być konieczne ręczne ustawienie kanału w konfiguracji WiFi.

Jeśli wiadomości nie są dostarczane pomimo prawidłowej konfiguracji, warto sprawdzić logi dotyczące dodawania peerów oraz zwiększyć wartości timeout i max_retries. W środowiskach o wysokim poziomie zakłóceń elektromagnetycznych może być konieczne dostosowanie tych parametrów do lokalnych warunków.

## Wnioski

BasicESPNowEx znacząco rozszerza możliwości standardowego ESP-NOW o profesjonalne funkcje niezawodnej komunikacji. System potwierdzeń, automatycznego ponawiania oraz deduplikacji czyni go idealnym rozwiązaniem do zastosowań wymagających wysokiej niezawodności transmisji danych. Bogaty zestaw triggerów i metod komunikacyjnych umożliwia implementację złożonych protokołów komunikacyjnych dostosowanych do specyficznych potrzeb automatyki domowej i systemów IoT.

Citations:
[1] init.py
[2] basic_espnowex.cpp
[3] basic_espnowex.h
[4] https://esphome.io/components/external_components.html
[5] https://community.home-assistant.io/t/implement-an-esphome-generic-custom-component-according-documentation-in-home-assistant/540895
[6] https://community.home-assistant.io/t/custom-component-readme/131133
[7] https://www.home-assistant.io/integrations/esphome/
[8] https://community.home-assistant.io/t/esphome-custom-component-for-beginner/544665
[9] https://esphome.io/components/api.html
[10] https://esphome.io/components/esphome.html
[11] https://developers.esphome.io/blog/2025/02/19/about-the-removal-of-support-for-custom-components/
[12] https://github.com/slimcdk/esphome-custom-components/blob/master/esphome/components/tmc2208/README.md
[13] https://github.com/thegroove/esphome-custom-component-examples/blob/master/README.md
