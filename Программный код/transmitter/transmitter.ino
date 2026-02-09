#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Пины подключения
#define CE_PIN   10
#define CSN_PIN  9

// Адреса радиоканалов
const uint64_t LISTENING_PIPE = 0xF0F0F0F0E1LL;  // Для отправки команд
const uint64_t BROADCAST_PIPE = 0xF0F0F0F0D2LL;  // Для приема данных

// Команды
#define CMD_START_SCAN   "START_SCAN"
#define CMD_STOP_SCAN    "STOP_SCAN"
#define CMD_STATUS       "STATUS"

// Структура для приема данных
struct DataPacket {
  float horizAngle;
  float vertAngle;
  uint8_t scanMode;
  uint32_t timestamp;
};

RF24 radio(CE_PIN, CSN_PIN);
DataPacket receivedData;

void setup() {
  Serial.begin(9600);
  
  Serial.println(F("=== ПУЛЬТ УПРАВЛЕНИЯ СИСТЕМОЙ НАВЕДЕНИЯ ==="));
  
  // Инициализация радиомодуля
  if (!radio.begin()) {
    Serial.println(F("Ошибка инициализации NRF24!"));
    while (1);
  }
  
  radio.openWritingPipe(LISTENING_PIPE);
  radio.openReadingPipe(1, BROADCAST_PIPE);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  
  Serial.println(F("Доступные команды:"));
  Serial.println(F("  1 - Начать сканирование"));
  Serial.println(F("  2 - Остановить сканирование"));
  Serial.println(F("  3 - Запросить статус"));
  Serial.println(F("  d - Включить/выключить отладку"));
  Serial.println(F("---------------------------------------"));
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd);
  }
  if (radio.available()) {
    radio.read(&receivedData, sizeof(receivedData));
    displayReceivedData();
  }
}

// Обработка команд
void processCommand(char cmd) {
  switch (cmd) {
    case '1':
      sendCommand(CMD_START_SCAN);
      Serial.println(F("Команда: Начать сканирование"));
      break;
      
    case '2':
      sendCommand(CMD_STOP_SCAN);
      Serial.println(F("Команда: Остановить сканирование"));
      break;
      
    case '3':
      sendCommand(CMD_STATUS);
      Serial.println(F("Команда: Запросить статус"));
      break;
      
    case 'd':
    case 'D':
      toggleDebug();
      break;
      
    case '\n':
    case '\r':
      break;
      
    default:
      Serial.println(F("Неизвестная команда"));
      break;
  }
}

// Отправка команды
void sendCommand(const char* command) {
  radio.stopListening();
  radio.openWritingPipe(LISTENING_PIPE);
  
  if (radio.write(command, strlen(command) + 1)) {
    Serial.print(F("Команда отправлена: "));
    Serial.println(command);
  } else {
    Serial.println(F("Ошибка отправки команды!"));
  }
  
  radio.startListening();
}

// Отображение полученных данных
void displayReceivedData() {
  Serial.print(F("\n[ДАННЫЕ] Время: "));
  Serial.print(receivedData.timestamp);
  Serial.print(F(" мс | Углы: H="));
  Serial.print(receivedData.horizAngle);
  Serial.print(F("°, V="));
  Serial.print(receivedData.vertAngle);
  Serial.print(F("° | Режим: "));
  
  switch (receivedData.scanMode) {
    case 0: Serial.print(F("ОЖИДАНИЕ")); break;
    case 1: Serial.print(F("ГОРИЗОНТАЛЬНЫЙ")); break;
    case 2: Serial.print(F("ВЕРТИКАЛЬНЫЙ")); break;
    case 3: Serial.print(F("ДИАГОНАЛЬНЫЙ 1")); break;
    case 4: Serial.print(F("ДИАГОНАЛЬНЫЙ 2")); break;
    case 5: Serial.print(F("ЗАВЕРШЕНО")); break;
    default: Serial.print(F("НЕИЗВЕСТНО")); break;
  }
  
  Serial.println();
}

// Включение/выключение отладки
void toggleDebug() {
  static bool debugEnabled = false;
  debugEnabled = !debugEnabled;
  
  if (debugEnabled) {
    Serial.println(F("Режим отладки ВКЛЮЧЕН"));
  } else {
    Serial.println(F("Режим отладки ВЫКЛЮЧЕН"));
  }
}