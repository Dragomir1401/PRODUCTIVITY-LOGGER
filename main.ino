#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RtcDS1302.h>
#include <ThreeWire.h> 

#define SS_PIN 10
#define RST_PIN 9
#define GREEN_PIN 6
#define RED_PIN 5
#define BLUE_PIN 7
#define BUZZER_PIN 8

#define NOTE_C4 261
#define NOTE_E4 329
#define NOTE_G4 392
#define NOTE_C5 523
#define NOTE_E5 659
#define NOTE_G5 784

#define MAX_UIDS 100

struct user
{
  String uid;
  bool logged;
};

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD I2C address and display size
user authorizedUsers[MAX_UIDS];
int uidCount = 0;
ThreeWire myWire(4,3,2); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Serial.begin(9600); // Start serial communication at 9600 baud.
  Wire.begin();
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522
  Rtc.Begin();
  lcd.init();         // Initialize the LCD
  lcd.backlight();    // Turn on the backlight
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // Ensure buzzer is off initially
  turnOffLEDs(); // Turn off all LEDs initially
  Serial.println("Approximate your card to the reader...");
  authorizedUsers[uidCount++].uid = "E3E40B2F";
  authorizedUsers[uidCount++].logged = false;
  authorizedUsers[uidCount++].uid = "E37A082F";
  authorizedUsers[uidCount++].logged = false;
}

void loop()
{
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID:");
      String readUID = convertUID(mfrc522);
      Serial.println(readUID);

      // Compare the read UID with the stored UID
      if (isAuthorizedUID(readUID)) {
        int index = uidToIndex(readUID);

        if (authorizedUsers[index].logged == false)
        {
          unsigned long time = readQuartzTime();

          authorizedUsers[index].logged = true;
          Serial.println("Access Granted");
          lcd.clear();
          lcd.print("Access Granted");
          accessGrantedMelody();
        }
        else
        {
          authorizedUsers[index].logged = false;
          Serial.println("Logging out...");
          lcd.clear();
          lcd.print("Logging out...");
          delay(3000);
          lcd.print("Logged out...");
          goodbyeMelody();
        }
      }
      else
      {
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

unsigned long readQuartzTime() {
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();

  if (!now.IsValid())
  {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }
}

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

String convertUID(MFRC522 &mfrc522)
{
  String readUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    readUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
  }
  readUID.toUpperCase();

  Serial.println(readUID);

  return readUID;
}

int uidToIndex(String uid)
{
  for (int i = 0; i < uidCount; i++)
  {
    if (authorizedUsers[i].uid == uid) 
    {
      return i;
    }
  }
}

bool isAuthorizedUID(String uid) {
  for (int i = 0; i < uidCount; i++) {
    if (authorizedUsers[i].uid == uid) {
      return true;
    }
  }
  return false;
}

void turnOffLEDs() {
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

void goodbyeMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  digitalWrite(BLUE_PIN, HIGH); // Turn blue LED on (assuming you use BLUE_PIN for indication)

  tone(BUZZER_PIN, NOTE_G5, 200); // G5
  delay(250);
  tone(BUZZER_PIN, NOTE_E5, 200); // E5
  delay(250);
  tone(BUZZER_PIN, NOTE_C5, 200); // C5
  delay(250);

  noTone(BUZZER_PIN); // Stop any tone
  digitalWrite(BLUE_PIN, LOW); // Turn off LED after melody
  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}

void accessGrantedMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  digitalWrite(GREEN_PIN, HIGH); // Turn green LED on

  tone(BUZZER_PIN, NOTE_C5, 250); // C5
  delay(350);
  tone(BUZZER_PIN, NOTE_E5, 250); // E5
  delay(350);

  noTone(BUZZER_PIN); // Stop any tone
  digitalWrite(GREEN_PIN, LOW); // Turn off LED after melody
  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}

void accessDeniedMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  digitalWrite(RED_PIN, HIGH); // Turn red LED on

  tone(BUZZER_PIN, NOTE_G4, 150); // G4
  delay(150);
  tone(BUZZER_PIN, NOTE_C4, 150); // C4
  delay(150);

  noTone(BUZZER_PIN); // Stop any tone
  digitalWrite(RED_PIN, LOW); // Turn off LED after melody
  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}
