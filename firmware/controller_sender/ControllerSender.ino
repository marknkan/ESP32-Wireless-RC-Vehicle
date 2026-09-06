#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <stdint.h>

// Joystick connections. GPIO34 and GPIO35 are ADC1 inputs, so they remain
// available while the ESP32 Wi-Fi radio is active.
constexpr uint8_t X_AXIS_PIN{34};
constexpr uint8_t Y_AXIS_PIN{35};
constexpr uint8_t SELECT_PIN{32};

constexpr uint8_t ESPNOW_CHANNEL{1};
constexpr uint8_t DRIVE_SPEED{130};
constexpr uint32_t SEND_INTERVAL_MS{50};

// Calibrated 12-bit ADC thresholds. A wide perpendicular-axis band prevents
// diagonal stick movement from being interpreted as an unintended command.
constexpr uint16_t AXIS_BAND_MIN{1700};
constexpr uint16_t AXIS_BAND_MAX{2300};
constexpr uint16_t LEFT_THRESHOLD{1000};
constexpr uint16_t RIGHT_THRESHOLD{3000};
constexpr uint16_t BACKWARD_THRESHOLD{1000};
constexpr uint16_t FORWARD_THRESHOLD{3000};

enum class JoystickDirection : uint8_t {
  Forward,
  Backward,
  Left,
  Right,
  Stop
};

// This enum and ControlPacket must remain identical in the receiver sketch.
enum class VehicleCommand : uint8_t {
  Forward,
  Backward,
  Left,
  Right,
  Stop,
  Invalid
};

struct ControlPacket {
  VehicleCommand command{VehicleCommand::Stop};
  uint8_t speed{0};
  uint32_t sequence{0};
};

static_assert(sizeof(ControlPacket) == 8,
              "Sender and receiver must use the same packet layout");

// Station MAC address of the ESP32 installed on the vehicle.
constexpr uint8_t VEHICLE_MAC[6]{
  0x8c, 0x94, 0xdf, 0x4d, 0x18, 0xa4
};

ControlPacket outgoingPacket{};
esp_now_peer_info_t peerInfo{};

bool valueInBand(uint16_t value) {
  return value >= AXIS_BAND_MIN && value <= AXIS_BAND_MAX;
}

// Reads the physical controller. INPUT_PULLUP makes the Select input LOW when
// the joystick is pressed and HIGH when it is released.
void readJoystick(uint16_t& xValue, uint16_t& yValue, bool& selectPressed) {
  xValue = analogRead(X_AXIS_PIN);
  yValue = analogRead(Y_AXIS_PIN);
  selectPressed = digitalRead(SELECT_PIN) == LOW;
}

// Converts calibrated ADC readings into one safe, discrete controller state.
// Select has priority, and any unrecognised/diagonal position becomes Stop.
JoystickDirection determineDirection(uint16_t xValue,
                                      uint16_t yValue,
                                      bool selectPressed) {
  if (selectPressed) {
    return JoystickDirection::Stop;
  }

  if (valueInBand(xValue) && yValue >= FORWARD_THRESHOLD) {
    return JoystickDirection::Forward;
  }

  if (valueInBand(xValue) && yValue <= BACKWARD_THRESHOLD) {
    return JoystickDirection::Backward;
  }

  if (xValue <= LEFT_THRESHOLD && valueInBand(yValue)) {
    return JoystickDirection::Left;
  }

  if (xValue >= RIGHT_THRESHOLD && valueInBand(yValue)) {
    return JoystickDirection::Right;
  }

  return JoystickDirection::Stop;
}

VehicleCommand convertDirection(JoystickDirection direction) {
  switch (direction) {
    case JoystickDirection::Forward:
      return VehicleCommand::Forward;
    case JoystickDirection::Backward:
      return VehicleCommand::Backward;
    case JoystickDirection::Left:
      return VehicleCommand::Left;
    case JoystickDirection::Right:
      return VehicleCommand::Right;
    case JoystickDirection::Stop:
      return VehicleCommand::Stop;
  }

  return VehicleCommand::Invalid;
}

void prepareControlPacket(VehicleCommand command, uint8_t speed) {
  outgoingPacket.command = command;
  outgoingPacket.speed = speed;
  ++outgoingPacket.sequence;
}

void onDataSent(const wifi_tx_info_t* transmissionInfo,
                esp_now_send_status_t status) {
  (void)transmissionInfo;
  Serial.println(status == ESP_NOW_SEND_SUCCESS
                   ? "Delivery successful"
                   : "Delivery failed");
}

void setup() {
  Serial.begin(115200);
  pinMode(SELECT_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  while (!WiFi.STA.started()) {
    delay(10);
  }

  WiFi.setChannel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialisation failed; restarting");
    delay(1000);
    ESP.restart();
  }

  memcpy(peerInfo.peer_addr, VEHICLE_MAC, sizeof(VEHICLE_MAC));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Vehicle peer registration failed; restarting");
    delay(1000);
    ESP.restart();
  }

  if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
    Serial.println("Send callback registration failed; restarting");
    delay(1000);
    ESP.restart();
  }

  Serial.println("RC controller sender ready");
}

void loop() {
  uint16_t xValue{};
  uint16_t yValue{};
  bool selectPressed{};

  readJoystick(xValue, yValue, selectPressed);

  const JoystickDirection direction{
    determineDirection(xValue, yValue, selectPressed)
  };
  const VehicleCommand command{convertDirection(direction)};
  const uint8_t speed{
    command == VehicleCommand::Stop || command == VehicleCommand::Invalid
      ? 0
      : DRIVE_SPEED
  };

  prepareControlPacket(command, speed);

  Serial.print("X: ");
  Serial.print(xValue);
  Serial.print(" | Y: ");
  Serial.print(yValue);
  Serial.print(" | command: ");
  Serial.print(static_cast<uint8_t>(command));
  Serial.print(" | speed: ");
  Serial.print(outgoingPacket.speed);
  Serial.print(" | sequence: ");
  Serial.println(outgoingPacket.sequence);

  const esp_err_t sendResult{
    esp_now_send(VEHICLE_MAC,
                 reinterpret_cast<const uint8_t*>(&outgoingPacket),
                 sizeof(outgoingPacket))
  };

  if (sendResult != ESP_OK) {
    Serial.print("Send request failed: ");
    Serial.println(esp_err_to_name(sendResult));
  }

  delay(SEND_INTERVAL_MS);
}
