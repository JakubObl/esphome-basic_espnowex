# BasicESPNowEx - Enhanced ESP-NOW Component for ESPHome

This document describes an advanced ESP-NOW component for ESPHome that extends standard ESP-NOW functionality with a reliable communication system featuring delivery confirmations, message retransmission, and deduplication mechanisms. Designed to ensure stable wireless communication between ESP32/ESP8266 devices in home automation environments.

## Core Features

### Reliable Communication System
BasicESPNowEx implements an advanced communication protocol surpassing standard ESP-NOW capabilities. The component automatically adds message headers containing unique identifiers that enable delivery status tracking and duplicate elimination. The system uses ACK (acknowledgement) mechanisms to verify message receipt, crucial for high-reliability applications.

Each sent message receives a 3-byte identifier generated from timestamp and random number components, ensuring uniqueness over extended periods. Messages are automatically retransmitted if no confirmation is received within a configurable timeout, with user-adjustable retry attempts and timeout settings.

### Deduplication & Message History
The component maintains a 300-second received message history, enabling duplicate detection and rejection. The system compares combinations of sender MAC addresses and full message content, effectively preventing duplicate processing. Automatic cleanup of outdated entries optimizes memory usage.

## Component Configuration

### Basic Setup
```yaml
external_components:
  - source: github://your-organization/basicespnowex

esphome:
  name: esp-device
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "YourNetwork"
  password: "YourPassword"

api:
  encryption:
    key: "your-api-key"

ota:
  password: "ota-password"

basicespnowex:
  id: espnow_component
  peer_mac: "AA:BB:CC:DD:EE:FF"
  max_retries: 5
  timeout_us: 200000
```

### Configuration Parameters
The `peer_mac` parameter defines the default target MAC address for point-to-point communication, while `max_retries` specifies maximum retransmission attempts. The `timeout_us` setting determines acknowledgement wait time in microseconds (default 200,000μs = 200ms). All parameters are optional with sensible defaults - unspecified `peer_mac` uses broadcast address (FF:FF:FF:FF:FF:FF).

## Events & Triggers

### on_message Trigger
```yaml
basicespnowex:
  on_message:
    - then:
        - logger.log:
            format: "Received message from %s: %s"
            args: ['id(mac_to_string(mac))', 'message.c_str()']
```
Activated when receiving valid text messages, providing `mac` (6-byte sender MAC) and `message` (string payload) parameters.

### on_recv_data Trigger
```yaml
basicespnowex:
  on_recv_data:
    - then:
        - lambda: |-
            ESP_LOGI("data", "Received %d bytes", data.size());
            for(auto byte : data) {
              ESP_LOGI("data", "Byte: 0x%02X", byte);
            }
```
Handles all binary data types. The `data` parameter contains raw payload bytes after protocol header removal.

### on_recv_cmd Trigger
```yaml
basicespnowex:
  on_recv_cmd:
    - then:
        - if:
            condition: 'cmd == 1001'
            then:
              - switch.toggle: relay1
```
Specialized handler for 4-byte numeric commands where first and second byte pairs match. The `cmd` parameter contains 16-bit big-endian command value.

### on_recv_ack Trigger
```yaml
basicespnowex:
  on_recv_ack:
    - then:
        - logger.log:
            format: "ACK from %s for message %02X%02X%02X"
            args: ['id(mac_to_string(mac))', 'msg_id[0]', 'msg_id[1]', 'msg_id[2]']
```
Provides message delivery confirmation with sender MAC and 3-byte message ID parameters.

## Message Transmission Methods

### Broadcast Communication
```yaml
button:
  - platform: template
    name: "Send Broadcast"
    on_press:
      - basicespnowex.send_broadcast_str:
          id: espnow_component
          message: "Message to all devices"
```
Broadcast messages don't require ACKs, ideal for status updates or system announcements.

### Point-to-Point Communication
```yaml
button:
  - platform: template
    name: "Send to Specific MAC"
    on_press:
      - basicespnowex.send_espnow_str:
          id: espnow_component
          message: "Test message"
          peer_mac: "11:22:33:44:55:66"
```
Implements automatic ACK verification and retransmission. Messages are tracked until confirmation or retry limit exhaustion.

## Command System (CMD)

### Command Structure
Commands use a 4-byte format with duplicate verification:
```yaml
button:
  - platform: template
    name: "Send Command 1001"
    on_press:
      - basicespnowex.send_espnow_cmd:
          id: espnow_component
          cmd: 1001
          peer_mac: "AA:BB:CC:DD:EE:FF"
```
The system prevents command duplication by checking for identical pending commands to the same device.

## Retransmission & Reliability

### Retry Algorithm
Implements optimized retransmission with periodic queue checks (400ms interval). Configurable parameters:
```yaml
basicespnowex:
  max_retries: 3      # Maximum 3 attempts
  timeout_us: 500000  # 500ms between attempts
```
Failed messages are removed after exceeding retry limits. All retransmissions are logged with target MAC, message ID, and attempt count.

### Peer Management
Automatic peer registration and channel synchronization with WiFi events. Failed peer additions trigger retries (max 3 attempts) before message discard.

## Thread Safety & Optimization

### Synchronization Mechanisms
Uses dual mutex system:
- `queue_mutex_`: Protects message queue access
- `history_mutex_`: Secures message history
Non-blocking operations maintain system responsiveness.

### Memory Management
- Message history: 1000-entry limit with 300-second TTL
- Dynamic pending message queue with automatic expiration

## Advanced Implementations

### Environmental Monitoring
```yaml
sensor:
  - platform: dht
    temperature:
      name: "Temperature"
      on_value:
        then:
          - basicespnowex.send_espnow_str:
              id: espnow_component
              message: !lambda "return 'TEMP:' + to_string(x);"
```

### Remote Device Control
```yaml
basicespnowex:
  on_recv_cmd:
    - lambda: |-
        switch(cmd) {
          case 1001: id(light1).toggle();
          case 1002: id(relay1).toggle();
        }
```

## Debugging & Diagnostics

### Log Configuration
```yaml
logger:
  level: DEBUG
  logs:
    basic_espnowex: DEBUG
```
Detailed logs track retransmissions, peer management, and transmission statuses.

### Common Issues
1. **Connection Failures**: Verify identical WiFi channels and MAC addresses
2. **Packet Loss**: Increase timeout intervals in noisy environments
3. **Peer Registration**: Check ESP-NOW initialization sequence

## Conclusion
BasicESPNowEx significantly enhances ESP-NOW with professional-grade reliability features. The combination of delivery confirmation, automatic retries, and deduplication makes it ideal for applications requiring guaranteed message delivery. Comprehensive triggers and communication methods enable complex protocol implementations tailored for home automation and IoT systems.
