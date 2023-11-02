/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#ifndef _SAVEPROFILE_H_
#define _SAVEPROFILE_H_

#include <Arduino.h>
#include "XY_Table.h"
#include "Platform.h"

#define EEPROM_SIZE 2048 
#define MAX_PROFILES (EEPROM_SIZE / sizeof(Profile))
#define PROFILE_START_ADDR 0

#define PROFILE_NAME_MAX_LEN 30

#define MAX_BUTTONS_X 13 // Columns
#define MAX_BUTTONS_Y 17 // Rows

typedef struct
{
    char profileName[30];
    int32_t profileId = 0;
    int32_t Tube_No_x = 0; // represent the number of columns
    int32_t Tube_No_y = 0; // represent the number of rows
    float pitch_x = 0.0;
    float pitch_y = 0.0;
    float trayOriginX = 0.0;
    float trayOriginY = 0.0;
    int32_t Cycles = 1;
    int32_t CurrentCycle = 0; // New field for storing the current cycle
    bool vibrationEnabled =false;
    bool dispenseEnabled = false;
    // bool **buttonStates; // Declare as a double pointer
    bool buttonStates[MAX_BUTTONS_Y][MAX_BUTTONS_X]; // Declare as a static 2D array
} Profile;

uint8_t getLastUsedProfileId();
const char *getLastUsedProfileName();

bool checkPasscode(Gpu_Hal_Context_t *phost, const char *enteredPasscode, const char *correctPasscode);

uint16_t calculateChecksum(const Profile &profile);
//bool loadProfile(Profile &profile, int32_t profileId);
bool loadProfile(Profile &profile);

void initializeProfile(Profile &profile);
void getLastUsedProfileNames(char *profileName);
//void saveProfile(Profile &profile, int32_t profileId);
void saveProfile(Profile &profile);

void printProfiles(Profile &profile);
void getProfileName(int index, char *buffer, size_t bufferSize);
void getLastProfileAndCopy();
void copyProfileName(char *dest, const char *src, size_t maxSize);

bool getProfileByIndex(Profile &profile, int32_t index);

void cleanupProfile(Profile &profile);

void WriteProfileToEEPROM(int address, Profile &profile);
void ReadProfileFromEEPROM(int address, Profile &profile);

#endif /*_SAVEPROFILE_H_*/
