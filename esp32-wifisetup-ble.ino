#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include "SPIFFS.h"
#include "Store.h"

// WiFI情報を保存するキー名
#define KEY_WIFI_SSID   "wifi_ssid"
#define KEY_WIFI_PSWD   "wifi_pswd"

// UUIDは新規取得しておいてください
// https://www.uuidgenerator.net/
#define SERVICE_UUID      "a0e23ef4-b17c-49c0-b1b8-1558ac876f7f"
#define CHAR_STAT_UUID    "500693cd-04f9-4bfe-baf9-dfe1bbbf2481"
#define CHAR_SSID_UUID    "5d0b8bdf-3d53-4550-a4d6-0b31ffa26ccb"
#define CHAR_PSWD_UUID    "3268ad71-7b6a-436a-bcdc-5b1a6d81a1c2"

// FSを継承したオブジェクトを引数に入れてStoreを立ち上げる
// ほかにもSDカードライブラリなどの利用が可能
// ESP32系ではSPIFFSが最も手軽で便利
Store store = Store(SPIFFS);



//=======================================================================
//                        WiFi接続開始用共通関数
//=======================================================================
void wifiConnect(const String ssid, const String pswd) {
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), pswd.c_str());
}



//=======================================================================
//                        SSID変更時のコールバック
//=======================================================================
class callbackSsid: public BLECharacteristicCallbacks {
  // 設定中のSSID読み取り
  void onRead(BLECharacteristic *pCharacteristic) {
    String value = store.get(KEY_WIFI_SSID);
    if (value.length() > 0) {
      pCharacteristic->setValue(value.c_str());
    } else {
      pCharacteristic->setValue("(none)");
    }
  }
  // SSID書き込み
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      log_i("SSID set to \"%s\"", value.c_str());
      String ssid = String(value.c_str());
      String pswd = store.get(KEY_WIFI_PSWD);
      store.set(KEY_WIFI_SSID, ssid);
      wifiConnect(ssid, pswd);
    }
  }
};



//=======================================================================
//                        パスワード変更時のコールバック
//=======================================================================
class callbackPswd: public BLECharacteristicCallbacks {
  // パスワード書き込み
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      log_i("Password set to \"%s\"", value.c_str());
      String ssid = store.get(KEY_WIFI_SSID);
      String pswd = String(value.c_str());
      store.set(KEY_WIFI_PSWD, pswd);
      wifiConnect(ssid, pswd);
    }
  }
};



//=======================================================================
//                        Setup
//=======================================================================
void setup() {
  // BLE初期化
  BLEDevice::init("ESP32-WiFiSetup-BLE");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // キャラクタリスティック:状態表示
  BLECharacteristic *pCharStat = pService->createCharacteristic(CHAR_STAT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  // キャラクタリスティック:SSID
  BLECharacteristic *pCharSsid = pService->createCharacteristic(CHAR_SSID_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharSsid->setCallbacks(new callbackSsid());
  // キャラクタリスティック:Password
  BLECharacteristic *pCharPswd = pService->createCharacteristic(CHAR_PSWD_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharPswd->setCallbacks(new callbackPswd());
  // BLE開始
  pService->start();

  // 初期化中を通知
  pCharStat->setValue("Initializing");
  pCharStat->notify();

  // アドバタイジング開始
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
  
  // SPIFFSを開始してからStoreの初期化処理を行う
  SPIFFS.begin(true);
  store.begin("/config_rw.txt");

  // WiFi:接続情報を読み出す
  String ssid = store.get(KEY_WIFI_SSID);
  String pswd = store.get(KEY_WIFI_PSWD);
  
  // WiFi:接続試行
  wifiConnect(ssid, pswd);
  // 状態変更待ち（5秒間）
  unsigned long now = millis();
  while (WiFi.status() != WL_CONNECTED && millis() <= (now+5000UL)) { delay(50); }

  // 接続状態に応じてNotifyする
  while (true) {
    if (ssid.equals("")) {
      // SSIDが設定されていない
      pCharStat->setValue("SSID empty");
      pCharStat->notify();
    } else if (WiFi.status() != WL_CONNECTED) {
      // SSIDが見つからない・パスワード違うなどで接続できない
      pCharStat->setValue("Connect fail");
      pCharStat->notify();
    } else {
      // 接続できたらIPを通知してループを抜ける
      pCharStat->setValue( WiFi.localIP().toString().c_str() );
      pCharStat->notify();
      break;
    }
    delay(50);
  }

  // これ以降はWiFi接続済み
  log_i("Connected to %s (%s)", store.get(KEY_WIFI_SSID), WiFi.localIP().toString().c_str());
}



//=======================================================================
//                        Loop
//=======================================================================
void loop() {
  // ここで既存WiFiの切断検知や別のWiFiに繋ぎ直した場合の状態検知は
  // pCharStatをグローバルで使えるようにする必要があります
  delay(100);
}
