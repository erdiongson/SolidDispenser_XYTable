/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#ifndef _SAVEPROFILE_H_
#define _SAVEPROFILE_H_

#include <Arduino.h>
#include "XY_Table.h"
#include "Platform.h"

#define EEPROM_SIZE 2048 
#define MAX_PROFILES 10//(EEPROM_SIZE / sizeof(Profile))
#define PROFILE_START_ADDR 0

#define PROFILE_NAME_MAX_LEN 30
#define KEYPAD_MAX_LEN 6

#define MAX_BUTTONS_X 13 // Columns
#define MAX_BUTTONS_Y 17 // Rows
#define MAXORGX 99.9
#define MAXORGY 99.9

#define MINNUMX 1
#define MINNUMY 1
#define MAXNUMX 13
#define MAXNUMY 17


#define MINPITCHX 0
#define MINPITCHY 0
#define MAXPITCHX 99.9
#define MAXPITCHY 99.9
#define MAXCYCLE 99
#define MINCYCLE 1


extern char Password[4][PROFILE_NAME_MAX_LEN];


typedef struct
{
    char profileName[PROFILE_NAME_MAX_LEN];
    int8_t profileId = 0;
    int8_t Tube_No_x = 0; // represent the number of columns
    int8_t Tube_No_y = 0; // represent the number of rows
    float pitch_x = 0.0;
    float pitch_y = 0.0;
    float trayOriginX = 0.0;
    float trayOriginY = 0.0;
    int16_t Cycles = 1;
    int16_t CurrentCycle = 0; // New field for storing the current cycle
    bool vibrationEnabled =false;
    bool dispenseEnabled = false;
    // bool **buttonStates; // Declare as a double pointer
    bool buttonStates[MAX_BUTTONS_X][MAX_BUTTONS_Y]; // Declare as a static 2D array
} Profile;

bool checkPasscode(Gpu_Hal_Context_t *phost, const char *enteredPasscode, const char *correctPasscode);

uint16_t calculateChecksum(const Profile &profile);
void loadProfile(void);
uint8_t LoadProfile(void);

void PreLoadEEPROM(void);
void BlankEEPROM(void);


void WriteCurIDEEPROM(uint8_t curprofid);
uint8_t ReadCurIDEEPROM(void);
void WritePassEEPROM(char *pass);

void ReadPassEEPROM(char *pass);



void WriteProfileEEPROM(int address, Profile &profile);
void ReadProfileEEPROM(int address, Profile &profile);

#endif /*_SAVEPROFILE_H_*/
extern Profile CurProf;
extern uint8_t CurProfNum;

