#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// --- Netwerkinstellingen ---
const char* ssid     = "WiFi-Name";
const char* password = "password";

WiFiUDP udp;
const unsigned int localPort = 4210;
char packetBuffer[255]; 

// --- Pin Definities Motoren ---
const int LV_DIR1 = 33; const int LV_DIR2 = 25; const int LV_PWM  = 32; // LinksVoor
const int RV_DIR1 = 26; const int RV_DIR2 = 27; const int RV_PWM  = 14; // RechtsVoor
const int LA_DIR1 = 18; const int LA_DIR2 = 19; const int LA_PWM  = 21; // LinksAchter
const int RA_DIR1 = 17; const int RA_DIR2 = 5;  const int RA_PWM  = 16; // RechtsAchter (Pin 16 Fix)

// --- Pin Definities Servos ---
Servo myservoX;
Servo myservoY;
const int servoPinX = 4;
const int servoPinY = 2;

// --- Variabelen voor Snelheid en Tuning Motoren ---
int baseSpeed = 150; 
int kLV = 0; int kRV = 0; int kLA = 0; int kRA = 0; 
char currentMove = 'X'; 

// --- Variabelen voor Servos ---
int hoofdwaardeX = 0;
int hoofdwaardeY = 0;
int posX = 0;
int posY = 0;
int kX = 0; // Kalibratie X-as
int kY = 0; // Kalibratie Y-as

// --- Failsafe ---
unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 300; 

// --- DEELFUNCTIES ---

void verbindWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Verbinden met WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi verbonden!");
  Serial.print("ESP32 IP-adres: ");
  Serial.println(WiFi.localIP());
}

void setupServos() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  myservoX.setPeriodHertz(50); 
  myservoY.setPeriodHertz(50); 
  
  myservoX.attach(servoPinX, 500, 2400);
  myservoY.attach(servoPinY, 500, 2400);
}

void stuurServos() {
  int basisX = map(hoofdwaardeX, 0, 1920, 135, 45); 
  int basisY = map(hoofdwaardeY, 1200, 0, 60, 120); 
  
  posX = constrain(basisX + kX, 0, 180);
  posY = constrain(basisY + kY, 0, 180);
  
  myservoX.write(posX);  
  myservoY.write(posY);  
}

void parseUDPCommand(String data) {
  // Controleer eerst of het een muis-coördinaatpakket is voor de servo's
  int xIndex = data.indexOf("X:");
  int yIndex = data.indexOf(",Y:");
  
  if (xIndex != -1 && yIndex != -1) {
    String xString = data.substring(xIndex + 2, yIndex);
    String yString = data.substring(yIndex + 3);
    
    hoofdwaardeX = xString.toInt();
    hoofdwaardeY = yString.toInt();
    stuurServos();
    return; // Stop hier, dit heeft geen invloed op het rijden
  }

  // Als het geen muisdata is, is het een motor/rij commando -> Reset de rij-failsafe
  lastCommandTime = millis();

  // Enkel rij-commando (Z, S, A, E, Q, D, X)
  if (data.length() == 1) {
    char cmd = toupper(data[0]);
    if (String("ZSAEQDX").indexOf(cmd) >= 0) {
      currentMove = cmd;
      applyMecanumMovement();
      return;
    }
  }

  // Motor Tweak commando's (bijv V200, LA-15) via UDP
  String prefix = "";
  int valIndex = 0;
  if (data.startsWith("V") || data.startsWith("v")) { prefix = "V"; valIndex = 1; }
  else if (data.length() >= 3) { prefix = data.substring(0, 2); prefix.toUpperCase(); valIndex = 2; }

  int val = data.substring(valIndex).toInt();

  if (prefix == "V")  baseSpeed = constrain(val, 0, 255);
  else if (prefix == "LV") kLV = val;
  else if (prefix == "RV") kRV = val;
  else if (prefix == "LA") kLA = val;
  else if (prefix == "RA") kRA = val;

  applyMecanumMovement();
}

void applyMecanumMovement() {
  int targetLV = 0, targetRV = 0, targetLA = 0, targetRA = 0;

  switch (currentMove) {
    case 'Z': // Vooruit
      targetLV = baseSpeed;  targetRV = baseSpeed;  targetLA = baseSpeed;  targetRA = baseSpeed;
      break;
    case 'S': // Achteruit
      targetLV = -baseSpeed; targetRV = -baseSpeed; targetLA = -baseSpeed; targetRA = -baseSpeed;
      break;
    case 'A': // Zijwaarts Links
      targetLV = -baseSpeed; targetRV = baseSpeed;  targetLA = baseSpeed;  targetRA = -baseSpeed;
      break;
    case 'E': // Zijwaarts Rechts
      targetLV = baseSpeed;  targetRV = -baseSpeed; targetLA = -baseSpeed; targetRA = baseSpeed;
      break;
    case 'Q': // Draaien Links
      targetLV = -baseSpeed; targetRV = baseSpeed;  targetLA = -baseSpeed; targetRA = baseSpeed;
      break;
    case 'D': // Draaien Rechts
      targetLV = baseSpeed;  targetRV = -baseSpeed; targetLA = baseSpeed;  targetRA = -baseSpeed;
      break;
    default: // STOP
      targetLV = 0; targetRV = 0; targetLA = 0; targetRA = 0;
      break;
  }

  setMotor("LinksVoor",    LV_DIR1, LV_DIR2, LV_PWM, targetLV, kLV);
  setMotor("RechtsVoor",   RV_DIR1, RV_DIR2, RV_PWM, targetRV, kRV);
  setMotor("LinksAchter",  LA_DIR2, LA_DIR1, LA_PWM, targetLA, kLA); // Richting omgedraaid
  setMotor("RechtsAchter", RA_DIR1, RA_DIR2, RA_PWM, targetRA, kRA);
}

void setMotor(String naam, int dir1, int dir2, int pwmPin, int targetSpeed, int kValue) {
  if (targetSpeed == 0) {
    digitalWrite(dir1, LOW); digitalWrite(dir2, LOW); analogWrite(pwmPin, 0);
    return;
  }
  int sign = (targetSpeed > 0) ? 1 : -1;
  int finalSpeed = abs(targetSpeed) + kValue;
  finalSpeed = constrain(finalSpeed, 0, 255);

  if (sign > 0) {
    digitalWrite(dir1, HIGH); digitalWrite(dir2, LOW);
  } else {
    digitalWrite(dir1, LOW);  digitalWrite(dir2, HIGH);
  }
  analogWrite(pwmPin, finalSpeed);
}

void verwerkSerieleMonitor() {
  if (Serial.available() > 0) {
    String invoer = Serial.readStringUntil('\n');
    invoer.trim();
    
    int xPos = invoer.indexOf('X');
    int yPos = invoer.indexOf('Y');
    
    if (xPos != -1) {
      kX = invoer.substring(xPos + 1).toInt();
      Serial.print("-> Offset kX aangepast! kX = "); Serial.println(kX);
    } 
    else if (yPos != -1) {
      kY = invoer.substring(yPos + 1).toInt();
      Serial.print("-> Offset kY aangepast! kY = "); Serial.println(kY);
    }
  }
}

// --- HOOFDPROGRAMMA ---

void setup() {
  Serial.begin(115200);
  
  pinMode(LV_DIR1, OUTPUT); pinMode(LV_DIR2, OUTPUT); pinMode(LV_PWM, OUTPUT);
  pinMode(RV_DIR1, OUTPUT); pinMode(RV_DIR2, OUTPUT); pinMode(RV_PWM, OUTPUT);
  pinMode(LA_DIR1, OUTPUT); pinMode(LA_DIR2, OUTPUT); pinMode(LA_PWM, OUTPUT);
  pinMode(RA_DIR1, OUTPUT); pinMode(RA_DIR2, OUTPUT); pinMode(RA_PWM, OUTPUT);

  setupServos();
  verbindWifi();
  
  udp.begin(localPort);
  lastCommandTime = millis();
  
  Serial.println("\n--- Mecanum & Servo Systeem Volledig Operationeel ---");
}

void loop() {
  // Controleer UDP Netwerk
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    parseUDPCommand(String(packetBuffer));
  }

  // Failsafe handrem voor de motoren
  if (currentMove != 'X' && (millis() - lastCommandTime > TIMEOUT_MS)) {
    currentMove = 'X';
    applyMecanumMovement();
    Serial.println("UDP Failsafe geactiveerd -> STOP");
  }
  
  verwerkSerieleMonitor();  
  delay(5); 
}