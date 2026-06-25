#include <WiFi.h>
#include <WiFiUdp.h>

// --- Wifi Instellingen ---
const char* ssid     = "WiFi-name";
const char* password = "password";

// UDP Instellingen
WiFiUDP udp;
const unsigned int localPort = 4210; 
char packetBuffer[255]; 

// --- Pin Definities (Inclusief alle eerdere fixes) ---
const int LV_DIR1 = 33; const int LV_DIR2 = 25; const int LV_PWM  = 32; 
const int RV_DIR1 = 26; const int RV_DIR2 = 27; const int RV_PWM  = 14; 
const int LA_DIR1 = 18; const int LA_DIR2 = 19; const int LA_PWM  = 21; 
const int RA_DIR1 = 17; const int RA_DIR2 = 5;  const int RA_PWM  = 16; // Pin 16 Fix

// --- Snelheid, Tuning & Failsafe ---
int baseSpeed = 150; 
int kLV = 0; int kRV = 0; int kLA = 0; int kRA = 0; 
char currentMove = 'X'; 

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 300; // Strengere failsafe voor UDP (300ms)

void setup() {
  Serial.begin(115200);

  pinMode(LV_DIR1, OUTPUT); pinMode(LV_DIR2, OUTPUT); pinMode(LV_PWM, OUTPUT);
  pinMode(RV_DIR1, OUTPUT); pinMode(RV_DIR2, OUTPUT); pinMode(RV_PWM, OUTPUT);
  pinMode(LA_DIR1, OUTPUT); pinMode(LA_DIR2, OUTPUT); pinMode(LA_PWM, OUTPUT);
  pinMode(RA_DIR1, OUTPUT); pinMode(RA_DIR2, OUTPUT); pinMode(RA_PWM, OUTPUT);

  // Verbinden met Wifi
  WiFi.begin(ssid, password);
  Serial.print("Verbinden met WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Verbonden!");
  
  // Start UDP verbinding
  udp.begin(localPort);
  Serial.print("Luisteren op UDP poort: ");
  Serial.println(localPort);
  Serial.print("IP-adres van de auto: ");
  Serial.println(WiFi.localIP()); // Dit IP-adres heb je nodig voor je Python script!
zzzz
  lastCommandTime = millis();
}

void loop() {
  // Controleer op binnenkomende UDP pakketjes
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    
    String input = String(packetBuffer);
    parseUDPCommand(input);
  }

  // Failsafe handrem
  if (currentMove != 'X' && (millis() - lastCommandTime > TIMEOUT_MS)) {
    currentMove = 'X';
    applyMecanumMovement();
    Serial.println("UDP Failsafe: Geen pakketjes ontvangen -> STOP");
  }
}

// Verwerkt de snelle commando's
void parseUDPCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  lastCommandTime = millis(); // Reset timer

  // Als het een enkel rij-commando is (Z, S, A, E, Q, D, X)
  if (input.length() == 1) {
    char cmd = toupper(input[0]);
    if (String("ZSAEQDX").indexOf(cmd) >= 0) {
      currentMove = cmd;
      applyMecanumMovement();
      return;
    }
  }

  // Als het een Tweak commando is (bijv V200 of LA-15)
  String prefix = "";
  int valIndex = 0;
  if (input.startsWith("V") || input.startsWith("v")) { prefix = "V"; valIndex = 1; }
  else if (input.length() >= 3) { prefix = input.substring(0, 2); prefix.toUpperCase(); valIndex = 2; }

  int val = input.substring(valIndex).toInt();

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
  setMotor("LinksAchter",  LA_DIR2, LA_DIR1, LA_PWM, targetLA, kLA); // LA omgedraaid
  setMotor("RechtsAchter", RA_DIR1, RA_DIR2, RA_PWM, targetRA, kRA);
}

void setMotor(String naam, int dir1, int dir2, int pwmPin, int targetSpeed, int kValue) {
  if (targetSpeed == 0) {
    digitalWrite(dir1, LOW);  digitalWrite(dir2, LOW);  analogWrite(pwmPin, 0);
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