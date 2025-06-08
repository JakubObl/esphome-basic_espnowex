# BasicLoRaEx – ESPHome LoRa SX1278 Component

BasicLoRaEx is an ESPHome custom component enabling reliable, long-range wireless communication between ESP32 devices using the LoRa SX1278 module [1]. It maintains full API compatibility with [basic_espnowex](https://github.com/JakubObl/esphome-basic_espnowex), but utilizes LoRa technology instead of ESP-NOW for transmission, providing significantly increased range and penetration capabilities [2][3].

**Source:** `github://JakubObl/esphome-basic_espnowex`

---

## Features

- Long-range communication (up to 10km in open space) [4]
- Reliable message protocol with automatic acknowledgements and retransmissions [1]
- Message deduplication to prevent processing duplicates [2]
- Broadcast and peer-to-peer communication modes [3]
- Four automation triggers: `on_message`, `on_recv_cmd`, `on_recv_ack`, `on_recv_data` [1]
- Configurable LoRa parameters (frequency, spreading factor, bandwidth, coding rate, etc.) [4][5]
- Thread-safe operations with FreeRTOS mutex implementation [2]
- Compatible with ESPHome automations and standard YAML configuration [6]

---

## Hardware Requirements

- ESP32 microcontroller (ESP8266 not supported) [7]
- LoRa SX1278 module [8]
- Connection wires

---

## Hardware Connections

| SX1278 Pin | ESP32 Pin   | Description         |
|:----------:|:-----------:|:-------------------|
| VCC        | 3.3V        | Power              |
| GND        | GND         | Ground             |
| SCK        | GPIO18      | SPI Clock          |
| MISO       | GPIO19      | SPI MISO           |
| MOSI       | GPIO23      | SPI MOSI           |
| NSS        | GPIO5       | SPI Chip Select    |
| DIO0       | GPIO2       | Interrupt (RX/TX)  |
| RST        | GPIO14      | Reset              |

Note: You can freely assign different GPIO pins in your YAML configuration [1][9].

---

## Installation

1. Create a `custom_components` directory in your ESPHome configuration folder if it doesn't exist already [10]
2. Inside that directory, create a `basic_loraex` directory [10]
3. Copy the component files into this directory [10]
4. Add the appropriate configuration to your ESPHome YAML file [7]

---

## Basic YAML Example

```yaml
esphome:
  name: lora_node

esp32:
  board: esp32dev

# SPI bus configuration for SX1278
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Basic LoRa configuration
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 433000000   # 433/868/915 MHz depending on region
  spreading_factor: 9
  bandwidth: 125000
  coding_rate: 7
  tx_power: 14

logger:
```

This basic configuration sets up a LoRa node with moderate range and data rate [4][11].

---

## Advanced YAML Example (with Automations)

```yaml
esphome:
  name: lora_gateway
  friendly_name: LoRa Gateway

esp32:
  board: esp32dev

# SPI bus configuration
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Advanced LoRa configuration
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 868000000    # EU frequency
  spreading_factor: 12    # Maximum range
  bandwidth: 125000       # Standard bandwidth
  coding_rate: 8          # Maximum error correction
  tx_power: 20            # Maximum power (check local regulations)
  sync_word: 0x12         # Network identifier
  preamble_length: 8      # Standard preamble
  enable_crc: true        # Enable error checking
  implicit_header: false  # Use explicit headers
  max_retries: 8          # Retransmission attempts
  timeout_us: 500000      # 500ms timeout

  # Automation triggers
  on_message:
    - then:
        - logger.log:
            format: "Received from %02X:%02X:%02X:%02X:%02X:%02X: %s"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]', 'message.c_str()' ]

  on_recv_cmd:
    - then:
        - logger.log:
            format: "Received command %d from %02X:%02X:%02X:%02X:%02X:%02X"
            args: [ 'cmd', 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]' ]

  on_recv_ack:
    - then:
        - logger.log:
            format: "Received ACK from %02X:%02X:%02X:%02X:%02X:%02X for message ID %02X%02X%02X"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]', 'msg_id[0]', 'msg_id[1]', 'msg_id[2]' ]

  on_recv_data:
    - then:
        - logger.log:
            format: "Received raw data from %02X:%02X:%02X:%02X:%02X:%02X"
            args: [ 'mac[0]', 'mac[1]', 'mac[2]', 'mac[3]', 'mac[4]', 'mac[5]' ]

# Add a sensor that will be transmitted via LoRa
sensor:
  - platform: dht
    model: DHT22
    pin: GPIO4
    temperature:
      name: "Temperature"
      on_value:
        - lambda: |-
            // Send temperature reading via LoRa
            id(lora_radio).send_broadcast_str(to_string(id(temperature).state));
    humidity:
      name: "Humidity"
    update_interval: 60s
```

This advanced configuration maximizes range and reliability with appropriate automations [5][6][12].

---

## Weather Station Example

```yaml
esphome:
  name: lora_weather
  friendly_name: LoRa Weather Station

esp32:
  board: esp32dev
  
# Deep sleep for battery saving
deep_sleep:
  run_duration: 30s
  sleep_duration: 15min
  wakeup_pin: GPIO33
  
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# Battery-optimized LoRa configuration
basic_loraex:
  id: lora_radio
  cs_pin: GPIO5
  dio0_pin: GPIO2
  rst_pin: GPIO14
  frequency: 433000000
  spreading_factor: 10    # Good range with reasonable air time
  bandwidth: 125000
  coding_rate: 5          # Lower error correction for faster transmissions
  tx_power: 10            # Reduced power to save battery
  max_retries: 2          # Limited retries to save power

# BME280 Environmental sensor
i2c:
  sda: GPIO21
  scl: GPIO22
  
sensor:
  - platform: bme280
    temperature:
      name: "Outdoor Temperature"
      id: outdoor_temp
    pressure:
      name: "Atmospheric Pressure"
      id: pressure
    humidity:
      name: "Outdoor Humidity"
      id: outdoor_humidity
    address: 0x76
    update_interval: 30s
    
  - platform: adc
    pin: GPIO34
    name: "Battery Voltage"
    id: battery_voltage
    update_interval: 30s
    attenuation: 11db
    filters:
      - multiply: 2 # Voltage divider adjustment
    
# Send all sensor data as a combined packet
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

This example creates a battery-powered weather station that wakes up periodically, collects environmental data, transmits it via LoRa, and returns to deep sleep [1][4][13].

---

## Button Example with Targeted Messages

```yaml
esphome:
  name: lora_button
  friendly_name: LoRa Remote Button

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
  spreading_factor: 7     # Fast transmission for button
  bandwidth: 250000       # Wider bandwidth for faster data rate
  coding_rate: 5
  tx_power: 14

# Physical buttons
binary_sensor:
  - platform: gpio
    pin: 
      number: GPIO12
      mode: INPUT_PULLUP
    name: "Button 1"
    on_press:
      - lambda: |-
          // Send to a specific receiver with address 01:02:03:04:05:06
          std::array receiver = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_str("BUTTON_1_PRESSED", receiver);
          
  - platform: gpio
    pin: 
      number: GPIO14
      mode: INPUT_PULLUP
    name: "Button 2"
    on_press:
      - lambda: |-
          // Send a numeric command instead of a string
          std::array receiver = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(42, receiver);
```

This example demonstrates how to send both text messages and numeric commands to a specific LoRa device rather than broadcasting [2][3][14].

---

## LoRa Parameters Explained

| Parameter         | Description                              | Example Values    | Notes |
|-------------------|------------------------------------------|-------------------|-------|
| frequency         | Carrier frequency (Hz)                   | 433000000, 868000000, 915000000 | Region dependent [15] |
| spreading_factor  | Data encoding rate (SF7-SF12)            | 7-12             | Higher = longer range, slower [16][17] |
| bandwidth         | Channel bandwidth (Hz)                   | 125000, 250000, 500000 | Wider = faster, less range [16] |
| coding_rate       | Error correction level (5-8)             | 5-8              | Higher = more robust [5] |
| tx_power          | Transmission power (dBm)                 | 2-20             | Check regional regulations [15] |
| sync_word         | Network identifier                       | 0x12             | Separates LoRa networks [3] |
| preamble_length   | LoRa preamble symbol count               | 6-65535          | Usually 8 is standard [5] |
| enable_crc        | Cyclic redundancy check                  | true/false       | Error detection [4] |
| implicit_header   | Header mode selection                    | true/false       | Usually false [5] |
| max_retries       | Retransmission attempts                  | 0-255            | Higher uses more power [2] |
| timeout_us        | Timeout per message (microseconds)       | 100000-1000000   | Usually 200000-500000 [2] |

---

## EU Duty Cycle Regulations

When operating in the European Union, be aware of duty cycle limitations for LoRa transmissions [15][26]:

- 433 MHz band: 10% duty cycle (can transmit 36 seconds per hour)
- 868.0-868.6 MHz: 1% duty cycle (can transmit 36 seconds per hour)
- 868.7-869.2 MHz: 0.1% duty cycle (can transmit 3.6 seconds per hour)
- 869.4-869.65 MHz: 10% duty cycle (can transmit 360 seconds per hour)

Adjust your transmission intervals accordingly to comply with these regulations [26].

---

## Sending Data in ESPHome Lambda

### Broadcast to all devices:

```yaml
button:
  - platform: template
    name: "Send LoRa Broadcast"
    on_press:
      - lambda: |-
          id(lora_radio).send_broadcast_str("Hello, world!");
```

### Send to a specific peer:

```yaml
button:
  - platform: template
    name: "Send to Specific Device"
    on_press:
      - lambda: |-
          std::array peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_str("Private message", peer);
```

### Send a numeric command:

```yaml
button:
  - platform: template
    name: "Send Command"
    on_press:
      - lambda: |-
          std::array peer = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          id(lora_radio).send_lora_cmd(123, peer);  // Send command value 123
```

These examples show how to trigger LoRa transmissions programmatically [1][2][3].

---

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
