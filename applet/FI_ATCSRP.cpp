/************************
 * FI-ATCSRP 
 * www.FisherInnovation.com
 * 2009 Matt Fisher
 *************************/

#include <LiquidCrystal.h>
#include <Servo.h>
#include <String.h>

/* User Defined Variables */
#include "WProgram.h"
void setup();
void loop();
void PrintToLCD(char* LineOne, char* LineTwo);
int ReadThermocouple();
void MoveServo(int Position, int DelayTime);
int CheckButtons();
void DisplayMenu();
void HaltProcess();
void RestartProcess();
void InitReflow();
void InitDesolder();
boolean SystemConfig();
void WaitForStableTemp();
void StartPreheat();
void StartReflowPrep();
void StartReflow();
void StartCooldown();
int ThermocouplePin = 0;                     // Thermcouple Pin
int ButtonOnePin = 6;                        // Button 1 Pin
int ButtonTwoPin = 7;                        // Button 2 Pin
int ServoPin = 9;                            // Servo Pin
int ServoLowPosition = 180;                  // Position the servo is in to have the heat at low or off
int ServoHighPosition = 0;                   // Position the servo is in to have the heat at high
int LCDBacklightPin = 13;                    // LCD Backlight Pin
LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);   // rw on pin 11, rs on pin 12, enable on pin 10, d4, d5, d6, d7 on pins 5, 4, 3, 2
/* End User Defined Variables */


Servo ControlServo;
int Temperature = 0;              // Current temperature
int LastTemperature = 0;          // The last temp reading
int LowestTemperature = 0;        // The lowest temperature we can achive with the board on. (this will be generated automatically).
int ServoPosition = 0;            // Servo position storage
int ButtonOneVal = 0;             // Button 1 state value
int ButtonTwoVal = 0;             // Button 2 state value
int PreviousButtonOneState = LOW;
int PreviousButtonTwoState = LOW;

long ButtonOneTime = 0;	          // Time since the last button one press
long ButtonTwoTime = 0;	          // Time since the last button two press
long DebounceTime = 500;          // The debounce time

boolean MenuDisplayed = false;
boolean ProcessStarted = false;   // If we are reflowing or desoldering.
boolean HaltSystem = false;       // The flag to check if we halt the system process.
boolean ServoReversed = false;


/**
 * Setup Program
 */
void setup() {
  Serial.begin(9600); // Output for serial debugging

  // Setup i/o pins.
  pinMode(ButtonOnePin, INPUT);
  pinMode(ButtonTwoPin, INPUT);
  pinMode(LCDBacklightPin, OUTPUT);

  digitalWrite(LCDBacklightPin, HIGH); // Turn on the LCD backlight

  // Display startup logo for 5 seconds, show the love.
  PrintToLCD("FI-ATCSRP 0.01", "FisherInnovation"); 
  delay(5000);
  
  DisplayMenu(); // Display the main menu.
}


/**
 * Main loop
 */
void loop() {
  // Checks for button input when the system is not active.
  if(MenuDisplayed == true && ProcessStarted == false) {
    if(CheckButtons() == 1) InitReflow(); 	// Check for button one hit
	if(CheckButtons() == 2) InitDesolder(); // Check for button two hit
  }
  
  // Checks for button input when the system is active.
  if(MenuDisplayed == false && ProcessStarted == true) {
    if(CheckButtons() == 1) HaltProcess(); 	// Halt the system
	if(CheckButtons() == 2) HaltProcess(); // Halt the system
  }
}


/**
 * Print 2 lines of text to the 16x2 LCD screen.
 */
void PrintToLCD(char* LineOne, char* LineTwo) {
  lcd.clear();                  
  lcd.setCursor(0,0);       
  lcd.print(LineOne);
  lcd.setCursor(0,1);
  lcd.print(LineTwo);
  
  // Send serial data as well.
  Serial.print("LCD Display L1:");
  Serial.println(LineOne);
  Serial.print("LCD Display L2:");
  Serial.println(LineTwo);
  Serial.println("");
} 


/**
 * Converts the ThermocouplePin input to a temperature variable
 */
int ReadThermocouple() {
  Temperature = analogRead(ThermocouplePin);
  int Celsius = ( 5.0 * Temperature * 100.0) / 1024.0;
  //int Fahrenheit = (((Celsius * 9) / 5) + 32);
  
  Serial.print("INFO: Reading Device Temperature: ");
  Serial.print(Celsius);
  Serial.println("");
  
  return Celsius;
}


/**
 * Move the servo to a given position in a given amount of time.
 */
void MoveServo(int Position, int DelayTime) {
  // Position protection
  if(Position > 180) Position = 180;
  if(Position < 0) Position = 0;
  
  (!ServoReversed) ? ControlServo.write(Position) : ControlServo.write(Position);
  
  if(DelayTime != 0) delay(DelayTime);
}


/**
 * Checks the status of the buttons
 */
int CheckButtons() {
  // Check button one for press
  ButtonOneVal = digitalRead(ButtonOnePin);
  if(ButtonOneVal == HIGH && PreviousButtonOneState == LOW && millis() - ButtonOneTime > DebounceTime) {
    ButtonOneTime = millis();  // Remember when the last button press was
    Serial.println("INFO: Button One");
    return 1;
  }
  PreviousButtonOneState = ButtonOneVal;
  
  // Check button two for press
  ButtonTwoVal = digitalRead(ButtonTwoPin);
  if(ButtonTwoVal == HIGH && PreviousButtonTwoState == LOW && millis() - ButtonTwoTime > DebounceTime) {
    ButtonTwoTime = millis();  // Remember when the last button press was
    Serial.println("INFO: Button Two");
    return 2;
  }
  PreviousButtonTwoState = ButtonTwoVal;
}


/**
 * Displays the menu for the user to select the action to perform
 */
void DisplayMenu() {
  MenuDisplayed = true;
  Serial.println("INFO: Displaying Main Menu");
  PrintToLCD("1. Reflow", "2. Desolder");
}


/**
 * Halts the system no matter in what phase its in.
 */
void HaltProcess() {
  HaltSystem = true;
  Serial.println("ALERT: Halting System!");
  PrintToLCD("HALTING SYSTEM", "Please Wait..."); 
}



/**
 * Resets the system to get ready for another job.
 */
void RestartProcess() {
  // Check that relay is off and the plate is not powered.

  MenuDisplayed = false;
  ProcessStarted = false;
  delay(10000);
	
  PrintToLCD("FI-ATCSRP 0.01", "FisherInnovation"); 
  delay(5000);
  DisplayMenu(); // Display the main menu, start over again.
}


/**
 * Starts the reflow process. 
 *
 * NOTE: Only allow actions to complete as long as
 * there is no halt action made. Halt actions are checked by nature in this
 * method but in some conditions when other methods run for an extended 
 * period of time, they will check for them as well. If a halt action is found
 * then we just restart the process.
 */
void InitReflow() {
  Serial.println("INFO: Reflow Process Started");
  ProcessStarted = true;
  MenuDisplayed = false;
  
  // Check for a stable system.
  if(SystemConfig() && !HaltSystem) {
    if(!HaltSystem) WaitForStableTemp(); // Stabilize the plate temperature.
    
    // Wait 10 seconds for the user to place the circuit on the plate.
    if(!HaltSystem) PrintToLCD("Place Circuit", "On Plate Now");
    if(!HaltSystem) delay(10000); 
   
    if(!HaltSystem) StartPreheat(); 	// Slowly Preheat the plate.
    if(!HaltSystem) StartReflowPrep();	// Amp up temperature to just before reflow point.
    if(!HaltSystem) StartReflow();		// Start the reflow process.
    if(!HaltSystem) StartCooldown();	// Slowly cooldown the plate.
	
    // Wait 10 seconds for the user to remove the circuit from the plate.
    if(!HaltSystem) PrintToLCD("Reflow Done", "Remove Circuit");
      RestartProcess();
  } else {
      PrintToLCD("System Config", "Config Failed!!"); // Alert the user the config test failed.
  }
}


/**
 * Starts the desoldering process
 */
void InitDesolder() {
  Serial.println("INFO: Desolder Process Started");
  ProcessStarted = true;
  MenuDisplayed = false;
  
  // Check for a stable system.
  if(SystemConfig() && !HaltSystem) {
    if(!HaltSystem) WaitForStableTemp(); // Stabilize the plate temperature.
	
    // Wait 10 seconds for the user to place the circuit on the plate.
    if(!HaltSystem) PrintToLCD("Place Circuit", "On Plate Now");
    if(!HaltSystem) delay(10000);
	
    // Set the board to full power
    if(ControlServo.read() != ServoHighPosition && !HaltSystem) {
      ControlServo.write(ServoHighPosition);
      delay(1000);
    }
	
    if(!HaltSystem) PrintToLCD("Heating Plate", "Please Wait..");
	
    // Heat the plate up untill its temp is 260 degrees celsius.
    do {
    }
    while(ReadThermocouple() < 260 && !HaltSystem);
	
    // Give the user 20 seconds to remove the IC and clean up the area.
    if(!HaltSystem) PrintToLCD("Remove IC", "Soak Solder");
    if(!HaltSystem) delay(20000);
	
    if(!HaltSystem) ControlServo.write(ServoLowPosition);
    if(!HaltSystem) PrintToLCD("Cooling Plate", "Please Wait...");
	
    do {
    }
    while(ReadThermocouple() > LowestTemperature && !HaltSystem);
	
    if(!HaltSystem) PrintToLCD("Desolder Done", "Remove Circuit");
    RestartProcess();
  }
}


/**
 * Test the system for faults
 */
boolean SystemConfig() {
  Serial.println("INFO: Starting System Config");
  PrintToLCD("System Config", "Please Wait...");

  // Check if the servo is setup to spin forward or backwards to get hot, make it public.
  (ServoLowPosition < ServoHighPosition) ? ServoReversed = false : ServoReversed = true;
  
  // Need to get a relay to turn the power on/off here...

  ControlServo.attach(ServoPin); // Attach the control servo

  // Set the servo at the lowest postion
  if(ControlServo.read() != ServoLowPosition) MoveServo(ServoLowPosition, 1000);
  
  // Sweep the servo to highest temperature
  ControlServo.write(ServoHighPosition);           
  
  // Check for temperature rise.
  boolean TempTestDone = false;
  boolean TempTestFail = false;
  int Celsius = 0;
  int BeforeTemp = 0;
  int TempTestCount = 0; // Count the times we test the temperature.
  
  do {
    Celsius = ReadThermocouple();
    BeforeTemp = Celsius; 
    delay(5000); // Wait for 5 seconds    
    Celsius = ReadThermocouple(); // Check if there was a rise in temperature.
  
    // Check if there was a 5 degree offset in 5 seconds.
    if(BeforeTemp + 5 < Celsius) TempTestDone = true; // Heat element is working and heating up.
    else (TempTestCount >= 50) ? TempTestFail = TempTestDone = true : TempTestCount++; // If the test fails 50 times, stop the program the heat element must not be connected or is broken.
  }
  while(TempTestDone == false);
  
  if(TempTestFail) return false; // Check to see if the test failed.
 
  MoveServo(ServoLowPosition, 1000);
  return true;
}


/**
 * Watch the temperature input until its stable.
 */
void WaitForStableTemp() {
  Serial.println("INFO: Waiting For Stable Temperature");
  
  MoveServo(ServoLowPosition, 500); // Double check that the servo is holding the temperature at the lowest setting.
  
  // Check for base temperature
  int Celsius  = LastTemperature = ReadThermocouple();

  // Watch the temp till its steady
  do {
    LastTemperature = Celsius;
    delay(2000); // wait two seconds before testing temp again.
    Celsius = LastTemperature;
    
    lcd.clear();  
    lcd.setCursor(0,0);       
    lcd.print("Setting Min Temp");
    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(Celsius);
    lcd.print(char(223));
    lcd.print("C/150"); 
    lcd.print(char(223));
    lcd.print("C"); 
  }
  while(LastTemperature == Celsius && Celsius <= 150); // Make sure the temp is stable and 150C or below. 
  
  Serial.print("INFO: Stable Temperature Found: ");
  Serial.print(Celsius);
  Serial.print(char(223));
  Serial.println("C");
  
  LowestTemperature = Celsius; // Mark this as our plates lowest temperature.
  
  lcd.clear();  
  lcd.setCursor(0,0);       
  lcd.print("Min. Temp Set");
  lcd.setCursor(0,1);
  lcd.print("Temp:");
  lcd.print(LowestTemperature);
  lcd.print(char(223));
  lcd.print("C"); 
  delay(1000); // Let the user know the lowest temp.
}


/**
 * Heat up the board from lowest temp to 200C a +3C/s no more then 120 seconds
 */
void StartPreheat() {
  Serial.println("INFO: Starting Preheat Process");
  
  MoveServo(ServoLowPosition, 5000); // Double check the servo position.
  
  int Duration = 120;
  int PreviousServoPosition = ControlServo.read(); 	// Check the current servo position.
  int PreviousTemperature = ReadThermocouple();
  int CurrentTemperature = PreviousTemperature;
  
  for(int i = 0; i < Duration; i++) {
    lcd.clear();  
    lcd.setCursor(0,0);       
    lcd.print("Preheating Plate");
    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(CurrentTemperature);
    lcd.print(char(223));
    lcd.print("C/200");
    lcd.print(char(223));
    lcd.print("C");
    
    CurrentTemperature = ReadThermocouple();
    
    // Check if temp is to low
    if(CurrentTemperature <= PreviousTemperature) {
      // Temp is going down 
      (!ServoReversed)? MoveServo(PreviousServoPosition + 5, 0) : MoveServo(PreviousServoPosition - 5, 0);
    } 
    
    // Check if to high
    
    // Check the time/temp
    
    
    delay(1000);
  }
}


/**
 * Heat up the board to 260C  in 20 seconds
 */
void StartReflowPrep() {
  Serial.println("INFO: Starting Reflow Preperation Process");
  
  int Duration = 20; 					// Duration of this process.
  int PreviousServoPosition = ControlServo.read(); 	// Check the current servo position.
  int PreviousTemperature = ReadThermocouple(); 	// Check inital cooldown cycle temperature.
  int CurrentTemperature = PreviousTemperature;
  
  for(int i = 0; i <= Duration; i++) {
    lcd.clear();  
    lcd.setCursor(0,0);  
    lcd.print("Prep ETA: ");
    lcd.print(Duration - i);
    lcd.print("s");
    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(ReadThermocouple());
    lcd.print(char(223));
    lcd.print("C");
	
     // check and modify.
	
    delay(1000);
  }
}


/**
 * Reflow for 30 seconds at 260 degrees.
 */
void StartReflow() {
  Serial.println("INFO: Starting Reflow Process");
  
  int Duration = 30; // Duration of this process.
  int PreviousTemperature = ReadThermocouple(); 	// Check inital cooldown cycle temperature.
  int CurrentTemperature = PreviousTemperature;
  int PreviousServoPosition = ControlServo.read(); 	// Check the current servo position.
  
  
  // Count down from 30
  for(int i = 0; i <= Duration; i++) {
    lcd.clear();  
    lcd.setCursor(0,0);       
    lcd.print("Reflow ETA: ");
    lcd.print(Duration - i);
    lcd.print("s");
    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(CurrentTemperature);
    lcd.print(char(223));
    lcd.print("C");

    CurrentTemperature = ReadThermocouple(); // Get inital temperature reading.
	
    // Hold the temp at 260 degrees celsius +-3 degrees celsius.
    if(CurrentTemperature != 260) {
      if(CurrentTemperature - 3 > 260) {
        // Temp is to high lower a bit
	(!ServoReversed)? MoveServo(PreviousServoPosition - 2, 0) : MoveServo(PreviousServoPosition + 2, 0); // Move the servo in the right direction.
      } else if(CurrentTemperature + 3 < 260) {
        // Temp is to low up it a bit
        (!ServoReversed)? MoveServo(PreviousServoPosition + 2, 0) : MoveServo(PreviousServoPosition - 2, 0); // Move the servo in the right direction.
      }
    }
	
    delay(1000); // 1 second delay to continue countdown.
  }
}


/**
 * Cooldown the plate -6C/s max
 */
void StartCooldown() {
  Serial.println("INFO: Starting Cooldown Process");
  
  int PreviousServoPosition = ControlServo.read(); 	// Check the current servo position.
  int PreviousTemperature = ReadThermocouple(); 	// Check inital cooldown cycle temperature.
  int CurrentTemperature = 0;
  
  do {
    CurrentTemperature = ReadThermocouple();// Read the temp.

    // Check if the temp is higher or the same.
    if(CurrentTemperature >= PreviousServoPosition) {
      // Lower the device temp alot
      (!ServoReversed) ? MoveServo(PreviousServoPosition - 10, 0) : MoveServo(PreviousServoPosition + 10, 0); // Move the servo in the right direction.
      delay(1000); // Two seconds should yield a negitive differnce (this and at the end).
    } else {
      // Watch that the decrease in temperature is no more then -6C/s.
      if(CurrentTemperature + 6 < PreviousServoPosition) {
        // Temp is dropping to fast
        if(CurrentTemperature + 10 < PreviousServoPosition) 
          (!ServoReversed) ? MoveServo(PreviousServoPosition + 2, 0) : MoveServo(PreviousServoPosition - 2, 0); // If the difference is two much, compensate.
        else 
          (!ServoReversed) ? MoveServo(PreviousServoPosition - 2, 0) : MoveServo(PreviousServoPosition + 2, 0); // All looks good, continue dropping the temp
    }
   }

   PreviousTemperature = CurrentTemperature;
   delay(1000); // One second check intervals.
  }
  while(ReadThermocouple() > LowestTemperature); // Do until the plates temp is back down to its lowest temp.
}

int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

