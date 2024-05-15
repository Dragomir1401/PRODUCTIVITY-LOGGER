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

#define MAX_UIDS 10

struct user
{
  String uid;
  bool logged;
};

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD I2C address and display size
user authorizedUsers[MAX_UIDS];
int logTimes[MAX_UIDS];
int lastTimeSpent[MAX_UIDS];
String reminders[MAX_UIDS];
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

  authorizedUsers[uidCount].uid = "E3E40B2F";
  authorizedUsers[uidCount].logged = false;
  logTimes[uidCount] = 0;
  lastTimeSpent[uidCount] = 0;
  reminders[uidCount] = "Message manager";
  uidCount++;

  authorizedUsers[uidCount].uid = "E37A082F";
  authorizedUsers[uidCount].logged = false;
  logTimes[uidCount] = 0;
  lastTimeSpent[uidCount] = 0;
  reminders[uidCount] = "Bugfix feature/IPD17-add-library";
  uidCount++;
}

void loop() {
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID:");
      String readUID = convertUID(mfrc522);
      Serial.println(readUID);

      // Compare the read UID with the stored UID
      if (isAuthorizedUID(readUID)) {
        int index = uidToIndex(readUID);

        if (!authorizedUsers[index].logged) {
          RtcDateTime now = Rtc.GetDateTime();
          String nowString = timeToString(now);
          Serial.println(nowString);
          Serial.println();

          authorizedUsers[index].logged = true;
          logTimes[index] = dateToInt(now);

          Serial.println("Access Granted");
          lcd.clear();
          lcd.print("Access Granted");
          lcd.setCursor(0, 1);
          lcd.print(nowString);
          accessGrantedMelody();
          delay(1000); // Display log in for 2 seconds
          printStringOnLCD(reminders[index]);
          delay(5000);
        } else {
          RtcDateTime now = Rtc.GetDateTime();
          String nowString = timeToString(now);
          Serial.println(nowString);
          Serial.println();

          authorizedUsers[index].logged = false;
          Serial.println("Logging out...");
          lcd.clear();
          lcd.print("Log out at ");
          lcd.setCursor(0, 1);
          lcd.print(nowString);
          goodbyeMelody();
          delay(2000); // Display log out time for 2 seconds

          displayExitTime(now, index);
        }
      } else {
        Serial.println("Access Denied");
        lcd.clear();
        lcd.print("Access Denied");
        accessDeniedMelody();
      }

      turnOffLEDs();
    }
  }
  printIdle();
}

void printStringOnLCD(String message) {
  int lcdWidth = 16; // Number of characters per row
  int lcdHeight = 2; // Number of rows

  lcd.clear();

  int messageLength = message.length();

  // Print the first row
  for (int i = 0; i < lcdWidth && i < messageLength; i++) {
    lcd.setCursor(i, 0);
    lcd.print(message[i]);
  }

  // Print the second row if the message is longer than the first row
  if (messageLength > lcdWidth) {
    for (int i = 0; i < lcdWidth && (lcdWidth + i) < messageLength; i++) {
      lcd.setCursor(i, 1);
      lcd.print(message[lcdWidth + i]);
    }
  }
}

void displayExitTime(RtcDateTime now, int index)
{
  int seconds = dateToInt(now);
  int spentTime = seconds - logTimes[index];
  String spentTimeString = formatSpentTime(spentTime);
  float difference = calculateTimeSpentPercentage(lastTimeSpent[index], spentTime);
  lastTimeSpent[index] = spentTime;

  lcd.clear();
  lcd.print("Spent time ");
  lcd.setCursor(0, 1);
  lcd.print(spentTimeString);
  delay(3000); // Display spent time for 3 seconds

  lcd.clear();
  lcd.print("From last time ");
  lcd.setCursor(0, 1);
  lcd.print(difference);
  if (difference >= 0)
  {
    lcd.print("% increase");
  }
  else
  {
    lcd.print("% decrease");
  }
  delay(5000); // Display spent time for 3 seconds
}

void printIdle()
{
  static unsigned long lastToggleTime = 0;
  static bool showScanMessage = true;
  if (millis() - lastToggleTime >= 4000) {
    lastToggleTime = millis();
    showScanMessage = !showScanMessage;

    lcd.clear();
    if (showScanMessage) {
      lcd.print("Scan Card");
    } else {
      int loggedUsersCount = 0;
      for (int i = 0; i < uidCount; i++) {
        if (authorizedUsers[i].logged) {
          loggedUsersCount++;
        }
      }
      lcd.print("Users logged: ");
      lcd.print(loggedUsersCount);
    }
  }
}

float calculateTimeSpentPercentage(int lastTimeSpent, int currentTimeSpent) {
  if (lastTimeSpent == 0) {
    if (currentTimeSpent == 0) {
      return 0.0; // No change if both are zero
    } else {
      return 100.0; // If lastTimeSpent is zero and currentTimeSpent is not zero, it's 100% more
    }
  }

  float percentageChange = ((float)(currentTimeSpent - lastTimeSpent) / lastTimeSpent) * 100.0;
  return percentageChange;
}

String formatSpentTime(int totalSeconds) {
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    char timeString[20];
    snprintf(timeString, sizeof(timeString), "%02dh:%02dm:%02ds", hours, minutes, seconds);
    return String(timeString);
}

RtcDateTime readQuartzTime() {
  RtcDateTime now = Rtc.GetDateTime();

  if (!now.IsValid())
  {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }

  return now;
}

int daysInMonth(int month, int year) {
    switch (month) {
        case 1: return 31;
        case 2: return 28;
        case 3: return 31;
        case 4: return 30;
        case 5: return 31;
        case 6: return 30;
        case 7: return 31;
        case 8: return 31;
        case 9: return 30;
        case 10: return 31;
        case 11: return 30;
        case 12: return 31;
        default: return 0; // Invalid month
    }
}

int dateToInt(const RtcDateTime& dt) {
    // Define the Unix epoch (January 1, 1970)
    const int EPOCH_YEAR = 1970;
    
    // Calculate the number of seconds in each component
    int seconds = 0;

    // Calculate months
    for (int month = 1; month < dt.Month(); month++) {
        seconds += daysInMonth(month, dt.Year()) * 24 * 3600;
    }

    // Calculate days
    seconds += (dt.Day() - 1) * 24 * 3600;

    // Calculate hours, minutes, and seconds
    seconds += dt.Hour() * 3600;
    seconds += dt.Minute() * 60;
    seconds += dt.Second();

    return seconds;
}

String timeToString(const RtcDateTime& dt)
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

  return String(datestring);
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
