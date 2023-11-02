/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#include "SaveProfile.h"
#include <EEPROM.h>
#include "Platform.h"

#define SINGLE_PROFILE

const int PROFILE_SIZE = sizeof(Profile);
//const int NUM_PROFILES = EEPROM_SIZE / PROFILE_SIZE;
const int NUM_PROFILES = 1;
const int MAX_BUTTONS = 250;

int getProfileAddress(int profileId) {
    return PROFILE_START_ADDR + profileId * PROFILE_SIZE;
}

bool checkPasscode(Gpu_Hal_Context_t *phost, const char *enteredPasscode, const char *correctPasscode)
{
    if (strcmp(enteredPasscode, correctPasscode) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

#ifdef SINGLE_PROFILE
bool loadProfile(Profile &profile)
{
    EEPROM.get(PROFILE_START_ADDR, profile);
    
    // Ensure profileName is null-terminated after loading from EEPROM
    profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';
    
    profile.profileId = 1; // This can be fixed since we have only one profile now
    return true;
}

void saveProfile(Profile &profile)
{
    // Ensure profileName is null-terminated
    profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';
    
    // Save the profile to the EEPROM
    EEPROM.put(PROFILE_START_ADDR, profile);
    
    // The following lines are not required for a single profile:
    // profileId = 0; 
    // EEPROM.update(0, profileId + 1); // Increment the profileId before updating it in EEPROM
}

#endif

#ifndef SINGLE_PROFILE

bool loadProfile(Profile &profile, int32_t profileId)
{
    // Serial.print("Loading profile at profileId ");
    // Serial.println(profileId);

    if (profileId >= 0 && profileId < NUM_PROFILES)
    {
        EEPROM.get(PROFILE_START_ADDR + profileId * sizeof(Profile), profile);
        // Ensure profileName is null-terminated after loading from EEPROM
        profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';

        profile.profileId = profileId; // Set the correct profileId after loading the profile
        return true;
    }
    else
    {
        return false;
    }
}

void saveProfile(Profile &profile, int32_t profileId) // pass-by-reference could be more effcieint in memeory handling then pass-by-value(without&)
{
    // Ensure profileName is null-terminated
    profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';

    // Set other parameters for the profile as needed
    EEPROM.put(PROFILE_START_ADDR + profileId * sizeof(Profile), profile);

    // Reset profileId to 0 if it is equal to or more than NUM_PROFILES
    if (profileId >= NUM_PROFILES)
    {
        profileId = 0;
    }
    // Serial.print("Saving profile with ID ");
    // Serial.println(profileId);

    // Update the last used profileId in EEPROM
    EEPROM.update(0, profileId + 1); // Increment the profileId before updating it in EEPROM
}

//bool loadProfile(Profile &profile, int32_t profileId) {
//    if (profileId < 0 || profileId >= NUM_PROFILES) {
//        return false;  // Invalid profileId
//    }
//
//    EEPROM.get(getProfileAddress(profileId), profile);
//    
//    // Ensure profileName is null-terminated after loading from EEPROM
//    profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';
//    
//    profile.profileId = profileId;
//    return true;
//}
//
//void saveProfile(Profile &profile, int32_t profileId) {
//    if (profileId < 0 || profileId >= NUM_PROFILES) {
//        return;  // Invalid profileId
//    }
//
//    // Ensure profileName is null-terminated
//    profile.profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';
//    
//    // Save the profile to the EEPROM
//    EEPROM.put(getProfileAddress(profileId), profile);
//}

#endif

void initializeProfile(Profile &profile)
{
    // Initialize the button states to false
    for (int32_t i = 0; i < profile.Tube_No_y; i++)
    {
        for (int32_t j = 0; j < profile.Tube_No_x; j++)
        {
            profile.buttonStates[i][j] = false;
        }
    }
}


uint8_t getLastUsedProfileId()
{
    uint8_t lastProfileId = EEPROM.read(0);
    if (lastProfileId == 0)
    {
        // EEPROM is uninitialized, return a default profile ID (e.g., 1)
        return 0;
    }
    return lastProfileId;
}

void getLastUsedProfileNames(char *profileName)
{
    uint8_t lastProfileId = EEPROM.read(0);
    if (lastProfileId == 0)
    {
        // EEPROM is uninitialized, return a default profile Name
        strcpy(profileName, "Default");
        return;
    }
    uint16_t eepromAddress = (lastProfileId - 1) * PROFILE_NAME_MAX_LEN; // calculate the address in EEPROM
    for (uint8_t i = 0; i < PROFILE_NAME_MAX_LEN; i++)
    {
        char readChar = EEPROM.read(eepromAddress + i);
        profileName[i] = readChar;
        if (readChar == '\0')
        {
            // If we encounter a null character, we can stop reading.
            break;
        }
    }
    // Ensure the profileName is null-terminated.
    profileName[PROFILE_NAME_MAX_LEN - 1] = '\0';
}


//Print multiple profile
//void printProfiles(Profile &profile) // profile is passed by reference
//{
//    Serial.println("Printing profiles...");
//    for (int i = 0; i < NUM_PROFILES; i++)
//    {
//        if (loadProfile(profile, i)) // assuming loadProfile function correctly loads the profile at index i into the passed profile
//        {
//            Serial.print("profileName: ");
//            Serial.println(profile.profileName);
//            Serial.print("Profile ID: ");
//            Serial.println(profile.profileId);
//            Serial.print("Number of X tubes: ");
//            Serial.println(profile.Tube_No_x);
//            Serial.print("Number of Y tubes: ");
//            Serial.println(profile.Tube_No_y);
//            Serial.print("Tube pitch x: ");
//            Serial.println(profile.pitch_x);
//            Serial.print("Tube pitch y: ");
//            Serial.println(profile.pitch_y);
//            Serial.print("Tray origin X: ");
//            Serial.println(profile.trayOriginX);
//            Serial.print("Tray origin Y: ");
//            Serial.println(profile.trayOriginY);
//            Serial.print("Number of cycles: ");
//            Serial.println(profile.Cycles);
//            Serial.print("Vibration enabled: ");
//            Serial.println(profile.vibrationEnabled ? "true" : "false");
//            Serial.print("Dispenser enabled: ");
//            Serial.println(profile.dispenseEnabled ? "true" : "false");
//            Serial.println("Button states:");
//
//            for (int j = 0; j < profile.Tube_No_y; j++) // iterate over rows
//            {
//                for (int k = 0; k < profile.Tube_No_x; k++) // iterate over columns
//                {
//                    Serial.print(profile.buttonStates[j][k] ? "OFF " : "ON ");
//                }
//                Serial.println(); // newline for each row
//            }
//        }
//    }
//}

// Print Single Profile
void printProfiles(Profile &profile) 
{
    Serial.println("Printing profile...");

    // No need for a loop since we have just one profile
    loadProfile(profile);  // Assuming loadProfile has been modified as per the previous answer
//    loadProfile(profile, profile.profileId);

    Serial.print("profileName: ");
    Serial.println(profile.profileName);
    Serial.print("Profile ID: ");
    Serial.println(profile.profileId);
    Serial.print("Number of X tubes: ");
    Serial.println(profile.Tube_No_x);
    Serial.print("Number of Y tubes: ");
    Serial.println(profile.Tube_No_y);
    Serial.print("Tube pitch x: ");
    Serial.println(profile.pitch_x);
    Serial.print("Tube pitch y: ");
    Serial.println(profile.pitch_y);
    Serial.print("Tray origin X: ");
    Serial.println(profile.trayOriginX);
    Serial.print("Tray origin Y: ");
    Serial.println(profile.trayOriginY);
    Serial.print("Number of cycles: ");
    Serial.println(profile.Cycles);
    Serial.print("Vibration enabled: ");
    Serial.println(profile.vibrationEnabled ? "true" : "false");
    Serial.print("Dispenser enabled: ");
    Serial.println(profile.dispenseEnabled ? "true" : "false");
    Serial.println("Button states:");

    for (int j = 0; j < profile.Tube_No_y; j++) // iterate over rows
    {
        for (int k = 0; k < profile.Tube_No_x; k++) // iterate over columns
        {
            Serial.print(profile.buttonStates[j][k] ? "OFF " : "ON ");
        }
        Serial.println(); // newline for each row
    }
}

void getProfileName(int index, char *buffer, size_t bufferSize)
{
    Profile tempProfile;
    int addr = index * sizeof(Profile);
    EEPROM.get(addr, tempProfile);
    strncpy(buffer, tempProfile.profileName, bufferSize);
}

void getLastProfileAndCopy()
{
    const char *lastProfileName = getLastUsedProfileName();
    // Create a buffer to hold the copied string
    char copiedProfileName[PROFILE_NAME_MAX_LEN + 1];
    // Copy the profile name
    copyProfileName(copiedProfileName, lastProfileName, PROFILE_NAME_MAX_LEN + 1);
}

void copyProfileName(char *dest, const char *src, size_t maxSize)
{
    // Copy the string
    strncpy(dest, src, maxSize - 1);
    // Null-terminate the string
    dest[maxSize - 1] = '\0';
}

const char *getLastUsedProfileName()
{
    static char profileName[PROFILE_NAME_MAX_LEN]; // define static variable, MAX_PROFILE_NAME_LENGTH should be defined as maximum possible length of profile name.

    uint8_t lastProfileId = EEPROM.read(0);
    if (lastProfileId == 0)
    {
        // EEPROM is uninitialized, return a default profile name
        strncpy(profileName, "Default Name", PROFILE_NAME_MAX_LEN - 1);
        profileName[PROFILE_NAME_MAX_LEN - 1] = '\0'; // Ensure null-termination
        return profileName;
    }
    lastProfileId -= 1;

    Profile profile;
    EEPROM.get(PROFILE_START_ADDR + lastProfileId * sizeof(Profile), profile);

    // Copy the name of the profile to the static variable
    strncpy(profileName, profile.profileName, PROFILE_NAME_MAX_LEN - 1);
    profileName[PROFILE_NAME_MAX_LEN - 1] = '\0'; // Ensure null-termination

    // Return the static variable
    return profileName;
}

bool getProfileByIndex(Profile &profile, int32_t index)
{
    Serial.print("Getting profile at index ");
    Serial.println(index);

    if (index >= 0 && index < NUM_PROFILES)
    {
        EEPROM.get(PROFILE_START_ADDR + index * sizeof(Profile), profile);
        profile.profileId = index; // Set the correct profileId after loading the profile
        return true;
    }
    else
    {
        return false;
    }
}

void cleanupProfile(Profile &profile)
{
    for (int i = 0; i < profile.Tube_No_y; i++)
    {
        delete[] profile.buttonStates[i];
    }
    delete[] profile.buttonStates;
}

void WriteProfileToEEPROM(int address, Profile &profile)
{
    EEPROM.put(address, profile.profileName);
    address += sizeof(profile.profileName);
    EEPROM.put(address, profile.profileId);
    address += sizeof(profile.profileId);
    EEPROM.put(address, profile.Tube_No_x);
    address += sizeof(profile.Tube_No_x);
    EEPROM.put(address, profile.Tube_No_y);
    address += sizeof(profile.Tube_No_y);
    EEPROM.put(address, profile.pitch_x);
    address += sizeof(profile.pitch_x);
    EEPROM.put(address, profile.pitch_y);
    address += sizeof(profile.pitch_y);
    EEPROM.put(address, profile.trayOriginX);
    address += sizeof(profile.trayOriginX);
    EEPROM.put(address, profile.trayOriginY);
    address += sizeof(profile.trayOriginY);
    EEPROM.put(address, profile.Cycles);
    address += sizeof(profile.Cycles);
    EEPROM.put(address, profile.vibrationEnabled);
    address += sizeof(profile.vibrationEnabled);
    EEPROM.put(address, profile.dispenseEnabled);
    address += sizeof(profile.dispenseEnabled);

    for (int i = 0; i < profile.Tube_No_y; i++)
    {
        for (int j = 0; j < profile.Tube_No_x; j += 8)
        {
            byte b = 0;
            for (int bit = 0; bit < 8 && (j + bit) < MAX_BUTTONS_X; ++bit)
            {
                if (profile.buttonStates[i][j + bit])
                    b |= (1 << bit);
            }
            EEPROM.put(address, b);
            address += sizeof(b);
        }
    }
}

void ReadProfileFromEEPROM(int address, Profile &profile)
{
    EEPROM.get(address, profile.profileName);
    address += sizeof(profile.profileName);
    EEPROM.get(address, profile.profileId);
    address += sizeof(profile.profileId);
    EEPROM.get(address, profile.Tube_No_x);
    address += sizeof(profile.Tube_No_x);
    EEPROM.get(address, profile.Tube_No_y);
    address += sizeof(profile.Tube_No_y);
    EEPROM.get(address, profile.pitch_x);
    address += sizeof(profile.pitch_x);
    EEPROM.get(address, profile.pitch_y);
    address += sizeof(profile.pitch_y);
    EEPROM.get(address, profile.trayOriginX);
    address += sizeof(profile.trayOriginX);
    EEPROM.get(address, profile.trayOriginY);
    address += sizeof(profile.trayOriginY);
    EEPROM.get(address, profile.Cycles);
    address += sizeof(profile.Cycles);
    EEPROM.get(address, profile.vibrationEnabled);
    address += sizeof(profile.vibrationEnabled);
    EEPROM.get(address, profile.dispenseEnabled);
    address += sizeof(profile.dispenseEnabled);

    for (int i = 0; i < MAX_BUTTONS_Y; i++)
    {
        for (int j = 0; j < MAX_BUTTONS_X; j += 8)
        {
            byte b;
            EEPROM.get(address, b);
            address += sizeof(b);
            for (int bit = 0; bit < 8 && (j + bit) < MAX_BUTTONS_X; ++bit)
            {
                profile.buttonStates[i][j + bit] = (b & (1 << bit));
            }
        }
    }
}
