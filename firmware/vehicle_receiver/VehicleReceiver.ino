#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <stdint.h>

constexpr uint8_t ESPNOW_CHANNEL{1};

// TB6612FNG control pins.
constexpr uint8_t PWMA_PIN{25};
constexpr uint8_t AIN1_PIN{26};
constexpr uint8_t AIN2_PIN{27};
constexpr uint8_t STBY_PIN{23};
constexpr uint8_t PWMB_PIN{13};
constexpr uint8_t BIN1_PIN{14};
constexpr uint8_t BIN2_PIN{33};

// Independently calibrated straight-line speeds compensate for differences
// between the two open-loop motors.
constexpr uint8_t LEFT_FORWARD_SPEED{228};
constexpr uint8_t RIGHT_FORWARD_SPEED{130};
constexpr uint8_t LEFT_BACKWARD_SPEED{219};
constexpr uint8_t RIGHT_BACKWARD_SPEED{130};

// Stop the vehicle if valid movement commands disappear for two seconds.
constexpr uint32_t COMMAND_TIMEOUT_MS{2000};

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

ControlPacket incomingPacket{};
volatile bool newPacketAvailable{false};
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;

uint32_t lastValidCommandTimeMs{};
bool vehicleMoving{false};

void configureMotorPins() {
  pinMode(PWMA_PIN, OUTPUT);
  pinMode(PWMB_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);
}

// Places the driver in its lowest-risk state during startup or radio failure.
void enterStandby() {
  analogWrite(PWMA_PIN, 0);
  analogWrite(PWMB_PIN, 0);
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, LOW);
  digitalWrite(STBY_PIN, LOW);
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

void brakeMotorA() {
  analogWrite(PWMA_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
}

void brakeMotorB() {
  analogWrite(PWMB_PIN, 0);
  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(BIN1_PIN, HIGH);
  digitalWrite(BIN2_PIN, HIGH);
}

void driveForward() {
  motorAClockwise(LEFT_FORWARD_SPEED);
  motorBClockwise(RIGHT_FORWARD_SPEED);
}

void driveBackward() {
  motorACounterClockwise(LEFT_BACKWARD_SPEED);
  motorBCounterClockwise(RIGHT_BACKWARD_SPEED);
}

void turnLeft(uint8_t speed) {
  motorAClockwise(speed);
  motorBCounterClockwise(speed);
}

void turnRight(uint8_t speed) {
  motorACounterClockwise(speed);
  motorBClockwise(speed);
}

void stopVehicle() {
  brakeMotorA();
  brakeMotorB();
}

bool commandIsSupported(VehicleCommand command) {
  return static_cast<uint8_t>(command) <=
         static_cast<uint8_t>(VehicleCommand::Stop);
}

// ESP-NOW executes this callback outside loop(). The critical section ensures
// loop() never reads the packet while the callback is replacing it.
void onDataReceived(const esp_now_recv_info_t* receiveInfo,
                    const uint8_t* receivedData,
                    int dataLength) {
  (void)receiveInfo;

  if (dataLength != sizeof(ControlPacket)) {
    return;
  }

  ControlPacket packetCopy{};
  memcpy(&packetCopy, receivedData, sizeof(packetCopy));

  portENTER_CRITICAL(&packetMux);
  incomingPacket = packetCopy;
  newPacketAvailable = true;
  portEXIT_CRITICAL(&packetMux);
}

void applyPacket(const ControlPacket& packet) {
  if (!commandIsSupported(packet.command)) {
    Serial.println("Unsupported command; stopping vehicle");
    stopVehicle();
    vehicleMoving = false;
    return;
  }

  lastValidCommandTimeMs = millis();

  switch (packet.command) {
    case VehicleCommand::Forward:
      driveForward();
      vehicleMoving = true;
      break;

    case VehicleCommand::Backward:
      driveBackward();
      vehicleMoving = true;
      break;

    case VehicleCommand::Left:
      turnLeft(packet.speed);
      vehicleMoving = true;
      break;

    case VehicleCommand::Right:
      turnRight(packet.speed);
      vehicleMoving = true;
      break;

    case VehicleCommand::Stop:
      stopVehicle();
      vehicleMoving = false;
      break;

    case VehicleCommand::Invalid:
    default:
      stopVehicle();
      vehicleMoving = false;
      break;
  }

  Serial.print("Command: ");
  Serial.print(static_cast<uint8_t>(packet.command));
  Serial.print(" | speed: ");
  Serial.print(packet.speed);
  Serial.print(" | sequence: ");
  Serial.println(packet.sequence);
}

void setup() {
  Serial.begin(115200);

  // Establish a safe motor state before initialising the wireless subsystem.
  configureMotorPins();
  enterStandby();

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  while (!WiFi.STA.started()) {
    delay(10);
  }

  WiFi.setChannel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialisation failed; motors remain in standby");
    return;
  }

  if (esp_now_register_recv_cb(onDataReceived) != ESP_OK) {
    Serial.println("Receive callback registration failed; motors remain safe");
    return;
  }

  Serial.print("Vehicle receiver ready on channel ");
  Serial.println(WiFi.channel());
}

void loop() {
  ControlPacket packetToProcess{};
  bool packetReady{false};

  portENTER_CRITICAL(&packetMux);
  if (newPacketAvailable) {
    packetToProcess = incomingPacket;
    newPacketAvailable = false;
    packetReady = true;
  }
  portEXIT_CRITICAL(&packetMux);

  if (packetReady) {
    applyPacket(packetToProcess);
  }

  if (vehicleMoving &&
      millis() - lastValidCommandTimeMs >= COMMAND_TIMEOUT_MS) {
    stopVehicle();
    vehicleMoving = false;
    Serial.println("Wireless command timeout; vehicle stopped");
  }
}
