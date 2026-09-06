#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <stdint.h>

constexpr uint8_t Xout{ 34 };
constexpr uint8_t Yout{ 35 };
constexpr uint8_t Sel{ 32 };
uint8_t dRead{};
uint16_t xRead{};
uint16_t yRead{};

uint8_t DR_OFF{ 1 };
uint8_t DR_ON{ 0 };

uint16_t XFORWARD_MIN{ 1700 };
uint16_t XFORWARD_MAX{ 2300 };

uint16_t XBACKWARD_MIN{ 1700 };
uint16_t XBACKWARD_MAX{ 2300 };

uint16_t XLEFT_MAX{ 1000 };

uint16_t XRIGHT_MAX{ 3000 };

uint16_t XSTOP_MIN{ 1700 };
uint16_t XSTOP_MAX{ 2300 };

uint16_t XCENTRE_MIN{ 1900 };
uint16_t XCENTRE_MAX{ 2100 };

uint16_t YFORWARD_MAX{ 3000 };

uint16_t YBACKWARD_MAX{ 1000 };

uint16_t YLEFT_MIN{ 1700 };
uint16_t YLEFT_MAX{ 2300 };

uint16_t YRIGHT_MIN{ 1700 };
uint16_t YRIGHT_MAX{ 2300 };

uint16_t YSTOP_MIN{ 1700 };
uint16_t YSTOP_MAX{ 2300 };

uint16_t YCENTRE_MIN{ 1920 };
uint16_t YCENTRE_MAX{ 2120 };

constexpr uint8_t ESPNOW_CHANNEL{ 1 };

enum class joyStick : uint8_t {
  F,
  B,
  L,
  R,
  S
};

joyStick direction{ };


enum class VehicleCommand : uint8_t {
  Forward,
  Backward,
  Left,
  Right,
  Stop,
  Invalid
};

joyStick determineDirection() {
  if (xRead >= XFORWARD_MIN && xRead <= XFORWARD_MAX && yRead >= YFORWARD_MAX && dRead == DR_OFF) {
    Serial.println("Forward");
    return joyStick::F;
  }

  else if (xRead >= XBACKWARD_MIN && xRead <= XBACKWARD_MAX && yRead <= YBACKWARD_MAX && dRead == DR_OFF) {
    Serial.println("Backward");
    return joyStick::B;
  }

  else if (xRead <= XLEFT_MAX && yRead >= YLEFT_MIN && yRead <= YLEFT_MAX && dRead == DR_OFF) {
    Serial.println("Left");
    return joyStick::L;
  }

  else if (xRead >= XRIGHT_MAX && yRead >= YRIGHT_MIN && yRead <= YRIGHT_MAX && dRead == DR_OFF) {
    Serial.println("Right");
    return joyStick::R;
  }

  if (xRead >= XCENTRE_MIN && xRead <= XCENTRE_MAX && yRead >= YCENTRE_MIN && yRead <= YCENTRE_MAX && dRead == DR_OFF) {
    Serial.println("Centre");
    return joyStick::S;
  }

  if (dRead == DR_ON) {
    Serial.println("Stop");
    return joyStick::S;
  }

  else {
    Serial.println("Stop");
    return joyStick::S;
  }
}

VehicleCommand convertDirection(joyStick controller) {
  switch (controller) {

    case joyStick::F:
      return VehicleCommand::Forward;

    case joyStick::B:
      return VehicleCommand::Backward;

    case joyStick::L:
      return VehicleCommand::Left;

    case joyStick::R:
      return VehicleCommand::Right;

    case joyStick::S:
      return VehicleCommand::Stop;
  }
  return VehicleCommand::Invalid;
}

struct ControlPacket {
  VehicleCommand command{ VehicleCommand::Stop };
  uint8_t speed{ 0 };
  uint32_t sequence{ 0 };
};


ControlPacket outgoingPacket{};

void prepareControlPacket(VehicleCommand command, uint8_t speed) {
  outgoingPacket.command = command;
  outgoingPacket.speed = speed;
  ++outgoingPacket.sequence;
}

void joystickRead() {
  dRead = digitalRead(Sel);
  Serial.print("Digital Reading: ");
  Serial.println(dRead);
  Serial.println();

  xRead = analogRead(Xout);
  Serial.print("X-axis Analog Reading: ");
  Serial.println(xRead);
  Serial.println();

  yRead = analogRead(Yout);
  Serial.print("Y-axis Analog Reading: ");
  Serial.println(yRead);
  Serial.println();
}

void onDataSent(
  const wifi_tx_info_t* transmissionInfo,
  esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Delivery Successful");
    Serial.println();
  } else {
    Serial.println("Delivery Failed.");
    Serial.println();
  }
}

constexpr uint8_t VEHICLE_MAC[6]{
  0x8c, 0x94, 0xdf, 0x4d, 0x18, 0xa4
};

esp_now_peer_info_t peerInfo{};

void setup() {
  Serial.begin(115200);
  pinMode(Xout, INPUT);
  pinMode(Yout, INPUT);
  pinMode(Sel, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  while (!WiFi.STA.started()) {
    delay(10);
  }

  WiFi.setChannel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP32 Initialised Succesfully!");
  } else {
    Serial.println("ESP Initialisation Error.  REBOOTING...");
    ESP.restart();
  }

  memcpy(peerInfo.peer_addr, VEHICLE_MAC, sizeof(VEHICLE_MAC));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("Vehicle Peer Registered!");
  } else {
    Serial.println("Failed To Register Peer Vehicle.");
  }

  esp_now_register_send_cb(onDataSent);
}

void loop() {
  joystickRead();

  direction = determineDirection();

  VehicleCommand command = convertDirection(direction);

  uint8_t speed{};

  if (command == VehicleCommand::Stop ||
      command == VehicleCommand::Invalid) {
    speed = 0;
  }
  else {
    speed = 130;
  }

  prepareControlPacket(command, speed);

  Serial.print("Sequence: ");
  Serial.println(outgoingPacket.sequence);

  Serial.print("Speed: ");
  Serial.println(outgoingPacket.speed);

  const esp_err_t sendResult{
    esp_now_send(
      VEHICLE_MAC,
      reinterpret_cast<const uint8_t*>(&outgoingPacket),
      sizeof(outgoingPacket))
  };

  if (sendResult == ESP_OK) {
    Serial.println("Send Queued");
  }
  else {
    Serial.println("Send request failed");
    Serial.println(esp_err_to_name(sendResult));
  }

  Serial.println();

  delay(50);
}
