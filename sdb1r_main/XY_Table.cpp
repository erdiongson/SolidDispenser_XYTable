/* Author : XentiQ
  Date created - 2022.12.14 - XentiQ version
*/

#include "Platform.h"
#include "App_Common.h"
#include <EEPROM.h>
#include "Config.h"
#include "SaveProfile.h"
#include "XY_Table.h"
#include "UART.h"

Gpu_Hal_Context_t host, *phost;

AccelStepper stepper_x(1, Motor_x_CLK, Motor_x_CW);
AccelStepper stepper_y(1, Motor_y_CLK, Motor_y_CW);

Profile profile; // Create a profile object
ActionState action;

volatile CommandState currentState = IDLE;
PauseState pauseState; //= {0, 0, 0, 0, 0, 0, 0, ActionState::MoveY, false, false};

int totalCycles; // Store the total number of cycles

bool ProfileNameTag = false;
bool TubeRowTag = false;
bool TubeColumnTag = false;
bool PitchRowTag = false;
bool PitchColumnTag = false;
bool OriginRowTag = false;
bool OriginColumnTag = false;
bool CyclesTag = false;
bool SelectProfile_flag = false;
bool defaultProfile_flag = false;
bool isStopped = false;
bool getStartButtonState = false; // declare globally
bool isResuming = false;
bool isPaused = false;

int32_t tube_no_x = 0;
int32_t tube_no_y = 0;
float pitch_row_x = 0.0;
float pitch_col_y = 0.0;
float trayOriginX_row = 0.0;
float trayOriginY_col = 0.0;
int32_t cyclesNo = 0;

int cnt = 0;
int currentCycle = 0;
int currentX = 0;
int currentY = 0;

void GPIO_Setup()
{
  Serial.begin(19200);
  pinMode(Motor_ON, OUTPUT);
  pinMode(Motor_UP, OUTPUT);
  pinMode(Motor_Dow, OUTPUT);
  pinMode(Vibrate, OUTPUT);
  pinMode(Limit_S_x_Homing, INPUT_PULLUP);
  pinMode(Limit_S_y_Homing, INPUT_PULLUP);
  pinMode(Limit_S_x_MAX, INPUT_PULLUP);
  pinMode(Limit_S_y_MAX, INPUT_PULLUP);
}

void init_Motors()
{
  // stepper_x.setSpeed(motor_x_speed);
  stepper_x.setMaxSpeed(motor_x_speed);
  stepper_x.setAcceleration(motor_x_Acceleration);

  // stepper_y.setSpeed(motor_y_speed);
  stepper_y.setMaxSpeed(motor_y_speed);
  stepper_y.setAcceleration(motor_y_Acceleration);
}

void Homing()
{
  Serial.println("Homing Start...");
  stepper_x.moveTo(40000);
  while (digitalRead(Limit_S_x_Homing) != 0)
    stepper_x.run();
  Serial.println(" limit x hit");
  stepper_x.stop();
  stepper_x.setCurrentPosition(0);
  stepper_x.setMaxSpeed(motor_y_speed);
  stepper_x.setAcceleration(motor_y_Acceleration);

  stepper_y.moveTo(-40000);
  while (digitalRead(Limit_S_y_Homing) != 0)
    stepper_y.run();
  Serial.println(" limit y hit");
  stepper_y.stop();
  stepper_y.setCurrentPosition(0);
  stepper_y.setMaxSpeed(motor_y_speed);
  stepper_y.setAcceleration(motor_y_Acceleration);

}

void runStepper_normal(AccelStepper &stepper, int distance, uint8_t limitPin, const char *limitMsg)
{
  Serial.println("Stepper Movement");
  Profile profile; //testcode 7 this is enable
  stepper.move(distance);
  
  while (pauseState.isPaused)//soon B
  {
    if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
  {
    pauseState.isPaused=false;  
    Home_Menu2(&host, profile);
    }
  else Home_Menu(&host, profile);
  }//soon E
  
  while (stepper.distanceToGo() != 0)
  {
    while (pauseState.isPaused)//soon B
    {
      if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
      {
        pauseState.isPaused=false;  
        Home_Menu2(&host, profile);
      }
      else Home_Menu(&host, profile);
    }//soon E
    
    if (isStopped || digitalRead(limitPin) == 0)
    {
      isStopped = true;
      Serial.println(limitMsg);
      Homing();
      profile.Cycles = 0;
      profile.Tube_No_x = 0;
      profile.Tube_No_y = 0;
      currentState = STOP_BUTTON;
      break;
    }

    //    uint8_t buttonTag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
    int checkResult = checkButtonTag(buttonTag);
    if (checkResult == -1)
    {
      isStopped = true; // Make sure the system will stop completely after Homing
      Homing();
      profile.Cycles = 0;
      profile.Tube_No_x = 0;
      profile.Tube_No_y = 0;
      currentState = STOP_BUTTON;
      break;
    }
    else if (!checkResult)
    {
      break;
    }
    
    while (pauseState.isPaused)//soon B
    {
      if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
      {
        pauseState.isPaused=false;  
        Home_Menu2(&host, profile);
      }
      else Home_Menu(&host, profile);
    }//soon E
    
    stepper.run();
  }
  stepper.stop();
}

bool checkButtonTag(uint8_t buttonTag)
{
  if (buttonTag == PAUSE)
  {
    buttonTag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
    if (buttonTag == START) // 3 =start
    {
      // isResuming = true;
      //      currentState = START_BUTTON;
      updateStateMachine(buttonTag);
      return true;
    }
    else if (buttonTag == STOP) // 5 =stop
    {
//      currentState = HOMING;
      isStopped = true; // This variable ensures the stop command is handled in runStepper()
      updateStateMachine(buttonTag);
      return false;
    }
    else
      return checkButtonTag(buttonTag); // recursive call
  }
  return (buttonTag != STOP);
}

void updateStateMachine(uint8_t buttonTag)
{
  switch (currentState)
  {
    case IDLE:
      Serial.println("State Machine: IDLE");
      if (buttonTag == START) {
        Serial.println("SM Button Pressed: START");
        startProcess();
      } else if (buttonTag == PAUSE) {
        Serial.println("SM Button Pressed: PAUSE");
        pausedProcess();
      } else if (buttonTag == STOP) {
        Serial.println("SM Button Pressed: STOP");
        stopProcess();
      }
      Serial.println("State Machine: IDLE - END");
      break;

    case HOMING:
      {
        Serial.println("State Machine: HOMING");
        Homing();
        currentState = IDLE; // State is set to IDLE after Homing is finished
        Serial.println("State Machine: HOMING - END");
        break;
      }

    case START_BUTTON: // Execute logic for START
      {
        Serial.println("State Machine: START_BUTTON");
        startProcess();
        currentState = IDLE;
        Serial.println("State Machine: START_BUTTON - END");
        break;
      }

    case PAUSE_BUTTON: // Execute logic for PAUSE
      {
        Serial.println("State Machine: PAUSE_BUTTON");
        pausedProcess();
        currentState = IDLE;
        Serial.println("State Machine: PAUSE_BUTTON - END");
        break;
      }

    case STOP_BUTTON:
      {
        Serial.println("State Machine: STOP_BUTTON");
        stopProcess();
        currentState = IDLE; // State is set to HOMING after Stop is pressed
        Serial.println("State Machine: STOP_BUTTON - END");
        break;
      }

    case ERROR1:
      {
        Serial.println("State Machine: ERROR1");
        Response response;
        sendCommand(SDB_Dispense_STOP);
        if (response.command == (uint8_t)Error_IR_Censor_Failure)
        {
          stepper_x.stop();
          stepper_y.stop();
          isResuming = false;
          Homing();
        }
        currentState = IDLE;
        Serial.println("State Machine: ERROR1 - END");
        break;
      }

    case ERROR2:
      {
        Serial.println("State Machine: ERROR2");
        Response response;
        sendCommand(SDB_Dispense_STOP);
        if (response.command == (uint8_t)Error_Marker_Not_Detected)
        {
          stepper_x.stop();
          stepper_y.stop();
          isResuming = false;
          Homing();
        }
        currentState = IDLE;
        Serial.println("State Machine: ERROR2 - END");
        break;
      }

    default:
      Serial.println("State Machine: DEFAULT - IDLE");
      currentState = IDLE;
      isResuming = false;
      Serial.println("State Machine: DEFAULT - IDLE - END");
      break;
  }
}

void startProcess() {
  Serial.println("startProcess - START");
  Home_Menu2(&host, profile);//soon
  
  if (pauseState.isPaused)
  {
    isResuming = true;
      
  }
  else
  {
    isResuming = false;
  }

  offsetStepperPosition(profile);

  if (pauseState.isPaused)
  {
    RunOrResume_profile(isResuming = true);
    isResuming = false;
  }
  else // Should be normal operation
  {
    RunOrResume_profile(isResuming = false);
  }
  // If you don't want to go through the START_BUTTON case again,
  // you can set currentState to IDLE here.
  Serial.println("startProcess - END");
  currentState = IDLE;
}

void pausedProcess() {
  Serial.println("pauseProcess - START");
  Response response;
  sendCommand(SDB_Dispense_PAUSE);
  if (response.command == (uint8_t)SDB_Dispense_PAUSE)
  {
    stepper_x.stop();
    stepper_y.stop();

    // Save the state of the motors
    pauseState.lastX = stepper_x.currentPosition();
    pauseState.lastY = stepper_y.currentPosition();

    // Correctly save the last action before the system was paused
    pauseState.lastAction = action;
    isStopped = false;
    isResuming = true;
  }
  Serial.println("pauseProcess - END");
  currentState = IDLE;
}

void stopProcess() {
  Serial.println("stopProcess - START");
  Response response;
  sendCommand(SDB_Dispense_STOP);
  if (response.command == (uint8_t)SDB_Dispense_STOP)
  {
    Homing();
    isStopped = true;
    isResuming = false;
    pauseState.isPaused = false; // Reset the pause state
    isStopped = false;           // Reset the stop flag
    //    stepper_x.stop();
    //    stepper_y.stop();
    //    //    Homing();
  }
  Serial.println("stopProcess - END");
  currentState = IDLE;
}

void checkAndMoveStepper_normal(AccelStepper & stepper, int steps, int limit, const char *message)
{
  runStepper_normal(stepper, steps, limit, message);
}

void vibrateAndCheckPause()
{
  // Only vibrate if isStopped is false
  if (!(isStopped))
  {
    if (!(pauseState.isPaused && pauseState.lastAction >= ActionState::VibrateOn))
    {
      if (profile.vibrationEnabled == true)
      {
        Serial.print(" profile.vibrationEnabled:  ");
        Serial.println(profile.vibrationEnabled);

        sendCommand(Vibrate_Mode_ON);
        Serial.print("Vibrate_Mode_ON:  ");
        Serial.println(Vibrate_Mode_ON);
        delay(800);
        sendCommand(Vibrate_Mode_OFF);
        Serial.print("Vibrate_Mode_OFF:  ");
        Serial.println(Vibrate_Mode_OFF);
        pauseState.lastAction = ActionState::VibrateOn; // Update the last action
      }

      if (Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == 4) // Pause button
      {
        pauseState.isPaused = true;
      }
      else
      {
        delay(200);
        sendCommand(Vibrate_Mode_OFF);
        Serial.print("Vibrate_Mode_OFF:  ");
        Serial.println(Vibrate_Mode_OFF);
      }
    }
  }
}

void turnOffVibration()
{
  if (profile.vibrationEnabled)
  {
    sendCommand(Vibrate_Mode_OFF);
  }
}

void dispenseAndCheckPause()
{
  // Only dispense if isStopped is false
  if (!(isStopped))
  {
    if (!(pauseState.isPaused && pauseState.lastAction >= ActionState::DispenseStart))
    {
      sendCommand(SDB_Dispense_START);
      delay(800);
      sendCommand(SDB_Dispense_STOP);
      pauseState.lastAction = ActionState::DispenseStart;

      if (Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == PAUSE) // Pause button
      {
        pauseState.isPaused = true;
      }
      else
      {
        delay(200);
      }
    }
  }
}

void RunOrResume_profile(bool isResuming)
{

  //  loadProfile(profile, profile.profileId);
  loadProfile(profile);
  getLastUsedProfileName();

  bool filledTubes[profile.Tube_No_x][profile.Tube_No_y] = {{false}};

  if (isResuming && !pauseState.isPaused)
  {
    Homing();
    return;
  }

  int startCycle = isResuming ? profile.CurrentCycle : 0;
  int endCycle = profile.Cycles;

  for (int cycle = startCycle; cycle < endCycle; cycle++)
  {
    if (isResuming && cycle < pauseState.cycle)
      continue;

    if (cycle < profile.Cycles - 1)
    {
      	Homing();
	  	Serial.println("Required");//soon
      Serial.println("Cycle y  x");//soon
   	  Serial.print(profile.Cycles,HEX);//soon
    	Serial.print("     ");//soon
    	Serial.print(profile.Tube_No_y,HEX);//soon
    	Serial.print("  ");//soon
    	Serial.println(profile.Tube_No_x,HEX);//soon
      if (!checkButtonTag(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG)))
        return;
    }

    for (int y = (isResuming && pauseState.lastAction <= ActionState::MoveY) ? pauseState.currentY : 0; y < profile.Tube_No_y; y++)
    {

      if (!checkButtonTag(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG)))
        return;

      if (y != 0 && !(pauseState.isPaused && pauseState.lastAction == ActionState::MoveY))
      {
        checkAndMoveStepper_normal(stepper_y, profile.pitch_y * STEPS_PER_UNIT_Y, Limit_S_y_MAX, "Checking Y MaxLimit...");
      }

      if (!performVibrateAndDispenseOperations())
        return;

      for (int x = (isResuming && pauseState.lastAction <= ActionState::MoveX) ? pauseState.currentX : 0; x < profile.Tube_No_x - 1; x++)
      {
        Serial.println("Cycle y  x");//soon
    	Serial.print(cycle,HEX);//soon
    	Serial.print("     ");//soon
    	Serial.print(y,HEX);//soon
    	Serial.print("  ");//soon
    	Serial.println(x,HEX);//soon
        if (!checkButtonTag(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG)))
          return;

        int direction = y % 2 == 0 ? -1 : 1;
        checkAndMoveStepper_normal(stepper_x, direction * profile.pitch_x * STEPS_PER_UNIT_X, Limit_S_x_MAX, "Checking X MaxLimit...");

        if (!checkButtonTag(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG)))
          return;

        if (!performVibrateAndDispenseOperations())
          return;

        filledTubes[x][y] = true;
        pauseState.filledTubes[x][y] = true;
      }

      if (pauseState.isPaused)
      {
        pauseState.currentY = y;
        return;
      }

      if (y == profile.Tube_No_y - 1 && cycle < profile.Cycles - 1)
      {
        Homing();
      }
    }

    handleCycleStatus(cycle);
    if (pauseState.isPaused)
      return;
  }

  if (!pauseState.isPaused)
  {
    finishProcess();
  }

  clearPauseState();
}


bool performVibrateAndDispenseOperations()
{
  vibrateAndCheckPause();
  //soon if (pauseState.isPaused)
    //soon return false;
  while (pauseState.isPaused)//soon B
  {
  	if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
	  {
		pauseState.isPaused=false;	
		Home_Menu2(&host, profile);
  	}
	else Home_Menu(&host, profile);
		
  }//soon E



  dispenseAndCheckPause();
  //soon if (pauseState.isPaused)
    //soon return false;
  while (pauseState.isPaused)//soon B
  {
	  if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
	  {
		  pauseState.isPaused=false;  
		  Home_Menu2(&host, profile);
	  }
	  else Home_Menu(&host, profile);

  }//soon E



  vibrateAndCheckPause();
  //soon if (pauseState.isPaused)
    //soon return false;
  while (pauseState.isPaused)//soon B
  {
	  if(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG) == START) 
	  {
		  pauseState.isPaused=false;  
		  Home_Menu2(&host, profile);
	  }
	  else Home_Menu(&host, profile);

  }//soon E


  return true;
}


void handleCycleStatus(int cycle)
{
  pauseState.cycle = cycle;
  if (pauseState.lastAction == ActionState::MoveX)
  {
    pauseState.lastX = stepper_x.currentPosition();
  }
  else if (pauseState.lastAction == ActionState::MoveY)
  {
    pauseState.lastY = stepper_y.currentPosition();
  }
}

void finishProcess()
{
  profile.CurrentCycle = 0;
  Homing();
  isStopped = false;
  Serial.println("Process Done... Thanks");
}

void clearPauseState()
{
  memset(&pauseState, 0, sizeof(pauseState));
  Serial.println("Ending Run/Resume profile...");
}

void setup()
{

  phost = &host;

  App_Common_Init(&host); //* Init HW Hal */
  //  App_Calibrate_Screen(&host); ///*Screen Calibration*//

  GPIO_Setup();
  init_Motors();


  Serial.begin(19200);  // Serial printing
#ifdef SERIAL_2_DEFAULT
  Serial2.begin(19200); // UART for Arduino-PIC18 communications
#endif

#ifndef SERIAL_2_DEFAULT
  Serial3.begin(19200); // UART for Arduino-PIC18 communications
#endif

  // handle_uart_command(Handshake);

  Logo_XQ_trans(&host);

  // Disble/Enable handshaking
  sendCommand(Handshake);
  /*  while(receiveResponse().command != 0x60){
      sendCommand(Handshake);
      delay(100);
    }*/
/*    while (receiveResponse().command != 0x60)
      delay(100);*/

  sendCommand(Vibrate_Mode_OFF);
  sendCommand(SDB_Dispense_STOP);

  //  while(!KeyPassCode(&host, profile, HOME_SCREEN)){
  //     //Do nothing;
  //   };

//      Homing();
  //  profile.profileId = getLastUsedProfileId();
  //  loadProfile(profile, profile.profileId); // Load the last saved Profile
  loadProfile(profile);

  Home_Menu(&host, profile);

}

void loop()
{
  while (true) {
    buttonTag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG); // get button pressed

    if (App_Read_Tag(phost)) // Allow if the Key is released
    {
      if (PageNum == 0) // Main menu
      {
        Serial.println("PAGE 0 - START");
        updateStateMachine(buttonTag);

        if (buttonTag == 2) // Setting button
        {
          Serial.println("buttonTag =2 , Setting buttons ");
          Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
        }
        Serial.println("PAGE 0 - END");
      }

      else if (PageNum == 1)
      {
        Serial.println("PAGE 1 - START");
        Serial.print("PageNum :");
        Serial.println(PageNum);

        while (true)
        {
          buttonTag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
          if (buttonTag == 6) // Home button
          {
            Serial.println("Button Pressed: HOME");
            Home_Menu(&host, profile);
            break;
          }
          else if (buttonTag == 7) // Profile_Menu screen
          {
            Serial.println("Button Pressed: Profile MENU");
            SelectProfile_flag = true;
            //            loadProfile(profile, profile.profileId);
            loadProfile(profile);
            Home_Menu(&host, profile);
            break;
          }
          else if (buttonTag == 11) // Load (Config Screen)
          {
            Serial.println("Button Pressed: LOAD");
            defaultProfile_flag = true;
            //            profile.profileId = getLastUsedProfileId();
            //            loadProfile(profile, profile.profileId); // Call the loadProfile() function to load the profile at the given index
            //            loadProfile(profile);
            //            Home_Menu(&host, profile);
            Profile_Menu(&host, profile);

            //            break;
          }
          else if (buttonTag == 8) // Advavanced button
          {
            Serial.println("Button Pressed: ADVANCED");
            //            confirmAdvanceSetting(phost);
            Tray_Screen(phost, profile);
            break;
          }
          else if (buttonTag == 12)
          {
            Serial.println("Button Pressed: SAVE");
            uint8_t lastProfileId = EEPROM.read(0);
            //            saveProfile(profile, lastProfileId); // Save profile after updating
            saveProfile(profile);
            // WriteProfileToEEPROM(0, profile);
            //            Profile_Menu(&host, profile);
            //            break;
          }

          else if (buttonTag == 13)
          {
            Serial.println("Button Pressed: PROFILE");
            ProfileNameTag = true;
            AlphaNumeric(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
          }
          else if (buttonTag >= 14 && buttonTag <= 20)
          {
            Serial.println("Button Pressed: Input Configuration");
            // ProfileNameTag = false;
            TubeRowTag = false;
            TubeColumnTag = false;
            PitchRowTag = false;
            PitchColumnTag = false;
            OriginRowTag = false;
            OriginColumnTag = false;
            CyclesTag = false;

            switch (buttonTag)
            {
              case 14:
                TubeRowTag = true;
                break;
              case 15:
                TubeColumnTag = true;
                break;
              case 16:
                PitchRowTag = true;
                break;
              case 17:
                PitchColumnTag = true;
                break;
              case 18:
                OriginRowTag = true;
                break;
              case 19:
                OriginColumnTag = true;
                break;
              case 20:
                CyclesTag = true;
                break;
            }
            Keyboard(&host, profile, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo);
          }
          else if (buttonTag == 25)
          {
            delay(50);
            profile.vibrationEnabled = !profile.vibrationEnabled; // Toggle the state
            // Update the display after changing the vibration state
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            //break;
          }

          else if (buttonTag == 246) // No button for Advanced Setting
          {
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
          }

          else if (buttonTag == 247) // Yes button for Advanced Setting
          {
            Tray_Screen(phost, profile);
            break;
          }
        }
        Serial.println("PAGE 1 - END");
      }
      else if (PageNum == 2)
      {
        Serial.println("PAGE 2 - START");
        Tray_Screen(phost, profile);
        Serial.println("PAGE 2 - END");
      }
    }
  }
  //  cleanupProfile(profile); // Call this when you're done with profile
  Gpu_Hal_Close(phost);
  Gpu_Hal_DeInit();
}

//Origin X and Origin Y Offset
void offsetStepperPosition(Profile & profile)
{
  profile.profileId = getLastUsedProfileId();
  //  loadProfile(profile, profile.profileId); // Call the loadProfile() function to load the profile at the given index
  loadProfile(profile);

  // Calculate the moveTo values for both steppers
  int xMoveValue = profile.trayOriginX * STEPS_PER_UNIT_X;
  int yMoveValue = profile.trayOriginY * STEPS_PER_UNIT_Y;

  // Print the original and calculated offset values
  Serial.print("Tray Origin X: ");
  Serial.println(profile.trayOriginX);
  Serial.print("Tray Origin Y: ");
  Serial.println(profile.trayOriginY);

  Serial.print("Stepper X MoveTo Value: ");
  Serial.println(xMoveValue);
  Serial.print("Stepper Y MoveTo Value: ");
  Serial.println(yMoveValue);

  // Use the calculated values to move the steppers
  stepper_x.moveTo(-xMoveValue);
  stepper_y.moveTo(yMoveValue);

  while (stepper_x.distanceToGo() != 0 || stepper_y.distanceToGo() != 0)
  {
    stepper_x.run();
    stepper_y.run();
  }
}
