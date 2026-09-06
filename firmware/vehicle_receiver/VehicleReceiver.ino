#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

constexpr uint8_t ESPNOW_CHANNEL{ 1 };

constexpr uint8_t PWMA_PIN{ 25 };
constexpr uint8_t AIN1_PIN{ 26 };
constexpr uint8_t AIN2_PIN{ 27 };
constexpr uint8_t STBY_PIN{ 23 };
constexpr uint8_t PWMB_PIN{ 13 };
constexpr uint8_t BIN1_PIN{ 14 };
constexpr uint8_t BIN2_PIN{ 33 };
constexpr uint8_t LEFT_FORWARD_SPEED{ 228 };
constexpr uint8_t RIGHT_FORWARD_SPEED{ 130 };
constexpr uint8_t LEFT_BACKWARD_SPEED{ 219 };
constexpr uint8_t RIGHT_BACKWARD_SPEED{ 130 };
constexpr uint8_t DRIVE_SPEED{ 130 };
constexpr uint32_t COMMAND_TIMEOUT_MS{ 2000 };

enum class VehicleCommand : uint8_t {
  Forward,
  Backward,
  Left,
  Right,
  Stop,
  Invalid
};

struct ControlPacket {
  VehicleCommand command{ VehicleCommand::Stop };
  uint8_t speed{ 0 };
  uint32_t sequence{ 0 };
};

ControlPacket incomingPacket{};

volatile bool newPacketAvailable{ false };

uint32_t lastValidCommandTime_MS{};
bool vehicleMoving{ false };


void enterStandby() {
  digitalWrite(STBY_PIN, LOW);
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
  analogWrite(PWMB_PIN, 0);
}

void motorAClockwise(uint8_t speed) {
  analogWrite(PWMA_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorACounterClockwise(uint8_t speed) {
  analogWrite(PWMA_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, speed);
}

void brakeMotorA() {
  analogWrite(PWMA_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, 0);
}

void coastMotorA() {
  analogWrite(PWMA_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 255);
}

void motorBClockwise(uint8_t speed) {
  analogWrite(PWMB_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(BIN1_PIN, HIGH);
  digitalWrite(BIN2_PIN, LOW);
  analogWrite(PWMB_PIN, speed);
}

void motorBCounterClockwise(uint8_t speed) {
  analogWrite(PWMB_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, HIGH);
  analogWrite(PWMB_PIN, speed);
}

void brakeMotorB() {
  analogWrite(PWMB_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(BIN1_PIN, HIGH);
  digitalWrite(BIN2_PIN, HIGH);
  analogWrite(PWMB_PIN, 0);
}

void coastMotorB() {
  analogWrite(PWMB_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, LOW);
  analogWrite(PWMB_PIN, 255);
}

void RCForward() {
  motorAClockwise(LEFT_FORWARD_SPEED);
  motorBClockwise(RIGHT_FORWARD_SPEED);
}

void RCBackward() {
  motorACounterClockwise(LEFT_BACKWARD_SPEED);
  motorBCounterClockwise(RIGHT_BACKWARD_SPEED);
}

void RCLeft(uint8_t speed) {
  motorAClockwise(speed);
  motorBCounterClockwise(speed);
}
void RCRight(uint8_t speed) {
  motorACounterClockwise(speed);
  motorBClockwise(speed);
}

void RCCoast() {
  coastMotorA();
  coastMotorB();
}

void RCStop() {
  brakeMotorA();
  brakeMotorB();
}

void onDataReceived(
  const esp_now_recv_info_t* receiveInfo,
  const uint8_t* receivedData,
  int dataLength) {
  if (dataLength != sizeof(ControlPacket)) {
    return;
  }


  memcpy(
    &incomingPacket,
    receivedData,
    sizeof(incomingPacket));

  newPacketAvailable = true;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  while (!WiFi.STA.started()) {
    delay(10);
  }

  WiFi.setChannel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Initialisation failed.");
    return;
  }

  if (esp_now_register_recv_cb(onDataReceived) != ESP_OK) {
    Serial.println("Receive callback registration failed.");
    return;
  }

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(PWMB_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);

  enterStandby();

  Serial.println("Vehicle ESP-NOW receiver ready! ");
}

void loop() {
  if (newPacketAvailable) {
    newPacketAvailable = false;

    Serial.print("Speed: ");
    Serial.println(incomingPacket.speed);

    Serial.print("Sequence: ");
    Serial.println(incomingPacket.sequence);

    switch (incomingPacket.command) {
      case VehicleCommand::Forward:
        RCForward();
        lastValidCommandTime_MS = millis();
        Serial.print("Last Valid Command Time: ");
        Serial.println(lastValidCommandTime_MS);
        vehicleMoving = true;
        break;

      case VehicleCommand::Backward:
        RCBackward();
        lastValidCommandTime_MS = millis();
        Serial.print("Last Valid Command Time: ");
        Serial.println(lastValidCommandTime_MS);
        vehicleMoving = true;
        break;

      case VehicleCommand::Left:
        RCLeft(incomingPacket.speed);
        lastValidCommandTime_MS = millis();
        Serial.print("Last Valid Command Time: ");
        Serial.println(lastValidCommandTime_MS);
        vehicleMoving = true;
        break;

      case VehicleCommand::Right:
        RCRight(incomingPacket.speed);
        lastValidCommandTime_MS = millis();
        Serial.print("Last Valid Command Time: ");
        Serial.println(lastValidCommandTime_MS);
        vehicleMoving = true;
        break;

      case VehicleCommand::Stop:
        RCStop();
        lastValidCommandTime_MS = millis();
        Serial.print("Last Valid Command Time: ");
        Serial.println(lastValidCommandTime_MS);
        vehicleMoving = false;
        break;

      case VehicleCommand::Invalid:
        Serial.println("Invalid Command");
        RCStop();
        vehicleMoving = false;
        break;
    }
  }
  if (vehicleMoving) {
    const uint32_t elapsedTime{
      millis() - lastValidCommandTime_MS
    };

    if (elapsedTime >= COMMAND_TIMEOUT_MS) {
      RCStop();
      vehicleMoving = false;
      Serial.println("Wireless command timeout");
    }
  }
}
