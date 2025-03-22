#include "RobotBLEClient_For_ArduinoR4Wifi.h"

static unsigned long s_lastReconnectAttempt = 0; // 再接続用タイマー
static const unsigned long RECONNECT_INTERVAL = 10000; // 10秒おきに再接続試行

//////////////////////////////////////////////////////////////////////////
// 静的コールバック関数
void RobotBLEClient_ArduinoBLE::notifyHandler(BLEDevice central, BLECharacteristic characteristic) {
  // 送られてきたデータを読み出す
  int dataSize = characteristic.valueLength();
  if (dataSize == sizeof(ControllerData)) {
    ControllerData tmp;
    memcpy(&tmp, characteristic.value(), dataSize);

    // デバッグ出力したい場合
    // Serial.println("[RobotBLEClient_ArduinoBLE] Notified new data!");
    // Serial.print("  buttons = 0x"); Serial.println(tmp.buttons, HEX);
    // Serial.print("  x = "); Serial.println(tmp.x);
    // Serial.print("  y = "); Serial.println(tmp.y);

    // ここでメンバに代入したいが、static関数なので直接アクセスは不可。
    // → 本来はシングルトンやグローバル変数参照するなどの方法が必要。
    //   簡易サンプルとしては「controllerDataをグローバルに置く」など。
    //   ここではライブラリ構造上、面倒なので… いったん無視 or 代替策を使う

    // ArduinoBLEライブラリではCharacteristicの「.valueUpdated()」イベントなどで
    // loop側で都度読み取る方法が一般的。
    // このサンプルでは後で getControllerData() 時に読み取れるように
    // "m_foundChar" から直接読み込む設計でもOKです。（後述）
  }
}

//////////////////////////////////////////////////////////////////////////
// コンストラクタ・デストラクタ
RobotBLEClient_ArduinoBLE::RobotBLEClient_ArduinoBLE() {
  m_connected = false;
  memset(&m_data, 0, sizeof(m_data));
}

RobotBLEClient_ArduinoBLE::~RobotBLEClient_ArduinoBLE() {
}

//////////////////////////////////////////////////////////////////////////
// 初期化
void RobotBLEClient_ArduinoBLE::begin(const char* deviceName) {
  // ArduinoBLEを初期化
  if (!BLE.begin()) {
    Serial.println("[RobotBLEClient_ArduinoBLE] BLE init failed!");
  } else {
    Serial.println("[RobotBLEClient_ArduinoBLE] BLE init OK.");
  }

  // デバイス名はセントラルモードだとあまり意味はないが、念のため
  BLE.setDeviceName(deviceName);
}

//////////////////////////////////////////////////////////////////////////
// サーバ(ESP32C6)へ接続
bool RobotBLEClient_ArduinoBLE::connectToController(const char* serviceUUID, const char* characteristicUUID) {
  // 値を保持しておき、再接続で使う
  m_serviceUUID = serviceUUID;
  m_charUUID    = characteristicUUID;

  // まず、serviceUUIDをターゲットにスキャン (タイムアウトは10秒にしてみる)
  // 非同期APIなので、ブロッキングでやるには工夫が必要
  Serial.println("[RobotBLEClient_ArduinoBLE] Scanning...");
  if (!BLE.scanForUuid(serviceUUID, 10)) {
    Serial.println("[RobotBLEClient_ArduinoBLE] No device found with target service UUID.");
    return false;
  }

  // スキャンが完了したら、loop() で BLEDevice が列挙できる
  bool found = false;
  while (BLE.available()) {
    BLEDevice peripheral = BLE.available();
    if (peripheral && peripheral.hasService(serviceUUID)) {
      // 見つかった
      m_foundPeripheral = peripheral;
      found = true;
      break;
    }
  }
  if (!found) {
    Serial.println("[RobotBLEClient_ArduinoBLE] No peripheral matched.");
    return false;
  }

  // 接続を試みる
  Serial.print("[RobotBLEClient_ArduinoBLE] Connecting to ");
  Serial.println(m_foundPeripheral.address());

  if (!m_foundPeripheral.connect()) {
    Serial.println("[RobotBLEClient_ArduinoBLE] Connection failed.");
    return false;
  }
  Serial.println("[RobotBLEClient_ArduinoBLE] Connected!");

  // Serviceを発見
  if (!m_foundPeripheral.discoverService(serviceUUID)) {
    Serial.println("[RobotBLEClient_ArduinoBLE] Service not found on this device.");
    m_foundPeripheral.disconnect();
    return false;
  }

  m_foundService = m_foundPeripheral.service(serviceUUID);
  if (!m_foundService) {
    Serial.println("[RobotBLEClient_ArduinoBLE] Service handle invalid.");
    m_foundPeripheral.disconnect();
    return false;
  }

  // Characteristic取得
  m_foundChar = m_foundService.characteristic(characteristicUUID);
  if (!m_foundChar) {
    Serial.println("[RobotBLEClient_ArduinoBLE] Characteristic not found!");
    m_foundPeripheral.disconnect();
    return false;
  }

  // Notify受信に対応していれば設定
  if (m_foundChar.canSubscribe()) {
    // ArduinoBLEは関数ポインタでイベントハンドラをセット
    m_foundChar.subscribe();
    // ここでハンドラをセットすると、本来は notifyHandler が呼ばれる
    m_foundChar.setEventHandler(BLESubscribed, notifyHandler);
    m_foundChar.setEventHandler(BLEUpdated, notifyHandler);

    Serial.println("[RobotBLEClient_ArduinoBLE] Subscribed for notifications.");
  } else {
    Serial.println("[RobotBLEClient_ArduinoBLE] This characteristic cannot notify.");
  }

  m_connected = true;
  Serial.println("[RobotBLEClient_ArduinoBLE] connectToController OK");
  return true;
}

//////////////////////////////////////////////////////////////////////////
// 受信データ取得
ControllerData RobotBLEClient_ArduinoBLE::getControllerData() {
  // ArduinoBLEの場合、Notifyハンドラで直接コピーするか、
  // あるいは必要なときに characteristic.value() から読み取る方法があります。
  //
  // ここでは characteristic.valueUpdated() が true なら
  // そこから読み取る感じにしてみる:
  if (m_foundChar && m_foundChar.valueUpdated()) {
    // データ長が正しいかチェック
    int len = m_foundChar.valueLength();
    if (len == sizeof(ControllerData)) {
      memcpy(&m_data, m_foundChar.value(), len);
    }
  }

  return m_data;
}

//////////////////////////////////////////////////////////////////////////
// update()で接続監視・再接続
void RobotBLEClient_ArduinoBLE::update() {
  // ArduinoBLEは裏でpollしないとイベント発生しないため
  BLE.poll();

  // すでに接続中の場合、切断されたかどうか
  if (m_connected) {
    if (!m_foundPeripheral.connected()) {
      Serial.println("[RobotBLEClient_ArduinoBLE] Disconnected. Will reconnect...");
      m_connected = false;
    }
  }

  // 切断されていれば再接続
  if (!m_connected) {
    reconnectIfNeeded();
  }
}

bool RobotBLEClient_ArduinoBLE::reconnectIfNeeded() {
  // 一定時間おきに再接続
  unsigned long now = millis();
  if (now - s_lastReconnectAttempt < RECONNECT_INTERVAL) {
    return false;
  }
  s_lastReconnectAttempt = now;

  Serial.println("[RobotBLEClient_ArduinoBLE] Reconnect attempt...");
  // 再度 connectToController する
  bool ok = connectToController(m_serviceUUID.c_str(), m_charUUID.c_str());
  if (ok) {
    Serial.println("[RobotBLEClient_ArduinoBLE] Reconnected!");
  } else {
    Serial.println("[RobotBLEClient_ArduinoBLE] Reconnect failed.");
  }
  return ok;
}
