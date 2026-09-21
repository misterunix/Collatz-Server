# Experimental Beowulf Server

## Cluster using ESP32 32bit Microcontrollers

Transport layer is The Espressif WiFi protocol ESP-Now

Protocol-MPI is a custom layer on top of ESP-Now

The server in this case is a ESP32 on a CYD

control

- 1 = ping  
- 2 = pong
- 4 = ack
- 8 =  
- 16 =  
- 32 =  
- 64 =  
- 128 =  

```c++
typedef struct now_msg
{
  uint8_t othermac[6];// the mac of a responding device
  uint8_t senderNode; // the node ID of the sender
  uint8_t othernode; // the node ID of the responding device
  uint8_t control; // control flags or commands
  uint8_t sequence; // sequence number of the message
  unsigned long long startnumber; // starting number for the computation
  unsigned long long length; // length of the computation range
  unsigned long long result; // result of the computation
  uint8_t status; // status of the message (e.g., MSG_FREE or MSG_BUSY)
  uint16_t checksum; // 16-bit checksum for data integrity
} now_msg;
```
