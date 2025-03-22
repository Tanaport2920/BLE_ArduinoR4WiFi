#include <Arduino.h>
#include "RobotBLEClient_For_ArduinoR4Wifi.h"

// サーバ(ESP32C6)と合わせたサービス/キャラUUID
static const char* SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0";
static const char* CHAR_UUID    = "abcdef01-1234-5678-1234-56789abcdef0";

// ライブラリのオブジェクト
RobotBLEClient_ArduinoBLE myRobot;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // 初期化
  myRobot.begin("ArduinoR4-Client");

  // 一旦接続を試みる
  if (!myRobot.connectToController(SERVICE_UUID, CHAR_UUID)) {
    Serial.println("Failed to connect at first, will try again in update().");
  } else {
    Serial.println("Initial connect OK!");
  }
}

void loop() {
  // BLE更新
  myRobot.update();

  // 受信データを1秒ごとに表示
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();

    if (myRobot.isConnected()) {
      ControllerData data = myRobot.getControllerData();

      bool a = (data.buttons & 0x01);
      bool b = (data.buttons & 0x02);
      bool x = (data.buttons & 0x04);
      bool y = (data.buttons & 0x08);

      Serial.print("ABXY = ");
      Serial.print(a ? "A " : "- ");
      Serial.print(b ? "B " : "- ");
      Serial.print(x ? "X " : "- ");
      Serial.print(y ? "Y " : "- ");
      Serial.print(" | X=");
      Serial.print(data.x);
      Serial.print(", Y=");
      Serial.println(data.y);
    } else {
      Serial.println("Not connected.");
    }
  }
}
