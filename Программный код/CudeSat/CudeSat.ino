#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// Пины подключения
#define HORIZ_SERVO_PIN 5    // Горизонтальный сервопривод
#define VERT_SERVO_PIN  6    // Вертикальный сервопривод  
#define LASER_PIN       3    // Лазерный модуль
#define CE_PIN          9    // NRF24 CE
#define CSN_PIN         10   // NRF24 CSN

// Адреса радиоканалов
const uint64_t LISTENING_PIPE = 0xF0F0F0F0E1LL;
const uint64_t BROADCAST_PIPE = 0xF0F0F0F0D2LL;

// Команды
#define CMD_START_SCAN   "START_SCAN"
#define CMD_STOP_SCAN    "STOP_SCAN"
#define CMD_STATUS       "STATUS"

// Параметры сканирования
#define SCAN_DELAY_MS    3000  
#define ANGLE_MIN       -40 
#define ANGLE_MAX        40  
#define ANGLE_STEP       10  

// Режимы сканирования
enum ScanMode {
  MODE_IDLE,         
  MODE_HORIZONTAL,    
  MODE_VERTICAL,   
  MODE_DIAGONAL1,    
  MODE_DIAGONAL2,  
  MODE_COMPLETE 
};

// Структура для передачи данных
struct DataPacket {
  float horizAngle; 
  float vertAngle;   
  uint8_t scanMode;   
  uint32_t timestamp; 
};

// Объекты
RF24 radio(CE_PIN, CSN_PIN);
Servo horizServo;  
Servo vertServo;   

// Переменные состояния
ScanMode currentMode = MODE_IDLE;
int currentHorizAngle = 0;
int currentVertAngle = 0;
bool isScanning = false;
unsigned long lastPositionTime = 0;
int scanPosition = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== Система наведения ==="));
  
  initAllSystems();
  
  setInitialPosition();
  
  Serial.println(F("Система готова к работе"));
  Serial.println(F("Ожидание команд..."));
}

void loop() {
  if (!isScanning) {
    listenForCommands();
  } else {
    performScanning();
  }
}

// Инициализация всех систем
void initAllSystems() {
  Serial.println(F("Инициализация систем..."));
  
  Serial.print(F("Сервоприводы... "));
  horizServo.attach(HORIZ_SERVO_PIN);
  vertServo.attach(VERT_SERVO_PIN);
  Serial.println(F("OK"));
  
  Serial.print(F("Лазер... "));
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);
  Serial.println(F("OK"));
  
  Serial.print(F("Радиомодуль... "));
  if (!radio.begin()) {
    Serial.println(F("ОШИБКА!"));
    while (1) {
      Serial.println(F("NRF24 не обнаружен! Проверьте подключение."));
      delay(5000);
    }
  }
  
  radio.openReadingPipe(1, LISTENING_PIPE);
  radio.openWritingPipe(BROADCAST_PIPE);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.startListening();
  Serial.println(F("OK"));
  
  Serial.println(F("Все системы инициализированы"));
}

// Установка начального положения (0°, 0°)
void setInitialPosition() {
  Serial.println(F("Установка начального положения (0°, 0°)..."));
  
  currentHorizAngle = 0;
  currentVertAngle = 0;
  
  horizServo.write(mapAngleToServo(currentHorizAngle));
  vertServo.write(mapAngleToServo(currentVertAngle));
  
  delay(1000);
  
  Serial.println(F("Начальное положение установлено"));
}

// Преобразование угла в значение для сервопривода (0-180 градусов)
int mapAngleToServo(int angle) {
  return map(angle, ANGLE_MIN, ANGLE_MAX, 0, 180);
}

// Прослушивание радиоканала
void listenForCommands() {
  if (radio.available()) {
    char command[32] = "";
    radio.read(&command, sizeof(command));
    
    Serial.print(F("Получена команда: "));
    Serial.println(command);
    
    if (strcmp(command, CMD_START_SCAN) == 0) {
      startScanning();
    } else if (strcmp(command, CMD_STOP_SCAN) == 0) {
      stopScanning();
    } else if (strcmp(command, CMD_STATUS) == 0) {
      sendStatus();
    }
  }
}

// Начало сканирования
void startScanning() {
  Serial.println(F("\n=== НАЧАЛО СКАНИРОВАНИЯ ==="));
  
  isScanning = true;
  currentMode = MODE_HORIZONTAL;
  scanPosition = 0;
  lastPositionTime = millis();
  
  digitalWrite(LASER_PIN, HIGH);
  
  radio.stopListening();
  radio.openWritingPipe(BROADCAST_PIPE);
  
  Serial.println(F("Переход в режим вещания"));
}

// Остановка сканирования
void stopScanning() {
  if (isScanning) {
    Serial.println(F("\n=== ОСТАНОВКА СКАНИРОВАНИЯ ==="));
    
    isScanning = false;
    currentMode = MODE_IDLE;
    
    digitalWrite(LASER_PIN, LOW);
    
    radio.startListening();
    radio.openReadingPipe(1, LISTENING_PIPE);
    
    setInitialPosition();
    
    Serial.println(F("Сканирование остановлено"));
  }
}

// Отправка статуса
void sendStatus() {
  radio.stopListening();
  radio.openWritingPipe(BROADCAST_PIPE);
  
  DataPacket packet;
  packet.horizAngle = currentHorizAngle;
  packet.vertAngle = currentVertAngle;
  packet.scanMode = currentMode;
  packet.timestamp = millis();
  
  radio.write(&packet, sizeof(packet));
  
  radio.startListening();
  radio.openReadingPipe(1, LISTENING_PIPE);
  
  Serial.print(F("Статус отправлен: H="));
  Serial.print(currentHorizAngle);
  Serial.print(F("°, V="));
  Serial.print(currentVertAngle);
  Serial.print(F("°, Mode="));
  Serial.println(currentMode);
}

// Выполнение сканирования
void performScanning() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastPositionTime >= SCAN_DELAY_MS) {
    lastPositionTime = currentTime;
    
    setNextScanPosition();
    
    sendPositionData();
    
    checkModeTransition();
  }
}

// Установка следующей позиции сканирования
void setNextScanPosition() {
  switch (currentMode) {
    case MODE_HORIZONTAL:
      currentVertAngle = ANGLE_MIN + (scanPosition * ANGLE_STEP);
      currentHorizAngle = 0;
      break;
      
    case MODE_VERTICAL:
      currentHorizAngle = ANGLE_MIN + (scanPosition * ANGLE_STEP);
      currentVertAngle = 0;
      break;
      
    case MODE_DIAGONAL1:
      currentHorizAngle = ANGLE_MIN + (scanPosition * ANGLE_STEP);
      currentVertAngle = ANGLE_MIN + (scanPosition * ANGLE_STEP);
      break;
      
    case MODE_DIAGONAL2:
      currentHorizAngle = ANGLE_MIN + (scanPosition * ANGLE_STEP);
      currentVertAngle = ANGLE_MAX - (scanPosition * ANGLE_STEP);
      break;
      
    default:
      return;
  }
  
  horizServo.write(mapAngleToServo(currentHorizAngle));
  vertServo.write(mapAngleToServo(currentVertAngle));
  
  Serial.print(F("Позиция ["));
  Serial.print(getModeName(currentMode));
  Serial.print(F("]: H="));
  Serial.print(currentHorizAngle);
  Serial.print(F("°, V="));
  Serial.print(currentVertAngle);
  Serial.println(F("°"));
  
  scanPosition++;
}

// Отправка данных о позиции
void sendPositionData() {
  DataPacket packet;
  packet.horizAngle = currentHorizAngle;
  packet.vertAngle = currentVertAngle;
  packet.scanMode = currentMode;
  packet.timestamp = millis();
  
  if (!radio.write(&packet, sizeof(packet))) {
    Serial.println(F("Ошибка отправки данных!"));
  }
}

// Проверка перехода между режимами
void checkModeTransition() {
  int maxPositions = ((ANGLE_MAX - ANGLE_MIN) / ANGLE_STEP) + 1;
  
  if (scanPosition >= maxPositions) {
    scanPosition = 0;
    
    switch (currentMode) {
      case MODE_HORIZONTAL:
        currentMode = MODE_VERTICAL;
        Serial.println(F("\n=== ПЕРЕХОД К ВЕРТИКАЛЬНОМУ СКАНИРОВАНИЮ ==="));
        break;
        
      case MODE_VERTICAL:
        currentMode = MODE_DIAGONAL1;
        Serial.println(F("\n=== ПЕРЕХОД К ДИАГОНАЛЬНОМУ СКАНИРОВАНИЮ 1 ==="));
        break;
        
      case MODE_DIAGONAL1:
        currentMode = MODE_DIAGONAL2;
        Serial.println(F("\n=== ПЕРЕХОД К ДИАГОНАЛЬНОМУ СКАНИРОВАНИЮ 2 ==="));
        break;
        
      case MODE_DIAGONAL2:
        currentMode = MODE_COMPLETE;
        Serial.println(F("\n=== СКАНИРОВАНИЕ ЗАВЕРШЕНО ==="));
        stopScanning();
        break;
        
      default:
        break;
    }
  }
}

// Получение имени режима
String getModeName(ScanMode mode) {
  switch (mode) {
    case MODE_HORIZONTAL: return "HORIZ";
    case MODE_VERTICAL: return "VERT";
    case MODE_DIAGONAL1: return "DIAG1";
    case MODE_DIAGONAL2: return "DIAG2";
    default: return "UNKNOWN";
  }
}