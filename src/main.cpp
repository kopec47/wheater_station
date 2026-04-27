#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define I2C_SDA 8
#define I2C_SCL 9

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID   "abcdefab-1234-5678-1234-56789abcdef0"

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  1800
#define BLE_TIMEOUT 60000

Adafruit_BME280 bme; //czujnik
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
unsigned long bootTime = 0;

void goToDeepSleep(){
  Serial.println("Przechodzenie do głębokiego snu...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush();
  esp_deep_sleep_start();

}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Klient połączony!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Klient rozłączony!");
      pServer-> getAdvertising()->start(); // Ponowne rozpoczęcie transmisji reklamowej po rozłączeniu klienta
      delay(500);
      goToDeepSleep();

    }
};

void setup(){
  Serial.begin(115200);
  bootTime = millis();
  
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
  if (wakeupReason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("wybudzenie z głębokiego snu po upływie czasu '30 s'.");
  } else {
    Serial.println("Wybudzenie urzadzenia przez przycisk 'RESET'");
  }


  Wire.begin(I2C_SDA, I2C_SCL); 
  if (!bme.begin(0x76, &Wire)) {
    Serial.println("Nie można znaleźć czujnika BME280!");
    delay(2000);
    goToDeepSleep();
  }

  BLEDevice::init("ESP32 Weather Station");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising ->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Funkcja zalecana do poprawy kompatybilności z urządzeniami iOS
  pAdvertising->setMinPreferred(0x12); // Funkcja zalecana do poprawy kompatybilności z urządzeniami iOS
  BLEDevice::startAdvertising();
  Serial.println("Oczekiwanie na połączenie klienta...");
}

void loop(){
  

  if(!deviceConnected && (millis() - bootTime > BLE_TIMEOUT)) {
    Serial.println("Brak połączenia z klientem BLE w ciągu 60 sekund. Przechodzenie do głębokiego snu...");
    goToDeepSleep();
  } 


  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F; // konwersja do hPa
  
  

  String dataString = String(temperature) + "," + String(humidity) + "," + String(pressure);


  if (deviceConnected) {
    pCharacteristic->setValue(dataString.c_str());
    pCharacteristic->notify();
    Serial.println("Wysłano dane do klienta BLE:");
    Serial.println(dataString);
  } else {
    Serial.println("Brak połączenia z klientem BLE. Dane nie zostały wysłane.");
  }

  Serial.println("Lokowanie: " + dataString);


  delay(2000); 
}