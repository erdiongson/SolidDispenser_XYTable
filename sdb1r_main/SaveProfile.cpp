/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#include "SaveProfile.h"
#include <EEPROM.h>
#include "Platform.h"
#include "app_common.h"

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
void BlankEEPROM(void)
{
	int i;

	for(i=0;i<4096;i++) EEPROM.put(i,0);
}

void PreLoadEEPROM(void)
{
	uint8_t i,x,y;
	char buf[20];
	Profile tempprof;

	for(i=0;i<MAX_PROFILES;i++)
	{
		sprintf(buf,"profile %d",i+1);
		strcpy(tempprof.profileName,buf);

		if(i==0)
		{
			tempprof.Tube_No_x=2;
			tempprof.Tube_No_y=2;
			tempprof.pitch_x=10;
			tempprof.pitch_y=10;
			tempprof.trayOriginX=10;
			tempprof.trayOriginY=10;
			tempprof.Cycles=2;
			tempprof.vibrationEnabled=TRUE;
			for(x=0;x<MAX_BUTTONS_X;x++)
				for(y=0;y<MAX_BUTTONS_Y;y++) tempprof.buttonStates[x][y]=TRUE;
		}
		else
		{
			tempprof.Tube_No_x=MAX_BUTTONS_X;
			tempprof.Tube_No_y=MAX_BUTTONS_Y;
			tempprof.pitch_x=10;
			tempprof.pitch_y=10;
			tempprof.trayOriginX=10;
			tempprof.trayOriginY=10;
			tempprof.Cycles=2;
			tempprof.vibrationEnabled=TRUE;
			for(x=0;x<MAX_BUTTONS_X;x++)
				for(y=0;y<MAX_BUTTONS_Y;y++) tempprof.buttonStates[x][y]=TRUE;
		}
		WriteProfileEEPROM(i,tempprof);
	}
	WriteCurIDEEPROM(0);//reset current profile in eeprom to 0
}

void CheckProfile(Profile &source)
{
	if(source.Cycles>MAXCYCLE) source.Cycles=MAXCYCLE;
	if(source.Cycles<0) source.Cycles=0;
	if(source.pitch_x>MAXPITCHX) source.pitch_x=MAXPITCHX;
	if(source.pitch_x<0) source.pitch_x=0;
	if(source.pitch_y>MAXPITCHY) source.pitch_y=MAXPITCHY;
	if(source.pitch_y<0) source.pitch_y=0;
	if(source.trayOriginX>MAXORGX) source.trayOriginX=MAXORGX;
	if(source.trayOriginX<0) source.trayOriginX=0;
	if(source.trayOriginY>MAXORGY) source.trayOriginY=MAXORGY;
	if(source.trayOriginY<0) source.trayOriginY=0;
	if(source.Tube_No_x>MAX_TUBES_X) source.Tube_No_x=MAX_TUBES_X;
	if(source.Tube_No_x<0) source.Tube_No_x=0;
	if(source.Tube_No_y>MAX_TUBES_Y) source.Tube_No_y=MAX_TUBES_Y;
	if(source.Tube_No_y<0) source.Tube_No_y=0;

}
uint8_t LoadProfile(void)
{
	uint8_t ret=0;

	ret=ReadCurIDEEPROM();
	if(ret>=MAX_PROFILES)  ret=0;// if corrcupt data from eeprom, set id=0
	
	ReadProfileEEPROM(ret,CurProf);//reread as it is not profile id 0
	if(CurProf.Cycles>MAXCYCLE) CurProf.Cycles=MAXCYCLE;
	if(CurProf.pitch_x>MAXPITCHX) CurProf.pitch_x=MAXPITCHX;
	if(CurProf.pitch_y>MAXPITCHY) CurProf.pitch_y=MAXPITCHY;
	if(CurProf.trayOriginX>MAXORGX) CurProf.trayOriginX=MAXORGX;
	if(CurProf.trayOriginY>MAXORGY) CurProf.trayOriginY=MAXORGY;
	if(CurProf.Tube_No_x>MAX_TUBES_X) CurProf.Tube_No_x=MAX_TUBES_X;
	if(CurProf.Tube_No_y>MAX_TUBES_Y) CurProf.Tube_No_y=MAX_TUBES_Y;
	return ret;
}


void WritePassEEPROM(char *pass)
{
	EEPROM.put(4000, *pass);
}

void ReadPassEEPROM(char *pass)
{
    EEPROM.get(4000, *pass);

}

void WriteCurIDEEPROM(uint8_t curprofid)
{
	EEPROM.put(0, curprofid);
}

uint8_t ReadCurIDEEPROM(void)
{
	uint8_t temp;

	EEPROM.get(0, temp);
	if(temp>MAX_PROFILES) temp=0;
	return temp;
}

void WriteProfileEEPROM(int address, Profile &profile)
{
	int x,y;
	byte dat;
	int bit;
	
	address=(address*sizeof(profile))+sizeof(uint8_t);
	
    //EEPROM.put(address, profile.profileId);
    //address += sizeof(profile.profileId);
    EEPROM.put(address, profile.profileName);
    address += sizeof(profile.profileName);
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
	//Dsprintln("size");
	//Dprintln(sizeof(profile));


	for (y = 0; y < MAX_BUTTONS_Y; y++)
	{
		for (x = 0; x < MAX_BUTTONS_X; x += 8)
		{
		    dat = 0;
            for (bit = 0; bit < 8 && (x + bit) < MAX_BUTTONS_X; ++bit)
            {
                if (profile.buttonStates[x+ bit][y])
                    dat |= (1 << bit);
            }

			EEPROM.put(address, dat);
			address += sizeof(dat);
		}
	}

}

void ReadProfileEEPROM(int address, Profile &profile)
{
	int x,y;
	byte dat;
	int bit;
	Dprint("profile size=", (float)sizeof(profile));
	address=(address*sizeof(profile))+ sizeof(uint8_t);

    //EEPROM.get(address, profile.profileId);
    //address += sizeof(profile.profileId);
    EEPROM.get(address, profile.profileName);
	Dprint(profile.profileName);
    address += sizeof(profile.profileName);

#if 1

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
#endif

    for (y = 0; y < MAX_BUTTONS_Y; y++)
    {
        for (x = 0; x < MAX_BUTTONS_X; x += 8)
        {
           	EEPROM.get(address, dat);
           	address += sizeof(dat);

            for (bit = 0; bit < 8 && (x + bit) < MAX_BUTTONS_X; ++bit)
            {
				//dat = 0;
                profile.buttonStates[x+ bit][y]= dat & (1 << bit);			
            }
		}
	}

  	CheckProfile(profile);

}
