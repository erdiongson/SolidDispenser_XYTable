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
Profile CurProf; //current profile
uint8_t CurProfNum;//current profile id


AccelStepper stepper_x(1, Motor_x_CLK, Motor_x_CW);
AccelStepper stepper_y(1, Motor_y_CLK, Motor_y_CW);

Profile profile; // Create a profile object
ActionState action;

volatile CommandState currentState = IDLE;
PauseState pauseState; //= {0, 0, 0, 0, 0, 0, 0, ActionState::MoveY, false, false};

uint8_t CurX;
uint8_t CurY;
uint8_t TotalTubeLeft;

int currentCycle = 0;
char Password[4][PROFILE_NAME_MAX_LEN]={ 	"XQokay",//super password
											"init1234",//current password
											0,//user enter password to be check
											0}; //user enter 2nd time password to be check
bool SpecialMode;

#if DEBUG
void Dprint(char x)
{
	Serial.print(x,HEX);
}
void Dprint(String x)
{
	Serial.print(x);
}
void Dprint(String x, String y)
{
	Serial.print(x);
	Serial.print(y);
}
void Dprint(String x, float y)
{
	char buf[20];

	dtostrf(y,3,5,buf);
	Serial.print(x);
	Serial.println(buf);
}
void Dprint(String x, uint8_t y)
{
	char buf[20];

	sprintf(buf,"%d",y);
	Serial.print(x);
	Serial.println(buf);
}

#else
void Dprint(char x)
{}
void Dprint(String x)
{}
void Dprint(String x, String y)
{}
void Dprint(String x, uint8_t y)
{}
void Dprint(String x, float y)
{}

#endif

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
   stepper_x.setSpeed(motor_x_speed);
  stepper_x.setMaxSpeed(motor_x_speed);
  stepper_x.setAcceleration(motor_x_Acceleration);

   stepper_y.setSpeed(motor_y_speed);
  stepper_y.setMaxSpeed(motor_y_speed);
  stepper_y.setAcceleration(motor_y_Acceleration);
  //stepper_x.setSpeed(10000);
  //stepper_y.setSpeed(10000);
}

bool Homing()
{
	bool successx=FALSE;
	bool successy=FALSE;
#if !DEBUG
  Serial.println("Homing Start...");
#else
	Dprint("Debugging Start...");
#endif
 
  stepper_x.moveTo(40000);
#if !DEBUG
  while (digitalRead(Limit_S_x_Homing) != 0)
    stepper_x.run();
  delay(20);
  if(digitalRead(Limit_S_x_Homing)==0 && digitalRead(Limit_S_x_MAX)!=0) successx=TRUE;
#endif
  Serial.println(" limit x hit");
  stepper_x.stop();
  stepper_x.setCurrentPosition(0);
  stepper_x.setMaxSpeed(motor_y_speed);
  stepper_x.setAcceleration(motor_y_Acceleration);

  stepper_y.moveTo(-40000);
#if !DEBUG
  while (digitalRead(Limit_S_y_Homing) != 0)
    stepper_y.run();
  delay(20);
    if(digitalRead(Limit_S_y_Homing)==0 && digitalRead(Limit_S_y_MAX)!=0) successy=TRUE;
#endif
  Serial.println(" limit y hit");
  stepper_y.stop();
  stepper_y.setCurrentPosition(0);
  stepper_y.setMaxSpeed(motor_y_speed);
  stepper_y.setAcceleration(motor_y_Acceleration);


	return successx && successy;
}

uint8_t runStepper_normal(AccelStepper &stepper, long distance, uint8_t limitPin)
{
	uint8_t ret=0;
	uint8_t localkeypressed=0;

  	stepper.move(distance);
	Dprint("move motor","\n");
 //stepper.runToPosition();

  	while (stepper.distanceToGo() != 0)
	{
	
	    if (digitalRead(limitPin) == 0)
	    {//stop motor
          ret=STOP;
	      	break;
	    }
		if(localkeypressed==0) localkeypressed =Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
		if (localkeypressed==STOP || localkeypressed==PAUSE) break;

  		stepper.run();
  	}
    if (localkeypressed==STOP || localkeypressed==PAUSE)
    {
		ret=localkeypressed;
	    if(localkeypressed==STOP) 
      	{
      		Home_Menu(&host, MAINMENU);
      	}
      	else Home_Menu(&host, PAUSEMENU);
		while (stepper.distanceToGo() != 0)//continue if motor not yet complete
		{	
		    if (digitalRead(limitPin) == 0)
		    {//stop motor
            ret=STOP;
		      	break;
		    }
			stepper.run();
		}
		
    }
  	stepper.stop();
  	Dprint("motor_end","\n");
  return ret;
}




uint8_t vibrateAndCheckPause()
{
	uint8_t i;
	bool pausestop=FALSE;
	uint8_t ret=0;

	Dprint("vib","\n");
	sendCommand(Vibrate_Mode_ON);

	for(i=0;i<9;i++)
	{
	    if(!pausestop)
		{
			ret = GetKeyPressed();
			if(ret==STOP || ret==PAUSE) 
			{
				pausestop=TRUE;
				//Dprint("P_S","\n");
			}

	    }
		delay(100);
	}
	Dprint("vib_end","\n");

	sendCommand(Vibrate_Mode_OFF);

  return ret;
}

uint8_t dispenseAndCheckPause()
{
	uint8_t i;
	bool pausestop=FALSE;
	uint8_t ret=0;

	Dprint("dispen","\n");

    sendCommand(SDB_Dispense_START);
	  
	for(i=0;i<9;i++)
	{
	  if(!pausestop)
	  {
		  ret = GetKeyPressed();
		  if(ret==STOP || ret==PAUSE) 
		  {
			pausestop=TRUE;
			Dprint("P_S","\n");
		  }
	  }
	  delay(100);
	}
    sendCommand(SDB_Dispense_STOP);
	Dprint("dispen_end","\n");
  return ret;//soon
}

bool PauseOperation(void)
{
	uint8_t localkeypressed = 0;
	bool ret=true;//return true if continue. return false if hardstop
	Home_Menu(&host, PAUSEMENU);

	while (1)
	{
	  localkeypressed = GetKeyPressed();
	  if(localkeypressed == START || localkeypressed == STOP ) 
	  {
		  if(localkeypressed == START ) 
		  {
		  	Home_Menu(&host, RUNMENU);
			break;
		  }
		  else 
		  {//when stop pressed
		  	Home_Menu(&host,MAINMENU);
			ret=FALSE;
			break;
		  }
	  }
	  
	}

	return ret;
}


void startProcess() 
{
	uint8_t temp;
  	int cycle;
	int direction;
	int x=0;
	int y=0;
	char buf[20];


	temp=offsetStepperPosition();
	if(temp!=0) 
	{
		if(temp==STOP)	return;
		else if(!PauseOperation()) return;
	}

    for (y =  0; y < CurProf.Tube_No_y; y++)
    {

		if (y!=0) temp=runStepper_normal(stepper_y, CurProf.pitch_y * STEPS_PER_UNIT_Y, Limit_S_y_MAX);
		CurY=y;
     	Home_Menu(&host, RUNMENU);
		if(temp!=0) 
	   	{
	   		if(temp==STOP)	return;
	   		else if(!PauseOperation()) return;
		}
		if(CurProf.buttonStates[CurX][CurY])
		{
			for (cycle = 0; cycle < CurProf.Cycles; cycle++)
		  	{
      			if (!performVibrateAndDispenseOperations()) return;
			}
		}

    	if(TotalTubeLeft>0) TotalTubeLeft--;
    	Home_Menu(&host, RUNMENU);
		for (x = 0; x < CurProf.Tube_No_x - 1; x++)
      	{
			Dprint("Cycle y  x","\n");
		  	Dprint(cycle);
		  	Dprint("	");
		  	Dprint(y);
		  	Dprint("  ");
		  	Dprint(x);
		  	Dprint("\n");
       
	         direction = y % 2 == 0 ? -1 : 1;
			if(direction==-1) CurX++;
			else 
			{
			if(CurX>0) CurX--;
			}
          	Home_Menu(&host, RUNMENU);
			

    	    temp=runStepper_normal(stepper_x, direction * CurProf.pitch_x * STEPS_PER_UNIT_X, Limit_S_x_MAX);
			if(temp!=0) 
			{
				if(temp==STOP)  return;
			 	else if(!PauseOperation()) return;
		 	}
			if(CurProf.buttonStates[CurX][CurY])
			{
				for (cycle = 0; cycle < CurProf.Cycles; cycle++)
				{
					if (!performVibrateAndDispenseOperations()) return;
				}
			}
      if(TotalTubeLeft>0) TotalTubeLeft--;
          Home_Menu(&host, RUNMENU);

      }

      if (y == CurProf.Tube_No_y - 1 && cycle < CurProf.Cycles - 1)
	  {
	  	Homing();
	  }
	}
}


bool performVibrateAndDispenseOperations()
{
	bool_t ret=true; // return true if continue. return false if hardstop
	uint8_t pausestop=0;

	if(CurProf.vibrationEnabled) pausestop=vibrateAndCheckPause();
	if(pausestop==STOP) Home_Menu(&host,  MAINMENU);
	if(pausestop==PAUSE) if(!PauseOperation()) pausestop=STOP;
	if(pausestop!=STOP)
	{	
		pausestop=dispenseAndCheckPause();
		if(pausestop==STOP) Home_Menu(&host,  MAINMENU);
 		if(pausestop==PAUSE) if(!PauseOperation()) pausestop=STOP;
	}
	if(pausestop!=STOP)
	{
		if(CurProf.vibrationEnabled) pausestop=vibrateAndCheckPause();
		if(pausestop==STOP) Home_Menu(&host,  MAINMENU);
		if(pausestop==PAUSE ) if(!PauseOperation()) pausestop=STOP;
	}

	if(pausestop==STOP) ret=FALSE;//hardstop
	return ret;
}


//Origin X and Origin Y Offset
uint8_t offsetStepperPosition(void)
{
	uint8_t keypressed=0;
	uint8_t ret=0;
	int xMoveValue;
	int yMoveValue;

  	// Calculate the moveTo values for both steppers
	xMoveValue = CurProf.trayOriginX * STEPS_PER_UNIT_X;
  	yMoveValue = CurProf.trayOriginY * STEPS_PER_UNIT_Y;

  	// Print the original and calculated offset values
	Serial.print("Tray Origin X: ");
	Serial.println(CurProf.trayOriginX);
	Serial.print("Tray Origin Y: ");
	Serial.println(CurProf.trayOriginY);

	Serial.print("Stepper X MoveTo Value: ");
	Serial.println(xMoveValue);
	Serial.print("Stepper Y MoveTo Value: ");
	Serial.println(yMoveValue);

  	// Use the calculated values to move the steppers
  	stepper_x.moveTo(-xMoveValue);
  	stepper_y.moveTo(yMoveValue);

	while (stepper_x.distanceToGo() != 0 || stepper_y.distanceToGo() != 0)
	{

		if(keypressed==0) keypressed =Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
		if (keypressed==STOP || keypressed==PAUSE) break;
		stepper_x.run();
		stepper_y.run();
	}
	if (keypressed==STOP || keypressed==PAUSE)
	{
	    ret=keypressed;
		if(keypressed==STOP) 
		{
			Home_Menu(&host, MAINMENU);
		}
		else Home_Menu(&host, PAUSEMENU);
		while (stepper_x.distanceToGo() != 0 || stepper_y.distanceToGo() != 0)
		{	
			stepper_x.run();
			stepper_y.run();
		}
	}
  	return ret;
}



void setup()
{
	int i;
  	phost = &host;


	SpecialMode=FALSE;
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
	//Gpu_Hal_Wr8(phost, REG_PWM_DUTY, 10);//brightness control

  // handle_uart_command(Handshake);

  	Logo_XQ_trans(&host);
  	Dprint("Firmware version :",FWVER);

	Gpu_Hal_Wr8(phost,REG_TOUCH_SETTLE,3);
  // Disble/Enable handshaking
  	sendCommand(Handshake);

#if !DEBUG
	i=10;
    while (receiveResponse().command != 0x60)
	{
      	delay(100);
	  	if(i>0) i--;
	  	else
	  	{
			Home_Menu(&host, TXRXERROR);
		}
	
	 }
#endif 

  	sendCommand(Vibrate_Mode_OFF);
  	sendCommand(SDB_Dispense_STOP);
	
#if !DEBUG

    if( digitalRead(Limit_S_x_Homing) == 0)
    {
      Dprint("motor x out","\n");
      stepper_x.moveTo(-2000);
      //while (digitalRead(Limit_S_x_Homing) == 0)
      while (stepper_x.distanceToGo() != 0)
        stepper_x.run();
        delay(1000);
    }
    if( digitalRead(Limit_S_y_Homing) == 0)
    {
      Dprint("motor y out","\n");
      stepper_y.moveTo(2000);
      //while (digitalRead(Limit_S_x_Homing) == 0)
      while (stepper_y.distanceToGo() != 0)
        stepper_y.run();
        delay(1000);
    }
     
#endif


    if(!Homing())
	{
#if !DEBUG
		Home_Menu(&host, HOMEERROR);
		while(1);
#endif
	}
	ReadPassEEPROM(Password[2]);
	if(Password[2][0]!=0) strcpy(Password[1],Password[2]);//if eeprom is blank load preset value
  	CurProfNum=LoadProfile();
  	Home_Menu(&host, MAINMENU);

}

void loop()
{
	uint8_t keypressed;
	char buf[PROFILE_NAME_MAX_LEN];
	int i;
	
	
    keypressed = GetKeyPressed(); // get button pressed
	
    if (keypressed!=0)
    {
		Home_Menu(&host,MAINMENU);
        switch(keypressed)
		{
			case START: Dprint("Cycle start","\n");
						CurX=0;
						CurY=0;
						TotalTubeLeft=(CurProf.Tube_No_x)*(CurProf.Tube_No_y);
						Home_Menu(&host, RUNMENU);
						WaitKeyRelease();
						startProcess();
						Dprint("Cycle end","\n");
            			Homing();
						//CurX=0;
						//CurY=0;
            			Home_Menu(&host, MAINMENU);
						keypressed=0;
						break;
			case SETTING: // Setting button
						Dprint("Enter Setting");
						if(digitalRead(Limit_S_y_MAX)==0) SpecialMode=TRUE;
						else SpecialMode=FALSE;
						Password[2][0]=0;
						WaitKeyRelease();
						Keyboard(phost,Password[2],"Enter Password",FALSE);
						if(strcmp(Password[1],Password[2])==0 || strcmp(Password[0],Password[2])==0)
						{
							if(SpecialMode) 
							{
                				if(digitalRead(Limit_S_y_MAX)!=0)
							  	{
							      	BlankEEPROM();
								 }
									else SpecialMode=FALSE;
							}
							DisplayConfig(phost);
							WaitKeyRelease();
							delay(100);
							Config_Settings(phost);
						}
						else
						{
							DisplayKeyboard(phost,0,"Wrong Password"," ", FALSE, FALSE, FALSE);
							delay(2000);
						}
						Home_Menu(&host, MAINMENU);
						keypressed=0;
						delay(500);
						break;
			default : 	keypressed=0;
						break;

		 }
    }
  //Gpu_Hal_Close(phost);
  //Gpu_Hal_DeInit();
}
