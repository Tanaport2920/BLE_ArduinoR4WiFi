#ifndef ROBOT_BLE_CLIENT_ARDUINO_BLE_H
#define ROBOT_BLE_CLIENT_ARDUINO_BLE_H

#include <Arduino.h>
#include <ArduinoBLE.h>

/*
  受信したいデータ構造
  - ボタン（buttons）: bit0=>A, bit1=>B, bit2=>X, bit3=>Y
  - x, y             : int16_t (アナログスティックなど)
*/
struct ControllerData {
  uint8_t buttons;
  int16_t x;
  int16_t y;
};

class RobotBLEClient_ArduinoBLE {
public:
  RobotBLEClient_ArduinoBLE();
  ~RobotBLEClient_ArduinoBLE();

  // BLEクライアント初期化
  //   deviceName: 広告上のデバイス名 (Arduino BLE Centralモードではあまり使わないが形だけ受け取る)
  void begin(const char* deviceName = "ArduinoR4");

  // サーバ(ESP32C6)へ接続する
  //   serviceUUID, characteristicUUID: ESP32C6側と一致する文字列 (例: "12345678-1234-5678-1234-56789abcdef0")
  //   戻り値: trueなら成功
  bool connectToController(const char* serviceUUID, const char* characteristicUUID);

  // 受信したデータを取得
  ControllerData getControllerData();

  // loop内で呼ぶ：切断確認・再接続など
  void update();

  // 接続状態を返す
  bool isConnected() const { return m_connected; }

private:
  // Notifyコールバック
  // ArduinoBLEの場合は「setEventHandler(BLECharacteristicEventHandler)」で関数ポインタ指定。
  static void notifyHandler(BLEDevice central, BLECharacteristic characteristic);

  // 再接続処理
  bool reconnectIfNeeded();

private:
  bool          m_connected;
  ControllerData m_data;         // 最新の受信データ

  // 接続先情報を保持しておき、再接続で使う
  String m_serviceUUID;
  String m_charUUID;

  // 今接続中の中央役(＝自分)が発見した周辺機器 (サーバ)
  BLEDevice m_foundPeripheral;   // ArduinoBLE では「BLEDevice」がスキャンなどで見つかる。

  // 対象のサービス・キャラ
  BLEService       m_foundService;
  BLECharacteristic m_foundChar;
};

#endif
