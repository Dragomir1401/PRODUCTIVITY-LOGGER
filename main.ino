#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define SS_PIN 10
#define RST_PIN 9
#define GREEN_PIN 6
#define RED_PIN 5
#define BLUE_PIN 7
#define BUZZER_PIN 8

#define NOTE_C3  131
#define NOTE_E3  165
#define NOTE_G3  196
#define NOTE_C5  523
#define NOTE_E5  659
#define NOTE_G5  784


MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD I2C address and display size

void setup() {
  Serial.begin(9600); // Start serial communication at 9600 baud.
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522
  lcd.init();         // Initialize the LCD
  lcd.backlight();    // Turn on the backlight
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // Ensure buzzer is off initially
  turnOffLEDs(); // Turn off all LEDs initially
  Serial.println("Approximate your card to the reader...");
}

void loop() {
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID:");
      String readUID = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        readUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
      }
      readUID.toUpperCase();
      Serial.println(readUID);

      // Compare the read UID with the stored UID
      if (readUID == "E3E40B2F") {
        Serial.println("Access Granted");
        lcd.clear();
        lcd.print("Access Granted");
        accessGrantedMelody();
      } else {
        Serial.println("Access Denied");
        lcd.clear();
        lcd.print("Access Denied");
        accessDeniedMelody();
      }
      turnOffLEDs();
      lcd.clear();
      lcd.print("Scan Card");
    }
  }
}

void turnOffLEDs() {
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

void accessGrantedMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  digitalWrite(GREEN_PIN, HIGH); // Turn green LED on
  for(uint8_t nLoop = 0; nLoop < 1; nLoop++) {
    tone(BUZZER_PIN, NOTE_C5, 200);
    delay(320);
    tone(BUZZER_PIN, NOTE_E5, 200);
    delay(320);
    tone(BUZZER_PIN, NOTE_G5, 200);
    delay(270);
    tone(BUZZER_PIN, NOTE_E5, 200);
    delay(270);
    tone(BUZZER_PIN, NOTE_C5, 200);
    delay(230);
  }
  noTone(BUZZER_PIN); // Stop any tone
  digitalWrite(GREEN_PIN, LOW); // Turn off LED after melody
  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}


void accessDeniedMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  digitalWrite(RED_PIN, HIGH); // Turn red LED on
  tone(BUZZER_PIN, NOTE_G3, 300); // Lower tone G3
  delay(420);
  tone(BUZZER_PIN, NOTE_E3, 300); // Lower tone E3
  delay(420);
  tone(BUZZER_PIN, NOTE_C3, 300); // Lowest tone C3
  delay(420);
  noTone(BUZZER_PIN); // Stop any tone
  digitalWrite(RED_PIN, LOW); // Turn off LED after melody
  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}
