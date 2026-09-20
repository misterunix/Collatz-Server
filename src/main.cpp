#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// 28:05:a5:33:23:fc
#define MSG_FREE 0
#define MSG_BUSY 1

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
uint16_t calculate_16_bit_checksum(const uint8_t *data, size_t length);
void set_hardware_wifi_channel(uint8_t channel);

// Replace with your receiver's MAC address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
// Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define RED_LED 4
#define GREEN_LED 17
#define BLUE_LED 16

#define LDR 34

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 1

#define CHANNEL 3

typedef struct now_msg
{
  uint8_t othermax[6];
  uint8_t mynode;
  uint8_t othernode;
  uint8_t control;
  uint8_t sequence;
  unsigned long long startnumber;
  unsigned long long length;
  unsigned long long result;
  uint8_t status;
  uint16_t checksum;
} now_msg;

#define MSG_COUNT 64
now_msg msg[MSG_COUNT];

void printErrorToDisplay(String errorMessage);

// Touchscreen coordinates: (x, y) and pressure (z)
int touch_x, touch_y, touch_z;
bool backlightOn = false;

long blPreviousMillis = 0;
long blInterval = 1000;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;

esp_now_peer_info_t peerInfo;

// Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
void printTouchToSerial(int touchX, int touchY, int touchZ)
{
  Serial.print("X = ");
  Serial.print(touchX);
  Serial.print(" | Y = ");
  Serial.print(touchY);
  Serial.print(" | Pressure = ");
  Serial.print(touchZ);
  Serial.println();
}

// Print Touchscreen info about X, Y and Pressure (Z) on the TFT Display
void printTouchToDisplay(int touchX, int touchY, int touchZ)
{
  // Clear TFT screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int centerX = SCREEN_WIDTH / 2;
  int textY = 80;

  String tempText = "X = " + String(touchX);
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  textY += 20;
  tempText = "Y = " + String(touchY);
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  textY += 20;
  tempText = "Pressure = " + String(touchZ);
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);
}

void printErrorToDisplay(String errorMessage)
{
  // Clear TFT screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int centerX = SCREEN_WIDTH / 2;
  int textY = 80;

  tft.drawCentreString(errorMessage, centerX, textY, FONT_SIZE);

  // String tempText = "X = " + String(touchX);
  // tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  // textY += 20;
  // tempText = "Y = " + String(touchY);
  // tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  // textY += 20;
  // tempText = "Pressure = " + String(touchZ);
  // tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);
}

void setup()
{
  currentMillis = millis();
  previousMillis = currentMillis;
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, HIGH);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  backlightOn = true;

  pinMode(LDR, INPUT);

  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin();
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(1);

  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Set X and Y coordinates for center of display
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;

  tft.drawCentreString("Hello, world!", centerX, 30, FONT_SIZE);
  tft.drawCentreString("Touch screen to test", centerX, centerY, FONT_SIZE);

  for (int i = 0; i < MSG_COUNT; i++)
  {
    msg[i].status = MSG_FREE;
    msg[i].sequence = 0;
    msg[i].checksum = 0;
    msg[i].length = 10000000ULL;
    msg[i].result = 0ULL;
    msg[i].startnumber = 0ULL;
    msg[i].control = 0;
    msg[i].othernode = 255;
    msg[i].mynode = 0; // 0 is Always the server
  }

  // ESP-NOW requires WiFi to be initialized in station mode first
  WiFi.mode(WIFI_STA);
  set_hardware_wifi_channel(CHANNEL);
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the send callback
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Register peer

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW Initialized!");
}

void turnBacklightOnOff()
{

  unsigned long blCurrentMillis = millis();
  if (blCurrentMillis - blPreviousMillis >= blInterval)
  {
    blPreviousMillis = blCurrentMillis;
    // Place any code here that you want to run at the specified interval

    /*  Read the LDR value and map it to a backlight value for the TFT display
        The LDR value is inverted, so that when the LDR is in darkness, the backlight is at maximum brightness (255)
        When the LDR is in bright light, the backlight is at minimum brightness (20)
    */
    uint16_t lightlevel = analogRead(LDR);
    lightlevel = constrain(lightlevel, 0, 600);
    if (lightlevel > 90)
    {
      digitalWrite(TFT_BL, TFT_BACKLIGHT_OFF);
      backlightOn = false;
    }
    else
    {
      digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
      backlightOn = true;
    }
  }
}

void loop()
{

  turnBacklightOnOff();

  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z) info on the TFT display and Serial Monitor
  if (touchscreen.tirqTouched() && touchscreen.touched())
  {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    touch_x = map(p.x, 3660, 300, 1, SCREEN_WIDTH);
    touch_y = map(p.y, 300, 3500, 1, SCREEN_HEIGHT);
    touch_z = p.z;

    // printTouchToSerial(p.x, p.y, touch_z);
    printTouchToDisplay(touch_x, touch_y, touch_z);

    delay(100);
  }

  currentMillis = millis();
  if (currentMillis - previousMillis >= 5000)
  {
    previousMillis = currentMillis;
    // Place any code here that you want to run every 10 seconds

    msg[0].sequence++;
    msg[0].control = 1; // ping
    msg[0].length = 10000000;
    msg[0].startnumber = 0; // example value
    msg[0].result = 0;      // example value
    msg[0].status = 0;      // example value
    msg[0].checksum = calculate_16_bit_checksum((const uint8_t *)&msg[0], sizeof(now_msg));

    // peerInfo.channel = CHANNEL;
    esp_err_t result = esp_now_send(peerInfo.peer_addr, (const uint8_t *)&msg[0], sizeof(now_msg));
    if (result == ESP_OK)
    {
      Serial.println("Sent with success");
    }
    else
    {
      Serial.println("Error sending the data");
    }
  }
}

// Calculates a 16-bit checksum by summing all bytes in the buffer.
uint16_t calculate_16_bit_checksum(const uint8_t *data, size_t length)
{
  uint16_t checksum = 0;
  for (size_t i = 0; i < length; i++)
  {
    checksum += data[i];
  }
  return checksum;
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.printf("other mac: %02X:%02X:%02X:%02X:%02X:%02X\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  // Copy incoming memory buffer directly into our structure variables
  memcpy(&msg[0], incomingData, sizeof(now_msg));

  Serial.println("\n--- New Packet Received ---");

  Serial.printf("Rcv: %02X:%02X:%02X:%02X:%02X:%02X\n", msg[0].othermax[0],
                msg[0].othermax[1], msg[0].othermax[2], msg[0].othermax[3],
                msg[0].othermax[4], msg[0].othermax[5]);
  Serial.printf("Other node: %i\n", msg[0].othernode);
  Serial.printf("Control: %i\n", msg[0].control);
  Serial.printf("Sequence: %i\n", msg[0].sequence);
  Serial.println(msg[0].startnumber);
  Serial.println(msg[0].length);
  Serial.println(msg[0].result);
  Serial.printf("Status: %i\n", msg[0].status);
  Serial.printf("Checksum: %04X\n", msg[0].checksum);
}

void set_hardware_wifi_channel(uint8_t channel)
{
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}