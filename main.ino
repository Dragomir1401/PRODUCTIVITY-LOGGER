#include <SPI.h>

#include <MFRC522.h>

#include <LiquidCrystal_I2C.h>

#include <Wire.h>

#include <RtcDS1302.h>

#include <ThreeWire.h>

// Pin defines
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

// User struct
struct user
{
    String uid;
    bool logged;
};

// MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// I2C LCD instance
LiquidCrystal_I2C lcd(0x27, 16, 2);

// User data
user authorizedUsers[MAX_UIDS];

// Time data
int logTimes[MAX_UIDS];
int lastTimeSpent[MAX_UIDS];
String lastAccess[MAX_UIDS];

// Reminder data
const char *reminders[MAX_UIDS];
const char *names[MAX_UIDS];

// Global counter
int uidCount = 0;
int currentEmployeeIndex = 0;

// IO, SCLK, CE
ThreeWire myWire(4, 3, 2);

// RTC instance
RtcDS1302<ThreeWire> Rtc(myWire);

// Menu flags
bool detailMode = false;   // Sets what type of mode the admin is in
bool adminFlag = false;    // Flag to indicate if admin is logged in
bool updateDisplay = true; // Flag to indicate when to update the display
int mainMenuIndex = 0;     // To keep track of the current main menu option
int menuLevel = 0;         // 0: Main Menu, 1: Employee List, 2: Employee Details

// Setup function to initialize the system
void setup()
{
    // Start serial communication at 9600 baud.
    Serial.begin(9600);

    // Initiate I2C bus
    Wire.begin();

    // Initiate SPI bus
    SPI.begin();

    // Initiate MFRC522
    mfrc522.PCD_Init();

    // Initiate RTC
    Rtc.Begin();

    // Initiate LCD
    lcd.init();

    // Turn on the backlight
    lcd.backlight();

    // Set the pins for the LEDs and buzzer
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(RED_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Ensure buzzer is off initially
    digitalWrite(BUZZER_PIN, HIGH);

    // Turn off all LEDs initially
    turnOffLEDs();

    // Print a message to the serial monitor
    Serial.println("Approximate your card to the reader...");

    // Set the correct date and time
    setDateTime();

    // Initialize joystick pins
    pinMode(JOYSTICK_URX_PIN, INPUT);
    pinMode(JOYSTICK_URY_PIN, INPUT);
    pinMode(JOYSTICK_SW_PIN, INPUT);

    // Set the reminders and names for employees
    reminders[0] = "Message manager for planning";
    names[0] = "Popescu Marius";
    reminders[1] = "Bugfix feature/IPD17-add-library";
    names[1] = "Gheorghe Hagi";
    reminders[2] = "Call HR for employment contract";
    names[2] = "Lionel Messi";
    reminders[3] = "Respond to product team emails";
    names[3] = "Nadia Marin";
}

// Main loop function
void loop()
{
    // Check if admin is logged in
    if (adminFlag)
    {
        adminLogged();
    }

    // Look for new cards
    if (mfrc522.PICC_IsNewCardPresent())
    {
        // Select one of the cards
        if (mfrc522.PICC_ReadCardSerial())
        {
            // Print the UID of the card
            Serial.print("Card UID:");
            String readUID = convertUID(mfrc522);
            Serial.println(readUID);

            // Check if the card is the admin card
            if (readUID == ADMIN_UID)
            {
                // If admin was not logged in, grant access
                if (!adminFlag)
                {
                    Serial.println("Admin Access Granted");
                    lcd.clear();
                    lcd.print("Admin Access");
                    lcd.setCursor(0, 1);
                    lcd.print("Granted");
                    adminAccessMelody();
                    adminFlag = true;
                    delay(2000);
                }
                // If admin was logged in, exit
                else
                {
                    Serial.println("Admin Exited");
                    lcd.clear();
                    lcd.print("Admin Exited");
                    adminGoodbyeMelody();
                    adminFlag = false;
                    delay(2000);
                }

                // Turn off LEDs after admin access
                turnOffLEDs();
                return;
            }

            // Don't allow other users to scan if admin is logged in
            if (adminFlag)
            {
                return;
            }

            // Compare the read UID with the stored UID
            if (isAuthorizedUID(readUID))
            {
                // Convert the UID to an index
                int index = uidToIndex(readUID);

                // Check if the user is logged in
                if (!authorizedUsers[index].logged)
                {
                    // Log the time of access
                    RtcDateTime now = Rtc.GetDateTime();
                    String nowString = timeToString(now);
                    lastAccess[index] = nowString;
                    Serial.println(lastAccess[index]);
                    authorizedUsers[index].logged = true;
                    logTimes[index] = dateToInt(now);

                    // Display the access granted message
                    Serial.println("Access Granted");
                    lcd.clear();
                    lcd.print("Access Granted");
                    lcd.setCursor(0, 1);
                    lcd.print(nowString);
                    accessGrantedMelody();
                    delay(1000);

                    // Display the welcome message
                    lcd.clear();
                    lcd.print("Welcome");
                    Serial.println("Welcome");
                    lcd.setCursor(0, 1);
                    lcd.print(names[uidToIndexMap(readUID)]);
                    Serial.println(names[uidToIndexMap(readUID)]);
                    delay(2000);

                    // Display the reminder message
                    lcd.clear();
                    lcd.print("Reminder");
                    Serial.println("Reminder");
                    delay(2000);

                    // Display the reminder message
                    printStringOnLCD(reminders[uidToIndexMap(readUID)]);
                    Serial.println(reminders[uidToIndexMap(readUID)]);
                    delay(2000);
                }
                else
                {
                    // Log the time of exit
                    RtcDateTime now = Rtc.GetDateTime();
                    String nowString = timeToString(now);
                    Serial.println(nowString);
                    authorizedUsers[index].logged = false;

                    // Display the log out messages
                    Serial.println("Logging out...");
                    lcd.clear();
                    lcd.print("Log out at ");
                    lcd.setCursor(0, 1);
                    lcd.print(nowString);
                    goodbyeMelody();
                    delay(2000);
                    displayExitTime(now, index);
                }
            }
            else
            {
                // Display the access denied message
                Serial.println("Access Denied");
                lcd.clear();
                lcd.print("Access Denied");
                accessDeniedMelody();
            }
            // Turn off LEDs after access
            turnOffLEDs();
        }
    }

    if (adminFlag)
    {
        // If admin is logged in, don't print idle message
        return;
    }

    // Print idle message
    printIdle();
}

// Function to print a message on the LCD
void printStringOnLCD(const char *message)
{
    // Number of characters per row
    int lcdWidth = 16;

    // Clear the LCD
    lcd.clear();

    // Print the first row
    for (int i = 0; i < lcdWidth && i < strlen(message); i++)
    {
        // Print char by char
        lcd.setCursor(i, 0);
        lcd.print(message[i]);
    }

    // Print the second row if the message is longer than the first row
    if (strlen(message) > lcdWidth)
    {
        for (int i = 0; i < lcdWidth && (lcdWidth + i) < strlen(message); i++)
        {
            // Print char by char
            lcd.setCursor(i, 1);
            lcd.print(message[lcdWidth + i]);
        }
    }
}

// Function to handle the admin logged in state
void adminLogged()
{
    // Index to iter through the index
    static int detailIndex = 0;
    // Time to debounce the joystick
    static unsigned long lastDebounceTime = 0;
    // Delay debouncer to prevent multiple readings
    const unsigned long debounceDelay = 300;
    // 1-second delay between changes
    const unsigned long navigationDelay = 1000;

    // Read the joystick values
    int joyX = analogRead(JOYSTICK_URY_PIN);
    int joyY = analogRead(JOYSTICK_URX_PIN);

    // Check if the joystick is moved
    if (joyX < 200 && (millis() - lastDebounceTime > navigationDelay))
    {
        if (menuLevel == 0)
        {
            // Move left in main menu
            mainMenuIndex = (mainMenuIndex > 0) ? mainMenuIndex - 1 : 3;
        }
        else if (menuLevel == 1)
        {
            // Move left in employee list
            currentEmployeeIndex = (currentEmployeeIndex > 0) ? currentEmployeeIndex - 1 : uidCount - 1;
        }
        else if (menuLevel == 2)
        {
            // Cycle left through employee details
            detailIndex = (detailIndex > 0) ? detailIndex - 1 : 4;
        }
        // Set the update flag
        updateDisplay = true;
        // Update the debounce time
        lastDebounceTime = millis();
    }
    else if (joyX > 800 && (millis() - lastDebounceTime > navigationDelay))
    {
        if (menuLevel == 0)
        {
            // Move right in main menu
            mainMenuIndex = (mainMenuIndex < 3) ? mainMenuIndex + 1 : 0;
        }
        else if (menuLevel == 1)
        {
            // Move right in employee list
            currentEmployeeIndex = (currentEmployeeIndex < uidCount - 1) ? currentEmployeeIndex + 1 : 0;
        }
        else if (menuLevel == 2)
        {
            // Cycle right through employee details
            detailIndex = (detailIndex < 4) ? detailIndex + 1 : 0;
        }
        // Set the update flag
        updateDisplay = true;
        // Update the debounce time
        lastDebounceTime = millis();
    }

    // Check if the joystick is pressed
    if (joyY < 200 && (millis() - lastDebounceTime > debounceDelay))
    {
        if (menuLevel == 0)
        {
            // Select option in main menu
            if (mainMenuIndex == 0)
            {
                // Go to employee list
                menuLevel = 1;
            }
            else if (mainMenuIndex == 1)
            {
                addCardAccess();
            }
            else if (mainMenuIndex == 2)
            {
                removeCardAccess();
            }
            else if (mainMenuIndex == 3)
            {
                showTotalNumber();
            }
        }
        else if (menuLevel == 1)
        {
            // Go to employee details
            menuLevel = 2;
        }
        // Set the update flag
        updateDisplay = true;
        // Update the debounce time
        lastDebounceTime = millis();
    }
    else if (joyY > 800 && (millis() - lastDebounceTime > debounceDelay))
    {
        if (menuLevel == 2)
        {
            // Go back to employee list
            menuLevel = 1;
        }
        else if (menuLevel == 1)
        {
            // Print the exit message
            lcd.clear();
            lcd.print("Exiting...");
            delay(1000);
            // Go back to main menu
            menuLevel = 0;
        }
        // Set the update flag
        updateDisplay = true;
        // Update the debounce time
        lastDebounceTime = millis();
    }

    // If the display needs to be updated
    if (updateDisplay)
    {
        // Check the menu level
        if (menuLevel == 0)
        {
            // Show main menu options
            lcd.clear();
            switch (mainMenuIndex)
            {
            case 0:
                lcd.print("See Employees");
                lcd.setCursor(3, 1);
                lcd.print("--page 1--");
                break;
            case 1:
                lcd.print("Add Access");
                lcd.setCursor(3, 1);
                lcd.print("--page 2--");
                break;
            case 2:
                lcd.print("Remove Access");
                lcd.setCursor(3, 1);
                lcd.print("--page 3--");
                break;
            case 3:
                lcd.print("Total Number");
                lcd.setCursor(3, 1);
                lcd.print("--page 4--");
                break;
            default:
                lcd.print("Default");
                break;
            }
        }
        else if (uidCount == 0)
        {
            // Show no employees message
            lcd.clear();
            lcd.print("No Employees");
            menuLevel = 1;
        }
        else if (menuLevel == 1)
        {
            // Show employee name from list
            lcd.clear();
            lcd.print(names[uidToIndexMap(authorizedUsers[currentEmployeeIndex].uid)]);
            lcd.setCursor(3, 1);
            lcd.print("--page ");
            lcd.print(currentEmployeeIndex + 1);
            lcd.print("--");
        }
        else if (menuLevel == 2)
        {
            // Show employee details
            lcd.clear();
            switch (detailIndex)
            {
            case 0:
                lcd.print("UID: ");
                lcd.print(authorizedUsers[currentEmployeeIndex].uid);
                break;
            case 1:
                lcd.print("Logged: ");
                lcd.print(authorizedUsers[currentEmployeeIndex].logged ? "Yes" : "No");
                break;
            case 2:
                lcd.print("Reminder:");
                lcd.setCursor(0, 1);
                lcd.print(reminders[currentEmployeeIndex]);
                break;
            case 3:
                lcd.print("Last access:");
                lcd.setCursor(0, 1);
                lcd.print(lastAccess[currentEmployeeIndex]);
                break;
            case 4:
                lcd.print("Last time spent:");
                lcd.setCursor(0, 1);
                String spentTimeString = formatSpentTime(lastTimeSpent[currentEmployeeIndex]);
                lcd.print(spentTimeString);
                break;
            default:
                lcd.print("Default");
                break;
            }
        }
        // Reset the update flag
        updateDisplay = false;
    }
}

// Function to add card access
void addCardAccess()
{
    lcd.clear();
    lcd.print("Scan new card...");
    // Wait for a new card to be present
    while (!mfrc522.PICC_IsNewCardPresent())
    {
        // Check if the joystick goes to exit
        int joyY = analogRead(JOYSTICK_URX_PIN);
        if (joyY > 800)
        {
            // Exit add card mode
            lcd.clear();
            lcd.print("Exiting...");
            delay(1000);
            return;
        }

        // Wait for a new card to be present
        delay(100);
    }

    // Read the card serial
    if (mfrc522.PICC_ReadCardSerial())
    {
        // Convert the UID to a string
        String newUID = convertUID(mfrc522);
        // Get the uid associated index
        int target = uidToIndex(newUID);
        if (target >= 0)
        {
            // Check if the user already exists
            for (int index = 0; index < uidCount; index++)
            {
                if (index == target)
                {
                    lcd.clear();
                    lcd.print("User Already");
                    lcd.setCursor(0, 1);
                    lcd.print("Exists");
                    delay(2000);
                    return;
                }
            }
        }

        // Add the new card to the list
        authorizedUsers[uidCount].uid = newUID;
        authorizedUsers[uidCount].logged = false;
        logTimes[uidCount] = 0;
        lastTimeSpent[uidCount] = 0;
        lastAccess[uidCount] = "No Access";
        uidCount++;

        // Print the card added message
        lcd.clear();
        lcd.print("Card Added:");
        lcd.setCursor(0, 1);
        lcd.print(newUID);
        delay(2000);
    }
    else
    {
        // Print the card read error message
        lcd.clear();
        lcd.print("Card Read Error");
        delay(2000);
    }
}

// Function to remove card access
void removeCardAccess()
{
    // Print the remove card message
    lcd.clear();
    lcd.print("Scan card to");
    lcd.setCursor(0, 1);
    lcd.print("Remove");

    // Wait for a new card to be present
    while (!mfrc522.PICC_IsNewCardPresent())
    {
        // Check if the joystick goes to exit
        int joyY = analogRead(JOYSTICK_URX_PIN);
        if (joyY > 800)
        {
            // Exit remove card mode
            lcd.clear();
            lcd.print("Exiting...");
            delay(1000);
            return;
        }
        // Wait for a new card to be present
        delay(100);
    }

    // Read the card serial
    if (mfrc522.PICC_ReadCardSerial())
    {
        String removeUID = convertUID(mfrc522);
        // Get the uid associated index
        int index = uidToIndex(removeUID);
        if (index >= 0)
        {
            // Remove the card from the list for shifting
            for (int i = index; i < uidCount - 1; i++)
            {
                authorizedUsers[i] = authorizedUsers[i + 1];
                logTimes[i] = logTimes[i + 1];
                lastTimeSpent[i] = lastTimeSpent[i + 1];
                reminders[i] = reminders[i + 1];
            }
            uidCount--;

            // Print the card removed message
            lcd.clear();
            lcd.print("Card Removed:");
            lcd.setCursor(0, 1);
            lcd.print(removeUID);
            delay(2000);
        }
        else
        {
            // Print the user not found message
            lcd.clear();
            lcd.print("User Not Found");
            delay(2000);
        }
    }
    else
    {
        // Print the card read error message
        lcd.clear();
        lcd.print("Card Read Error");
        delay(2000);
    }
}

// Function to show the total number of users
void showTotalNumber()
{
    // Print the total number of users
    lcd.clear();
    lcd.print("Total Users: ");
    lcd.setCursor(0, 1);
    lcd.print(uidCount);

    // Wait for the joystick to exit
    while (true)
    {
        int joyY = analogRead(JOYSTICK_URX_PIN);
        if (joyY > 800)
        {
            // Exit total number mode
            lcd.clear();
            lcd.print("Exiting...");
            delay(1000);
            return;
        }
        delay(100);
    }
}

// Function to set the date and time
void setDateTime()
{
    // Set the date and time to the current time
    // Format: (year, month, day, hour, minute, second)
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    Rtc.SetDateTime(compiled);
}

// Function to display the exit time
void displayExitTime(RtcDateTime now, int index)
{
    // Get the current time
    int seconds = dateToInt(now);
    int spentTime = seconds - logTimes[index];

    // Convert the spent time to a string
    String spentTimeString = formatSpentTime(spentTime);
    float difference = calculateTimeSpentPercentage(lastTimeSpent[index], spentTime);

    // Set last time spent to the current time spent
    lastTimeSpent[index] = spentTime;

    // Print the spent time
    lcd.clear();
    lcd.print("Spent time ");
    lcd.setCursor(0, 1);
    lcd.print(spentTimeString);
    delay(3000);

    // Print the difference in time spent from last time
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
    delay(4000);
}

// Function to print the idle message
void printIdle()
{
    // Static variables to keep track of the last toggle time and the message to show
    static unsigned long lastToggleTime = 0;
    static bool showScanMessage = true;

    // Cycle the scan card message with the total number of users after 4 seconds
    if (millis() - lastToggleTime >= 4000)
    {
        // Set the last toggle time
        lastToggleTime = millis();
        showScanMessage = !showScanMessage;

        lcd.clear();
        // Print the scan card message or the total number of users
        if (showScanMessage)
        {
            lcd.print("Scan Card");
        }
        else
        {
            int loggedUsersCount = 0;
            for (int i = 0; i < uidCount; i++)
            {
                if (authorizedUsers[i].logged)
                {
                    loggedUsersCount++;
                }
            }
            lcd.print("Users logged: ");
            lcd.print(loggedUsersCount);
        }
    }
}

// Function to calculate the percentage change in time spent
float calculateTimeSpentPercentage(int lastTimeSpent, int currentTimeSpent)
{
    // If last time spent is zero
    if (lastTimeSpent == 0)
    {
        if (currentTimeSpent == 0)
        {
            // No change if both are zero
            return 0.0;
        }
        else
        {
            // If lastTimeSpent is zero and currentTimeSpent is not zero, it's 100% more
            return 100.0;
        }
    }

    // Calculate the percentage change and return it
    float percentageChange = ((float)(currentTimeSpent - lastTimeSpent) / lastTimeSpent) * 100.0;
    return percentageChange;
}

// Function to format the spent time
String formatSpentTime(int totalSeconds)
{
    // Calculate the hours, minutes, and seconds
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    char timeString[20];

    // Format the time string and return it
    snprintf(timeString, sizeof(timeString), "%02dh:%02dm:%02ds", hours, minutes, seconds);
    return String(timeString);
}

// Function to read the time from the RTC
RtcDateTime readQuartzTime()
{
    // Read the time from the RTC
    RtcDateTime now = Rtc.GetDateTime();

    // Check if the time is valid
    if (!now.IsValid())
    {
        Serial.println("RTC lost confidence in the DateTime!");
    }

    return now;
}

// Function to calculate the number of days in a month
int daysInMonth(int month, int year)
{
    // Check the month and return the number of days
    switch (month)
    {
    case 1:
        return 31;
    case 2:
        return 28;
    case 3:
        return 31;
    case 4:
        return 30;
    case 5:
        return 31;
    case 6:
        return 30;
    case 7:
        return 31;
    case 8:
        return 31;
    case 9:
        return 30;
    case 10:
        return 31;
    case 11:
        return 30;
    case 12:
        return 31;
    default:
        return 0;
    }
}

// Function to convert a date to an integer
int dateToInt(const RtcDateTime &dt)
{
    // Define the Unix epoch
    const int EPOCH_YEAR = 1970;

    // Calculate the number of seconds in each component
    int seconds = 0;

    // Calculate months
    for (int month = 1; month < dt.Month(); month++)
    {
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

// Convert a time to a string
String timeToString(const RtcDateTime &dt)
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
               dt.Second());

    return String(datestring);
}

// Function to convert a UID to a string
String convertUID(MFRC522 &mfrc522)
{
    // Read the UID from the card
    String readUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        // Convert the UID to a string
        readUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
    }
    readUID.toUpperCase();

    return readUID;
}

// Function to convert a UID to an index based on authorized users
int uidToIndex(String uid)
{
    for (int i = 0; i < uidCount; i++)
    {
        if (authorizedUsers[i].uid == uid)
        {
            return i;
        }
    }

    return -1;
}

// Function to map an UID to an index based on card users
int uidToIndexMap(String uid)
{
    if (uid == "E3E40B2F")
    {
        return 0;
    }
    else if (uid == "E37A082F")
    {
        return 1;
    }
    else if (uid == "42487441")
    {
        return 2;
    }
    else if (uid == "53F7CA0E")
    {
        return 3;
    }
    else
    {
        return -1;
    }
}

// Function to check if a UID is authorized
bool isAuthorizedUID(String uid)
{
    for (int i = 0; i < uidCount; i++)
    {
        if (authorizedUsers[i].uid == uid)
        {
            return true;
        }
    }
    return false;
}

// Function to turn off all LEDs
void turnOffLEDs()
{
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
}

// Function to play the goodbye melody
void goodbyeMelody()
{
    // Ensure buzzer and LEDs is off initially
    digitalWrite(BUZZER_PIN, LOW);
    turnOffLEDs();

    // Turn the LED blue
    digitalWrite(BLUE_PIN, HIGH);

    // Play the melody
    tone(BUZZER_PIN, NOTE_G5, 200);
    delay(250);
    tone(BUZZER_PIN, NOTE_E5, 200);
    delay(250);
    tone(BUZZER_PIN, NOTE_C5, 200);
    delay(250);

    // Stop any tone
    noTone(BUZZER_PIN);

    // Turn off LED after melody
    digitalWrite(BLUE_PIN, LOW);

    // Explicitly turn off buzzer after use
    digitalWrite(BUZZER_PIN, HIGH);
}

// Function to play the admin goodbye melody
void adminGoodbyeMelody()
{
    // Ensure buzzer is off initially
    digitalWrite(BUZZER_PIN, LOW);

    // Turn the LED blue
    turnOffLEDs();

    // Turn the LED yellow
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 55);
    analogWrite(BLUE_PIN, 0);

    // Play the melody
    tone(BUZZER_PIN, NOTE_C5, 200);
    delay(250);
    tone(BUZZER_PIN, NOTE_G5, 200);
    delay(250);
    tone(BUZZER_PIN, NOTE_E5, 200);
    delay(250);

    // Stop any tone
    noTone(BUZZER_PIN);

    // Turn off LED after melody
    turnOffLEDs();

    // Explicitly turn off buzzer after use
    digitalWrite(BUZZER_PIN, HIGH);
}

// Function to play the admin access melody
void adminAccessMelody()
{
    // Ensure buzzer is off initially
    digitalWrite(BUZZER_PIN, LOW);

    // Turn the LED yellow
    turnOffLEDs();

    // Turn the LED yellow
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 55);
    analogWrite(BLUE_PIN, 0);

    // Play the melody
    tone(BUZZER_PIN, NOTE_E5, 250);
    delay(350);
    tone(BUZZER_PIN, NOTE_C5, 250);
    delay(350);

    // Stop any tone
    noTone(BUZZER_PIN);

    // Turn off LED after melody
    turnOffLEDs();

    // Explicitly turn off buzzer after use
    digitalWrite(BUZZER_PIN, HIGH);
}

// Function to play the access granted melody
void accessGrantedMelody()
{
    // Ensure buzzer is off initially
    digitalWrite(BUZZER_PIN, LOW);
    turnOffLEDs();
    digitalWrite(GREEN_PIN, HIGH); // Turn green LED on

    tone(BUZZER_PIN, NOTE_C5, 250); // C5
    delay(350);
    tone(BUZZER_PIN, NOTE_E5, 250); // E5
    delay(350);

    noTone(BUZZER_PIN);             // Stop any tone
    digitalWrite(GREEN_PIN, LOW);   // Turn off LED after melody
    digitalWrite(BUZZER_PIN, HIGH); // Explicitly turn off buzzer after use
}

// Function to play the access denied melody
void accessDeniedMelody()
{
    // Ensure buzzer is off initially
    digitalWrite(BUZZER_PIN, LOW);

    // Turn the LED red
    turnOffLEDs();

    // Turn the LED red
    digitalWrite(RED_PIN, HIGH);

    // Play the melody
    tone(BUZZER_PIN, NOTE_G4, 150);
    delay(150);
    tone(BUZZER_PIN, NOTE_C4, 150);
    delay(150);

    // Stop any tone
    noTone(BUZZER_PIN);

    // Turn off LED after melody
    digitalWrite(RED_PIN, LOW);

    // Explicitly turn off buzzer after use
    digitalWrite(BUZZER_PIN, HIGH);
}
