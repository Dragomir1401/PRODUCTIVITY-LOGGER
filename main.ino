#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RtcDS1302.h>
#include <ThreeWire.h> 
#include <ezButton.h>

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

#define ADMIN_UID "53F7CA0E"

// Joystick pins
#define JOYSTICK_SW_PIN A0
#define JOYSTICK_URX_PIN A2
#define JOYSTICK_URY_PIN A1


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
ezButton joystickButton(JOYSTICK_SW_PIN); // Initialize the joystick button
int currentEmployeeIndex = 0; // To keep track of the currently displayed employee
bool detailMode = false; // Sets what type of mode the admin is in
bool employeeMode = false; // Sets what type of mode the admin is in
bool adminFlag = false;
bool updateDisplay = true;    // Flag to indicate when to update the display
int mainMenuIndex = 0; // To keep track of the current main menu option
int menuLevel = 0; // 0: Main Menu, 1: Employee List, 2: Employee Details

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

  // Set the correct date and time
  setDateTime();

  // Initialize joystick pins
  pinMode(JOYSTICK_URX_PIN, INPUT);
  pinMode(JOYSTICK_URY_PIN, INPUT);
  joystickButton.setDebounceTime(50); // Set debounce time for joystick button

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
  if (adminFlag) {
    adminLogged();
  }

  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID:");
      String readUID = convertUID(mfrc522);
      Serial.println(readUID);

      if (readUID == ADMIN_UID) {
        if (!adminFlag) {
          Serial.println("Admin Access Granted");
          lcd.clear();
          lcd.print("Admin Access");
          lcd.setCursor(0, 1);
          lcd.print("Granted");
          adminAccessMelody();
          adminFlag = true;
          delay(2000);
        } else {
          Serial.println("Admin Exited");
          lcd.clear();
          lcd.print("Admin Exited");
          adminGoodbyeMelody();
          adminFlag = false;
          delay(2000);
        }

        turnOffLEDs();
        return;
      }

      if (adminFlag)
      {
        return;
      }

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

  if (adminFlag)
  {
    return;
  }
  printIdle();
}


void adminLogged() {
  static int detailIndex = 0;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 300;
  const unsigned long navigationDelay = 1000; // 1-second delay between changes

  // Adjust the range of the joystick
  int joyX = analogRead(JOYSTICK_URY_PIN);
  int joyY = analogRead(JOYSTICK_URX_PIN);

  if (joyX < 200 && (millis() - lastDebounceTime > navigationDelay)) {
    if (menuLevel == 0) {
      // Move left in main menu
      mainMenuIndex = (mainMenuIndex > 0) ? mainMenuIndex - 1 : 3;
    } else if (menuLevel == 1) {
      // Move left in employee list
      currentEmployeeIndex = (currentEmployeeIndex > 0) ? currentEmployeeIndex - 1 : uidCount - 1;
    } else if (menuLevel == 2) {
      // Cycle left through employee details
      detailIndex = (detailIndex > 0) ? detailIndex - 1 : 2;
    }
    updateDisplay = true;
    lastDebounceTime = millis();
  } else if (joyX > 800 && (millis() - lastDebounceTime > navigationDelay)) {
    if (menuLevel == 0) {
      // Move right in main menu
      mainMenuIndex = (mainMenuIndex < 3) ? mainMenuIndex + 1 : 0;
    } else if (menuLevel == 1) {
      // Move right in employee list
      currentEmployeeIndex = (currentEmployeeIndex < uidCount - 1) ? currentEmployeeIndex + 1 : 0;
    } else if (menuLevel == 2) {
      // Cycle right through employee details
      detailIndex = (detailIndex < 2) ? detailIndex + 1 : 0;
    }
    updateDisplay = true;
    lastDebounceTime = millis();
  }

  if (joyY < 200 && (millis() - lastDebounceTime > debounceDelay)) {
    if (menuLevel == 0) {
      // Select option in main menu
      if (mainMenuIndex == 0) {
        menuLevel = 1; // Go to employee list
      } else if (mainMenuIndex == 1) {
        addCardAccess();
      } else if (mainMenuIndex == 2) {
        removeCardAccess();
      } else if (mainMenuIndex == 3) {
        showTotalNumber();
      }
    } else if (menuLevel == 1) {
      // Go to employee details
      menuLevel = 2;
    }
    updateDisplay = true;
    lastDebounceTime = millis();
  } else if (joyY > 800 && (millis() - lastDebounceTime > debounceDelay)) {
    if (menuLevel == 2) {
      // Go back to employee list
      menuLevel = 1;
    } else if (menuLevel == 1) {
      // Go back to main menu
      menuLevel = 0;
    }
    updateDisplay = true;
    lastDebounceTime = millis();
  }

  if (updateDisplay) {
    if (menuLevel == 0) {
      // Show main menu options
      lcd.clear();
      switch (mainMenuIndex) {
        case 0:
          lcd.print("See Employees");
          break;
        case 1:
          lcd.print("Add Card Access");
          break;
        case 2:
          lcd.print("Remove Card Access");
          break;
        case 3:
          lcd.print("Total Number");
          break;
      }
    } else if (menuLevel == 1) {
      // Show employee list
      lcd.clear();
      lcd.print("Employee ");
      lcd.print(currentEmployeeIndex + 1);
    } else if (menuLevel == 2) {
      // Show employee details
      lcd.clear();
      switch (detailIndex) {
        case 0:
          lcd.print("UID: ");
          lcd.print(authorizedUsers[currentEmployeeIndex].uid);
          break;
        case 1:
          lcd.print("Logged: ");
          lcd.print(authorizedUsers[currentEmployeeIndex].logged ? "Yes" : "No");
          break;
        case 2:
          lcd.print("Reminder: ");
          lcd.setCursor(0, 1);
          lcd.print(reminders[currentEmployeeIndex]);
          break;
      }
    }
    updateDisplay = false;
  }
}

void addCardAccess() {
  lcd.clear();
  lcd.print("Scan new card...");
  while (!mfrc522.PICC_IsNewCardPresent()) {
    int joyY = analogRead(JOYSTICK_URX_PIN);
    if (joyY > 800) {
      // Exit add card mode
      lcd.clear();
      lcd.print("Exiting...");
      delay(1000);
      return;
    }
    delay(100); // Wait for a new card to be present
  }

  if (mfrc522.PICC_ReadCardSerial()) {
    String newUID = convertUID(mfrc522);
    authorizedUsers[uidCount].uid = newUID;
    authorizedUsers[uidCount].logged = false;
    logTimes[uidCount] = 0;
    lastTimeSpent[uidCount] = 0;
    reminders[uidCount] = "New User";
    uidCount++;
    lcd.clear();
    lcd.print("Card Added:");
    lcd.setCursor(0, 1);
    lcd.print(newUID);
    delay(2000);
  } else {
    lcd.clear();
    lcd.print("Card Read Error");
    delay(2000);
  }
}

void removeCardAccess() {
  lcd.clear();
  lcd.print("Scan card to remove");
  while (!mfrc522.PICC_IsNewCardPresent()) {
    int joyY = analogRead(JOYSTICK_URX_PIN);
    if (joyY > 800) {
      // Exit remove card mode
      lcd.clear();
      lcd.print("Exiting...");
      delay(1000);
      return;
    }
    delay(100); // Wait for a new card to be present
  }

  if (mfrc522.PICC_ReadCardSerial()) {
    String removeUID = convertUID(mfrc522);
    int index = uidToIndex(removeUID);
    if (index >= 0) {
      for (int i = index; i < uidCount - 1; i++) {
        authorizedUsers[i] = authorizedUsers[i + 1];
        logTimes[i] = logTimes[i + 1];
        lastTimeSpent[i] = lastTimeSpent[i + 1];
        reminders[i] = reminders[i + 1];
      }
      uidCount--;
      lcd.clear();
      lcd.print("Card Removed:");
      lcd.setCursor(0, 1);
      lcd.print(removeUID);
      delay(2000);
    } else {
      lcd.clear();
      lcd.print("Card Not Found");
      delay(2000);
    }
  } else {
    lcd.clear();
    lcd.print("Card Read Error");
    delay(2000);
  }
}

void showTotalNumber() {
  lcd.clear();
  lcd.print("Total Users: ");
  lcd.setCursor(0, 1);
  lcd.print(uidCount);
  while (true) {
    int joyY = analogRead(JOYSTICK_URX_PIN);
    if (joyY > 800) {
      // Exit total number mode
      lcd.clear();
      lcd.print("Exiting...");
      delay(1000);
      return;
    }
    delay(100);
  }
}
void setDateTime() {
  // Set the date and time to the current time
  // Format: (year, month, day, hour, minute, second)
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);
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
  digitalWrite(BLUE_PIN, HIGH); // Turn blue LED on

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

void adminGoodbyeMelody() {
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();
  // Turn the LED yellow
  analogWrite(RED_PIN, 255);  // Full brightness
  analogWrite(GREEN_PIN, 55); // Full brightness
  analogWrite(BLUE_PIN, 0);   // Off

  tone(BUZZER_PIN, NOTE_C5, 200); // G5
  delay(250);
  tone(BUZZER_PIN, NOTE_G5, 200); // E5
  delay(250);
  tone(BUZZER_PIN, NOTE_E5, 200); // C5
  delay(250);

  noTone(BUZZER_PIN); // Stop any tone
  turnOffLEDs();

  digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}

void adminAccessMelody()
{
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  turnOffLEDs();

  // Turn the LED yellow
  analogWrite(RED_PIN, 255);  // Full brightness
  analogWrite(GREEN_PIN, 55); // Full brightness
  analogWrite(BLUE_PIN, 0);   // Off
          
  tone(BUZZER_PIN, NOTE_E5, 250); // C5
  delay(350);
  tone(BUZZER_PIN, NOTE_C5, 250); // E5
  delay(350);

  noTone(BUZZER_PIN); // Stop any tone
  turnOffLEDs();

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
