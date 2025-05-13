# Basic ESPNowEx - ESP-NOW Component for ESPHome

This comprehensive guide describes the basic_espnowex component, which enables ESP-NOW communication in ESPHome projects. ESP-NOW is a lightweight, peer-to-peer communication protocol developed by Espressif that allows ESP devices to communicate directly without requiring a WiFi router.

## Overview

ESP-NOW provides low-latency communication between ESP devices and is ideal for simple IoT projects where traditional WiFi connectivity may be overkill. The basic_espnowex component integrates ESP-NOW capabilities into the ESPHome framework, allowing you to easily create mesh-like networks of ESP devices.

## Installation

To use this component in your ESPHome projects, add it as an external component in your configuration:

```yaml
external_components:
  - source: github://JakubObl/esphome-basic_espnowex
```

## Component Configuration

The basic configuration for the component is straightforward:

```yaml
# Basic configuration
basic_espnowex:
  id: my_espnow
  peer_mac: "AA:BB:CC:DD:EE:FF"  # Optional: Default peer MAC address
```

### Configuration Variables

- **id** *(Optional)*: Identifier used to reference this component in code
- **peer_mac** *(Optional)*: Default MAC address to communicate with
- **on_message** *(Optional)*: Automation triggered when any message is received
- **on_recv_ack** *(Optional)*: Automation triggered when an acknowledgment is received
- **on_recv_cmd** *(Optional)*: Automation triggered when a command message is received

## Sending Messages

The component provides several methods to send data to other ESP devices:

### Method: send_broadcast

Sends a message to all ESP-NOW devices in range.

```yaml
button:
  - platform: template
    name: "Broadcast Message"
    on_press:
      - lambda: 'id(my_espnow).send_broadcast("Hello everyone!");'
```

### Method: send_to_peer

Sends a message to the default configured peer.

```yaml
button:
  - platform: template
    name: "Message Default Peer"
    on_press:
      - lambda: 'id(my_espnow).send_to_peer("Hello default peer!");'
```

### Method: send_espnow

A convenience method for YAML lambdas that sends a message to the default peer.

```yaml
button:
  - platform: template
    name: "Send Message"
    on_press:
      - lambda: 'id(my_espnow).send_espnow("Test message from ESPHome");'
```

### Method: send_espnow_cmd

Sends a command (a 16-bit integer) to a specific peer.

```yaml
button:
  - platform: template
    name: "Send Command"
    on_press:
      - lambda: |-
          std::array target_mac = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
          id(my_espnow).send_espnow_cmd(42, target_mac);  // Send command code 42
```

### Method: send_espnow_ex

Sends raw data (as a vector of bytes) to a specific peer.

```yaml
button:
  - platform: template
    name: "Send Raw Data"
    on_press:
      - lambda: |-
          std::array target_mac = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
          std::vector data = {0x01, 0x02, 0x03, 0x04};
          id(my_espnow).send_espnow_ex(data, target_mac);
```

## Handling Received Messages

The component provides three types of callbacks for handling incoming ESP-NOW communications:

### Callback: on_message

Triggered when any ESP-NOW message is received. Provides access to the raw message data and sender's MAC address.

```yaml
basic_espnowex:
  id: my_espnow
  on_message:
    - then:
        - lambda: |-
            ESP_LOGI("espnow", "Received message from %02X:%02X:%02X:%02X:%02X:%02X", 
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            // Convert byte vector to string for display
            std::string msg_str(message.begin(), message.end());
            ESP_LOGI("espnow", "Message content: %s", msg_str.c_str());
```

### Callback: on_recv_ack

Triggered when an acknowledgment is received. Provides the sender's MAC address.

```yaml
basic_espnowex:
  id: my_espnow
  on_recv_ack:
    - then:
        - lambda: |-
            ESP_LOGI("espnow", "Received ACK from %02X:%02X:%02X:%02X:%02X:%02X", 
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            // Do something when acknowledgment is received
```

### Callback: on_recv_cmd

Triggered when a command message is received. Provides the sender's MAC address and the command value.

```yaml
basic_espnowex:
  id: my_espnow
  on_recv_cmd:
    - then:
        - lambda: |-
            ESP_LOGI("espnow", "Received command %d from %02X:%02X:%02X:%02X:%02X:%02X", 
              cmd, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            if (cmd == 10) {
              // Turn on a light when command 10 is received
              id(my_light).turn_on();
            } else if (cmd == 20) {
              // Turn off a light when command 20 is received
              id(my_light).turn_off();
            }
```

## Complete Example

Here's a more comprehensive example showing how to create a simple ESP-NOW network with this component:

```yaml
esphome:
  name: espnow_node
  friendly_name: ESP-NOW Node

esp32:
  board: esp32dev

# Required for ESP-NOW to function properly
wifi:
  ssid: "MyWiFiNetwork"
  password: "MyWiFiPassword"

# Load the external component
external_components:
  - source: github://JakubObl/esphome-basic_espnowex

# Enable logging
logger:

# Configure ESP-NOW component
basic_espnowex:
  id: my_espnow
  peer_mac: "AA:BB:CC:DD:EE:FF"
  on_message:
    - then:
        - lambda: |-
            std::string msg_str(message.begin(), message.end());
            ESP_LOGI("espnow", "Message from %02X:%02X:%02X:%02X:%02X:%02X: %s", 
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], msg_str.c_str());
  
  on_recv_cmd:
    - then:
        - lambda: |-
            ESP_LOGI("espnow", "Command %d received", cmd);
            if (cmd == 1) {
              id(relay1).turn_on();
            } else if (cmd == 2) {
              id(relay1).turn_off();
            }

# Add a relay to be controlled via ESP-NOW
switch:
  - platform: gpio
    pin: GPIO12
    id: relay1
    name: "ESP-NOW Controlled Relay"

# Add buttons to send ESP-NOW commands
button:
  - platform: template
    name: "Send ON Command"
    on_press:
      - lambda: |-
          std::array target = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
          id(my_espnow).send_espnow_cmd(1, target);
  
  - platform: template
    name: "Send OFF Command"
    on_press:
      - lambda: |-
          std::array target = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
          id(my_espnow).send_espnow_cmd(2, target);
```

## Notes on ESP-NOW

- ESP-NOW works independently of traditional WiFi connections but requires the WiFi hardware to be initialized
- Devices can communicate even without being connected to the same WiFi network
- The maximum data payload is 250 bytes per message
- The component automatically adds peers when sending messages to new MAC addresses
- For reliable communication, ensure devices are within range of each other

This component provides a straightforward way to implement ESP-NOW communication in your ESPHome projects, enabling simple mesh networks, remote sensors, and other applications that benefit from direct device-to-device communication.
