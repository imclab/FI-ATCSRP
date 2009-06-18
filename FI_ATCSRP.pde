/****************************************************************************************
 * FI-ATCSRP (Fisher Innovation - Autonomous Temperature Controlled Solder Reflow Plate)
 *                             www.FisherInnovation.com
 *                                 2009 Matt Fisher
 ****************************************************************************************/

#include <LiquidCrystal.h>
#include <Servo.h>


// The following variables should be the only changes needed to get this code working. Be sure to have a 180 degree rotation
// servo hooked up to your temperature control in any fassion you want as long as it can rotate from off/verylow to the highest setting.

/* Start User Defined Variables */

// Pin Connections
int ThermocouplePin = 0;                     // Thermcouple Pin
int ButtonOnePin = 6;                        // Button 1 Pin
int ButtonTwoPin = 7;                        // Button 2 Pin
int ServoPin = 9;                            // Servo Data Pin
int LCDBacklightPin = 13;                    // LCD Backlight Pin

// Servo Control
int ServoLowPosition = 180;                  // Position the servo is in to have the heat at low or off
int ServoHighPosition = 0;                   // Position the servo is in to have the heat at high

// Temperature Limits (all temperature are read in celcius)
int MaxDeviceTemperature = 260;              // The maximum temperature that we will allow the device to get up to
int ReflowTemperature = 255;                 // The temperature we expect the board to reflow at

// LCD Screen
boolean LCDBacklightOn = true;               // If we should turn the backlight on or not
LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);   // rw on pin 11, rs on pin 12, enable on pin 10, d4, d5, d6, d7 on pins 5, 4, 3, 2

/* End User Defined Variables */


Servo ControlServo;               // Servo to control the heat element temperature control.
int Temperature = 0;              // Current temperature
int LowestTemperature = 0;        // The lowest temperature we have achived with the board on. (this will be generated automatically).
int ServoPosition = 0;            // Servo position storage
int ButtonOneVal = 0;             // Button 1 state value
int ButtonTwoVal = 0;             // Button 2 state value
int PreviousButtonOneState = LOW; // Button 1 previous state
int PreviousButtonTwoState = LOW; // Button 2 previous state
int TemperatureIndex[20];         // An array to hold the temperature of the board in 10 servo movment intervals
long ButtonOneTime = 0;	          // Time since the last button one press
long ButtonTwoTime = 0;	          // Time since the last button two press
long DebounceTime = 500;          // The debounce time
boolean MenuDisplayed = false;    // If the menu is being displayed or not.
boolean ProcessStarted = false;   // If we are reflowing or desoldering.
boolean ServoReversed = false;    // If the user has the servo attached to the heating element backward or forwards (ie 0-180 or 180-0).


/**
 * Setup Program
 */
void setup() {
  Serial.begin(9600);                                      // Output for serial debugging

  // Setup i/o pins.
  pinMode(ButtonOnePin, INPUT);
  pinMode(ButtonTwoPin, INPUT);
  pinMode(LCDBacklightPin, OUTPUT);

  if(LCDBacklightOn) digitalWrite(LCDBacklightPin, HIGH);  // Turn on the LCD backlight

  DisplayStartupScreen(5000);                              // Display startup screen for 5 seconds.
  DisplayMenu();                                           // Display the main menu.
}


/**
 * Main loop
 */
void loop() {
  // Checks for button input when the system is not active.
  if(MenuDisplayed == true && ProcessStarted == false) {
    if(CheckButtons() == 1) SetupProcess(0); // Reflow
    if(CheckButtons() == 2) SetupProcess(1); // Desolder
  }
}


/**
 * Print 2 lines of text to the 16x2 LCD screen.
 * 
 * @param    LineOne    A char array max 16 character of text for line one
 * @param    LineTwo    A char array max 16 character of text for line two
 */
void PrintToLCD(char* LineOne, char* LineTwo) {
  lcd.clear();                  
  lcd.setCursor(0,0);       
  lcd.print(LineOne);
  lcd.setCursor(0,1);
  lcd.print(LineTwo);
} 


/**
 * Converts the ThermocouplePin input to a temperature variable (Celsius).
 */
int ReadThermocouple() { return (5.0 * analogRead(ThermocouplePin) * 100.0) / 1024.0; }


/**
 * Displays the main startup screen.
 */
void DisplayStartupScreen(int DelayTime) { 
  PrintToLCD("FI-ATCSRP 0.02", "FisherInnovation");
  delay(DelayTime);
}

/**
 * Move the servo to a given position in a given amount of time.
 *
 * @param    Position      The position to move the servo to (0 - 180).
 * @param    DelayTime     The amount of delay time to add to allow the servo to get there.
 */
void MoveServo(int Position, int DelayTime = 0) {
  ControlServo.write(constrain(Position, 0, 180)); // Move the servo to something between 0-180
  if(DelayTime != 0) delay(DelayTime);
}


/**
 * Attempts to figure out how much rotation the servo needs in order to achive the proper heating element temperature.
 * 
 * @param    TimeLeft              The amount of time left in the running process.
 * @param    PreviousTemperature   What the previous temperature reading was.
 * @param    CurrentTemperature    The current temperature of the plate (We could get this value inside this method..)
 * @param    MaxTemperature        The maximum temperature the heat element can be in this process.
 * @param    MaxTempVariation      The maximum variation in temperature we want to see (degrees celsius per second).
 *
 * @return   AmountToUpdateServo   The amount to add to the current servo position.
 */
int CalculateCompensation(int TimeLeft, int PreviousTemperature, int CurrentTemperature, int MaxTemperature, int MaxTempVariation = 0) {
  byte CrunchTime = 10;                                                          // The amount of time (seconds) that will be left until we start making sure we reach out target temperature.
  int ServoBuffer = ServoHighPosition - ControlServo.read();                     // Calculate the max servo buffer for movment.
  int DegreeChangePerSecond = (MaxTemperature - CurrentTemperature) / TimeLeft;  // Amount of temp to be changing per second to reach goal.
  int AmountToUpdateServo = 0;                                                   // The amount to update the servo by.
  int FutureServoPosition = 0;                                                   // A variable to hold where we want to move the servo to.
  int RoundingCalculation = 0;                                                   // A place to store a var when calculating the closes 10th spot
  
  // Temperature variation check and response (1 Unit Update)
  if(MaxTempVariation != 0) {                                                                   
    if(CurrentTemperature - PreviousTemperature > MaxTempVariation) AmountToUpdateServo--;
    else if(CurrentTemperature - PreviousTemperature < MaxTempVariation) AmountToUpdateServo++;
  }
  
  // Check if the remaining time is less then CrunchTime seconds (make final adjustments)
  if(TimeLeft < CrunchTime && TimeLeft > 0) {
     if(CurrentTemperature < MaxTemperature) AmountToUpdateServo++;
     if(CurrentTemperature > MaxTemperature) AmountToUpdateServo--;
  }
  
  // Double check that the calculated movment won't put the plate over temp.
  // We do this by checking against the data the configuration temp test stored.
  FutureServoPosition = ControlServo.read() + AmountToUpdateServo;
  
  // Round the future postion to the nearest 10 spot recored from the ControlServo.
  RoundingCalculation = FutureServoPosition % 10;
  if(RoundingCalculation < 5) RoundingCalculation = FutureServoPosition - RoundingCalculation;
  else {
    RoundingCalculation = 10 - RoundingCalculation;
    RoundingCalculation = FutureServoPosition + RoundingCalculation;
  }
  
  // Check the rounding calculation against of temperature array created in the config. (NOTE: Off by one reference in array)
  // If we rounded up to find this number just zero out this update, if we are rounding down, we need to lower the dial because we are over temp.
  if(TemperatureIndex[(RoundingCalculation / 10) - 1] > MaxTemperature && FutureServoPosition % 10 < 5) AmountToUpdateServo = AmountToUpdateServo - 2; 
  
  return AmountToUpdateServo; // Return a numeric value to move servo.
}


/**
 * Check if a button has been pressed and returns the button number
 * if it was pressed. Also check for debouncing to avoid button presses
 * when no buttons have actually been pressed.
 *
 * @return  0 = No Button Press
 *          1 = Button 1 Press
 *          2 = Button 2 Press
 */
int CheckButtons() {
  ButtonOneVal = digitalRead(ButtonOnePin); // Check button one
  if(ButtonOneVal == HIGH && PreviousButtonOneState == LOW && millis() - ButtonOneTime > DebounceTime) {
    ButtonOneTime = millis();  // Remember when the last button press was
    return 1;
  }
  PreviousButtonOneState = ButtonOneVal;
  
  ButtonTwoVal = digitalRead(ButtonTwoPin); // Check button two
  if(ButtonTwoVal == HIGH && PreviousButtonTwoState == LOW && millis() - ButtonTwoTime > DebounceTime) {
    ButtonTwoTime = millis();  // Remember when the last button press was
    return 2;
  }
  PreviousButtonTwoState = ButtonTwoVal;
  
  return 0;
}


/**
 * Displays the menu for the user to select the action to perform
 */
void DisplayMenu() {
  MenuDisplayed = true;
  PrintToLCD("1. Reflow", "2. Desolder");
}


/**
 * Test the system for faults return true if config is ok false
 * if it fails config tests.
 */
boolean SystemConfig() {
  PrintToLCD("System Config...", "Please Wait...");

  // Check if the servo is setup to spin forward or backwards to get hot, make it public.
  (ServoLowPosition < ServoHighPosition) ? ServoReversed = false : ServoReversed = true;
  
  // Need to get a relay to turn the power on/off here...

  ControlServo.attach(ServoPin);  // Attach the control servo
  if(ControlServo.read() != ServoLowPosition) MoveServo(ServoLowPosition, 1000); // Set the servo at the lowest postion
  
  // Check the plate temp for every 10 postion changes of the servo
  int CheckInterval = 10;
  int TempReading1 = 0;
  int TempReading2 = 0;
  int TempReading3 = 0;
  boolean StableTemp = false;

  for(int i = 0; i < 18; i++) { 
    do {
      TempReading3 = TempReading2;
      TempReading2 = TempReading1;
      TempReading1 = ReadThermocouple();  
      
      // Make sure last 3 checks are the same temp.
      if(TempReading1 == TempReading2 == TempReading3) {
        StableTemp = true;
        TemperatureIndex[i] = TempReading1; // Store stable temp reading in array.
        if(i == 0) LowestTemperature = TempReading1; // Store the inital temp as the lowest temp.
      } else {
        // Display the temperature as we wait for another check.
        for(int n = 0; n < 20; n++) {
         lcd.setCursor(0,1);
         lcd.print("P:"); // Position
         int CurrentPosition = ControlServo.read();
         lcd.print(CurrentPosition);
         lcd.print(" T:"); // Temperature
         TempReading1 = ReadThermocouple();
         lcd.print(TempReading1);
         lcd.print(char(223));
         lcd.print("C      ");
         
         delay(500); 
        }
      }
    }
    while(!StableTemp);
    
    (!ServoReversed)? MoveServo(ControlServo.read() + 10) : MoveServo(ControlServo.read() - 10); // Move the servo 10 spots and wait.
  }
    
  MoveServo(ServoLowPosition, 1000); // Return to the lowest temperature setting.
  StableTemp = false;                // Re-mark the temp as non-stable.
  
  // Restabilze the temperature at the min setting and make sure its within the given limits in the 
  // user defined settings as MinDeviceTemperature.
  do {
    TempReading3 = TempReading2;
    TempReading2 = TempReading1;
    TempReading1 = ReadThermocouple(); 
    if(TempReading1 == TempReading2 == TempReading3) StableTemp = true;
    
    lcd.setCursor(0,1);
    lcd.print("T:"); // Temperature
    lcd.print(TempReading1);
    lcd.print(char(223));
    lcd.print("C           ");  // The extra spacing clears remaining characters.
    
    delay(1000); // Check the heat element temperature in 2 second intervals.
  }
  while(!StableTemp);//&& MinDeviceTemperature < TempReading1);
  
  return true;
}


/**
 * Starts the reflow process.
 * 
 * @param    ProcessID  The ID of the process to setup (0 = Reflow, 1 = Desolder).
 */
void SetupProcess(int ProcessID) {
  ProcessStarted = true;
  MenuDisplayed = false;
  
  if(SystemConfig()) {
    PrintToLCD("Place Circuit", "On Plate Now"); 
    delay(10000);                                  // Wait 10 seconds for the user to place the circuit on the plate.
   
    switch(ProcessID){
      case 0:  // Reflow
        InitProcess(0, 120, 3, 200);               // Heat up the board from lowest temp to 200C a +3C/s no more then 120 seconds
        InitProcess(1, 20, 0, ReflowTemperature);  // Heat up the board from 200C to 260C in 20 seconds
        InitProcess(2, 30, 0, ReflowTemperature);  // Reflow for 30 seconds at 260 degrees.
        InitProcess(4, 30, -6, LowestTemperature); // Cool down to base temp at -6C/s max.
	
        PrintToLCD("Reflow Done", "Remove Circuit");
      break;
      
      case 1:  // Desolder 
        if(ControlServo.read() != ServoHighPosition) MoveServo(ServoHighPosition, 1000); // Set the board to full power
        PrintToLCD("Heating Plate", "Please Wait..");
        
        InitProcess(3, 180, 3, 260);               // Heat up the board from lowest temp to 260C a +3C/s no more then 180 seconds
        
        PrintToLCD("Remove IC", "Soak Solder");
        delay(20000);                              // Give the user 20 seconds to remove the IC and clean up the area.
        
        InitProcess(4, 30, 0, LowestTemperature);  // Cool down to base temp at -6C/s max.
        
        PrintToLCD("Desolder Done", "Remove Circuit");
      break;
    }
    delay(10000);                                  // Wait 10 seconds for the user to remove the circuit off the plate.
    RestartProcess();                              // Restart the system (not a hard restart, just bring up the menu again).
  }
}


/**
 * Resets the system to get ready for another job.
 */
void RestartProcess() {
  // Check that relay is off and the plate is not powered.

  MenuDisplayed = false;
  ProcessStarted = false;
  delay(10000);
	
  DisplayStartupScreen(5000);
  DisplayMenu();                // Display the main menu, start over again.
}


/**
 * This method assembles the process of heating and cooling the plate.
 *
 * @param    ProcessID               The process ID - Preheat = 0, HeatUp = 1, Reflow = 2, Desolder = 3, Cooling = 4.
 * @param    Duration                The MAXIMUM amount of time this process can take.
 * @param    MaxTemperatureChage     The MAXIMUM amount of temperature change per second (in degrees celsius).
 * @param    TemperatureGoal         The temperature we need to achive before the end of the process.
 */
void InitProcess(int ProcessID, int Duration, int MaxTemperatureChage, int TemperatureGoal) {
  int CurrentTemperature = ReadThermocouple();
  int PreviousTemperature = CurrentTemperature;
  int ServoCompensation = 0;
  int PreviousServoPosition = 0;
  
  // If the duration is set, we must time our process. So we will run a for loop with the duration set as the 
  // interval amount and use a one second delay at the end of each loop.
  if(Duration > 0) {
    for(int i = 0; i < Duration; i++) {
      CurrentTemperature = ReadThermocouple();     // Get base temperature.
      ServoCompensation = CalculateCompensation(Duration, PreviousTemperature, CurrentTemperature, TemperatureGoal, MaxTemperatureChage); // Plug collected data into the compensation algorithm.
      PreviousServoPosition = ControlServo.read(); // Remember this servo position so we can add our new compensation value to it.
    
      // Move the servo in the proper direction. Pending there is a change to be made.
      if(ServoCompensation != 0) {
        if(ServoCompensation > 0) (!ServoReversed) ? MoveServo(PreviousServoPosition + ServoCompensation) : MoveServo(PreviousServoPosition - ServoCompensation);
        else (!ServoReversed) ? MoveServo(PreviousServoPosition - ServoCompensation) : MoveServo(PreviousServoPosition + ServoCompensation);
      }  
      
      lcd.clear();  
      lcd.setCursor(0,0);       
      
      // Display the proper process name on the LCD.
      switch(ProcessID) {
        // TODO: Concat the strings and int's before adding to the LCD thus allowing us to use the PrintTOLCD method.
        case 0: lcd.print("Preheating Plate"); break;
        case 1: lcd.print("Heating Plate"); break;
        case 2: lcd.print("Reflowing"); break;
        case 3: lcd.print("Preheating Plate"); break;
      }
      
      // TODO: Concat the strings and int's before adding to the LCD thus allowing us to use the PrintTOLCD method.
      lcd.setCursor(0,1);
      lcd.print("Temp:");
      lcd.print(CurrentTemperature);
      lcd.print(char(223));
      lcd.print("C");
      
      PreviousServoPosition = CurrentTemperature;   // Store this temp reading for next interval.
      delay(1000);
    }
  } else {
    // There is no time limit just do the work till the job is done (ie.cooling the plate).
    do{ 
      CurrentTemperature = ReadThermocouple();      // Get a base temperature reading.
      ServoCompensation = CalculateCompensation(0, PreviousTemperature, CurrentTemperature, 0, -6);
   
      // Move the servo in the proper direction.
      if(ServoCompensation != 0) {
        if(ServoCompensation > 0) (!ServoReversed) ? MoveServo(PreviousServoPosition + ServoCompensation) : MoveServo(PreviousServoPosition - ServoCompensation);
        else (!ServoReversed) ? MoveServo(PreviousServoPosition - ServoCompensation) : MoveServo(PreviousServoPosition + ServoCompensation);
      }
    
      // TODO: Concat the strings and int's before adding to the LCD thus allowing us to use the PrintTOLCD method.
      lcd.clear();  
      lcd.setCursor(0,0);       
      lcd.print("Cooling Plate");
      lcd.setCursor(0,1);
      lcd.print("Temp:");
      lcd.print(CurrentTemperature);
      lcd.print(char(223));
      lcd.print("C");
    
      PreviousServoPosition = CurrentTemperature;  // Store this temp reading for next interval.
      delay(1000);                                 // One second check intervals.
    }
    while(ReadThermocouple() >= LowestTemperature + 5); 
  }
}
