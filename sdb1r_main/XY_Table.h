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
    bool isPaused;
    bool pendingPause; // New flag for pending pause command
    float remainingDistanceX;
    float remainingDistanceY;
    bool filledTubes[MAX_TUBES_X][MAX_TUBES_Y];
    int32_t currentCycle;
};

//void checkButtonTag();
bool checkButtonTag(uint8_t buttonTag);

void activateDispenser(void);

void GPIO_Setup(void);
void init_Motors(void);
void Homing(void);

void runStepper(AccelStepper &stepper, int distance, uint8_t limitPin, const char *limitMsg, ActionState actionState);

bool runStepper_normal(AccelStepper &stepper, int distance, uint8_t limitPin, const char *limitMsg); //soon
void returnToStart();
void updateStateMachine(uint8_t buttonTag);
// void checkAndMoveStepper(AccelStepper &stepper, int steps, int limit, const char *message, ActionState action);

void checkAndMoveStepper_normal(AccelStepper &stepper, int steps, int limit, const char *message);
bool vibrateAndCheckPause();//soon
bool dispenseAndCheckPause();//soon
void Run_profile();
// void Resume_profile();
void RunOrResume_profile(bool isResuming);

void xy_init(void);
void xy_main(void);

bool performVibrateAndDispenseOperations();
void handleCycleStatus(int cycle);
void finishProcess();
void clearPauseState();

void startProcess();
void pausedProcess();
void resumeProcess();
void stopProcess();


#endif /*_XY_TABLE_H_*/
