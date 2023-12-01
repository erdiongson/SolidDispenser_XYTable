/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#include "UART.h"
#include <AccelStepper.h>

#ifndef _XY_TABLE_H_
#define _XY_TABLE_H_

#define MAX_TUBES_X 13
#define MAX_TUBES_Y 17

//#define SERIAL_2_DEFAULT  //Comment this out for Serial 3 usage

enum class ActionState
{
    VibrateOn,
    VibrateOff,
    DispenseStart,
    DispenseStop,
    MoveX,
    MoveY,
};

struct PauseState
{
    int32_t x;
    int32_t y;
    int32_t cycle;
    float currentX;
    float currentY;
    long lastX;
    long lastY;
    ActionState lastAction;
    // ActionState currentAction;
    int32_t currentCycle;
};


void activateDispenser(void);

void GPIO_Setup(void);
void init_Motors(void);
bool Homing(void);

void runStepper(AccelStepper &stepper, int distance, uint8_t limitPin, const char *limitMsg, ActionState actionState);

uint8_t runStepper_normal(AccelStepper &stepper, long distance, uint8_t limitPin); 
void returnToStart();
// void checkAndMoveStepper(AccelStepper &stepper, int steps, int limit, const char *message, ActionState action);

void checkAndMoveStepper_normal(AccelStepper &stepper, int steps, int limit, const char *message);
uint8_t vibrateAndCheckPause();
uint8_t dispenseAndCheckPause();
void Run_profile();


void xy_init(void);
void xy_main(void);

bool performVibrateAndDispenseOperations();
void startProcess();
void resumeProcess();


#endif /*_XY_TABLE_H_*/
