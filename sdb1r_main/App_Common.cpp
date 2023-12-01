/*

Copyright (c) Bridgetek Pte Ltd
Copyright (c) Riverdi Sp. z o.o. sp. k.
Copyright (c) Lukasz Skalski <contact@lukasz-skalski.com>

THIS SOFTWARE IS PROVIDED BY BRIDGETEK PTE LTD "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
BRIDGETEK PTE LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

BRIDGETEK DRIVERS MAY BE USED ONLY IN CONJUNCTION WITH PRODUCTS BASED ON BRIDGETEK PARTS.

BRIDGETEK DRIVERS MAY BE DISTRIBUTED IN ANY FORM AS LONG AS LICENSE INFORMATION IS NOT MODIFIED.
F
IF A CUSTOM VENDOR ID AND/OR PRODUCT ID OR DESCRIPTION STRING ARE USED, IT IS THE
RESPONSIBILITY OF THE PRODUCT MANUFACTURER TO MAINTAIN ANY CHANGES AND SUBSEQUENT WHQL
RE-CERTIFICATION AS A RESULT OF MAKING THESE CHANGES.

Abstract:

Application to demonstrate function of EVE.

Author : Bridgetek
Modified by: XentiQ

Revision History:
0.1 - date 2017.03.24 - Initial version
0.2 - date 2022.12.14 - XentiQ version
*/

#include "Platform.h"
#include "App_Common.h"
#include "SaveProfile.h"
#include <EEPROM.h>
#include <AccelStepper.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
/*************************************************************************************************
 *                      These functions work with FT8XX GPU buffer
 **************************************************************************************************/

uint32_t CmdBuffer_Index;
volatile uint32_t DlBuffer_Index;
volatile uint16_t toggleState;

uint8_t font = 27;
uint8_t Line = 0;
uint16_t line2disp = 0;

const int STARTPOS = 270; // GUI screen coordinates


inline int32_t minimum(int32_t a, int32_t b) { return a < b ? a : b; }

struct
{
    uint8_t Key_Detect : 1;
    uint8_t Caps : 1;
    uint8_t Numeric : 1;
    uint8_t Exit : 1;
} Flag;

//struct Notepad_buffer
//{
//    char8_t *temp;
//    char8_t notepad[MAX_LINES + 1][800];
//} Buffer;

#ifdef BUFFER_OPTIMIZATION
uint8_t DlBuffer[DL_SIZE];
uint8_t CmdBuffer[CMD_FIFO_SIZE];
#endif

void App_WrCoCmd_Buffer(Gpu_Hal_Context_t *phost, uint32_t cmd)
{
#ifdef BUFFER_OPTIMIZATION
    /* Copy the command instruction into buffer */
    uint32_t *pBuffcmd;
    /* Prevent buffer overflow */
    if (CmdBuffer_Index >= CMD_FIFO_SIZE)
    {
        // printf("CmdBuffer overflow\n");

        if (CmdBuffer_Index > 0)
        {
            Gpu_Hal_WrCmdBuf_nowait(phost, CmdBuffer, CmdBuffer_Index);
        }
        CmdBuffer_Index = 0;
    }

    pBuffcmd = (uint32_t *)&CmdBuffer[CmdBuffer_Index];
    *pBuffcmd = cmd;

#endif

#if defined(STM32_PLATFORM) || defined(ARDUINO_PLATFORM)
    Gpu_Hal_WrCmd32(phost, cmd);
#endif
    /* Increment the command index */
    CmdBuffer_Index += CMD_SIZE;
}

void App_WrDl_Buffer(Gpu_Hal_Context_t *phost, uint32_t cmd)
{
#ifdef BUFFER_OPTIMIZATION
    /* Copy the command instruction into buffer */
    uint32_t *pBuffcmd;
    /* Prevent buffer overflow */
    if (DlBuffer_Index < DL_SIZE)
    {
        pBuffcmd = (uint32_t *)&DlBuffer[DlBuffer_Index];
        *pBuffcmd = cmd;
    }
    else
    {
        printf("DlBuffer overflow\n");
    }

#endif

#if defined(STM32_PLATFORM) || defined(ARDUINO_PLATFORM)
    Gpu_Hal_Wr32(phost, (RAM_DL + DlBuffer_Index), cmd);
#endif
    /* Increment the command index */
    DlBuffer_Index += CMD_SIZE;
}

void App_WrCoStr_Buffer(Gpu_Hal_Context_t *phost, const char8_t *s)
{
#ifdef BUFFER_OPTIMIZATION
    uint16_t length = 0;

    if (CmdBuffer_Index >= CMD_FIFO_SIZE)
    {
        printf("CmdBuffer overflow\n");

        if (CmdBuffer_Index > 0)
        {
            Gpu_Hal_WrCmdBuf(phost, CmdBuffer, CmdBuffer_Index); // This blocking state may be infinite due to Display list overflow
        }
        CmdBuffer_Index = 0;
    }

    length = strlen(s) + 1; // last for the null termination

    strcpy((char *)&CmdBuffer[CmdBuffer_Index], s);

    /* increment the length and align it by 4 bytes */
    CmdBuffer_Index += ((length + 3) & ~3);
#endif
}

void App_Flush_DL_Buffer(Gpu_Hal_Context_t *phost)
{
#ifdef BUFFER_OPTIMIZATION
    if (DlBuffer_Index > 0)
        Gpu_Hal_WrMem(phost, RAM_DL, DlBuffer, DlBuffer_Index);
#endif
    DlBuffer_Index = 0;
}

void App_Flush_Co_Buffer(Gpu_Hal_Context_t *phost)
{
#ifdef BUFFER_OPTIMIZATION
    if (CmdBuffer_Index > 0)
        Gpu_Hal_WrCmdBuf(phost, CmdBuffer, CmdBuffer_Index);
#endif
    CmdBuffer_Index = 0;
}

void App_Flush_Co_Buffer_nowait(Gpu_Hal_Context_t *phost)
{
#ifdef BUFFER_OPTIMIZATION
    if (CmdBuffer_Index > 0)
        Gpu_Hal_WrCmdBuf_nowait(phost, CmdBuffer, CmdBuffer_Index);
#endif
    CmdBuffer_Index = 0;
}

void App_Set_DlBuffer_Index(uint32_t index)
{
    DlBuffer_Index = index;
}

void App_Set_CmdBuffer_Index(uint32_t index)
{
    CmdBuffer_Index = index;
}
/*************************************************************************************************
 *                      Application Utilities
 **************************************************************************************************/

static uint8_t sk = 0;
uint8_t App_Read_Tag(Gpu_Hal_Context_t *phost)
{
    static uint8_t Read_tag = 0, temp_tag = 0, ret_tag = 0;
    Read_tag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
    ret_tag = 0;
    if (Read_tag != 0) // Allow if the Key is released
    {
        if (temp_tag != Read_tag)
        {
            temp_tag = Read_tag;
            sk = Read_tag; // Load the Read tag to temp variable
        }
    }
    else
    {
        if (temp_tag != 0)
        {
            ret_tag = temp_tag;
            temp_tag = 0; // The event will be processed. Clear the tag
        }
        sk = 0;
    }
    return ret_tag;
}

uint8_t App_Touch_Update(Gpu_Hal_Context_t *phost, uint8_t *currTag, uint16_t *x, uint16_t *y)
{
    static uint8_t Read_tag = 0, temp_tag = 0, ret_tag = 0;
    uint32_t touch;
    Read_tag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
    ret_tag = 0;
    if (Read_tag != 0) // Allow if the Key is released
    {
        if (temp_tag != Read_tag)
        {
            temp_tag = Read_tag;
            sk = Read_tag; // Load the Read tag to temp variable
        }
    }
    else
    {
        if (temp_tag != 0)
        {
            ret_tag = temp_tag;
            temp_tag = 0; // The event will be processed. Clear the tag
        }
        sk = 0;
    }
    *currTag = Read_tag;
    touch = Gpu_Hal_Rd32(phost, REG_TOUCH_SCREEN_XY);
    *x = (uint16_t)(touch >> 16);
    *y = (uint16_t)(touch & 0xFFFF);
    return ret_tag;
}
void App_Play_Sound(Gpu_Hal_Context_t *phost, uint8_t sound, uint8_t vol, uint8_t midi)
{
    uint16_t val = (midi << 8) | sound;
    Gpu_Hal_Wr8(phost, REG_VOL_SOUND, vol);
    Gpu_Hal_Wr16(phost, REG_SOUND, val);
    Gpu_Hal_Wr8(phost, REG_PLAY, 1);
}

void App_Calibrate_Screen(Gpu_Hal_Context_t *phost)
{
    Gpu_CoCmd_Dlstart(phost);
    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_Text(phost, DispWidth / 2, DispHeight / 2, 26, OPT_CENTERX | OPT_CENTERY, "Please tap on a dot");

    /* Calibration */
    Gpu_CoCmd_Calibrate(phost, 0);
    App_Flush_Co_Buffer(phost);
    Gpu_Hal_WaitCmdfifo_empty(phost);
}

void App_Common_Init(Gpu_Hal_Context_t *phost)
{
    Gpu_HalInit_t halinit;
    uint8_t chipid;

    Gpu_Hal_Init(&halinit);
    Gpu_Hal_Open(phost);

    Gpu_Hal_Powercycle(phost, TRUE);

    /* FT81x will be in SPI Single channel after POR
    If we are here with FT4222 in multi channel, then
    an explicit switch to single channel is essential
    */
#ifdef FT81X_ENABLE
    Gpu_Hal_SetSPI(phost, GPU_SPI_SINGLE_CHANNEL, GPU_SPI_ONEDUMMY);
#endif

    /* access address 0 to wake up the chip */
    Gpu_HostCommand(phost, GPU_ACTIVE_M);
    Gpu_Hal_Sleep(300);

    // Gpu_HostCommand(phost,GPU_INTERNAL_OSC);
    Gpu_HostCommand(phost, GPU_EXTERNAL_OSC); // Added for ips_35 display
    Gpu_Hal_Sleep(100);

    /* read Register ID to check if chip ID series is correct */
    chipid = Gpu_Hal_Rd8(phost, REG_ID);
    while (chipid != 0x7C)
    {
        chipid = Gpu_Hal_Rd8(phost, REG_ID);
        Gpu_Hal_Sleep(100);
    }

    /* read REG_CPURESET to confirm 0 is returned */
    {
        uint8_t engine_status;

        /* Read REG_CPURESET to check if engines are ready.
             Bit 0 for coprocessor engine,
             Bit 1 for touch engine,
             Bit 2 for audio engine.
        */
        engine_status = Gpu_Hal_Rd8(phost, REG_CPURESET);
        while (engine_status != 0x00)
        {
            engine_status = Gpu_Hal_Rd8(phost, REG_CPURESET);
            Gpu_Hal_Sleep(100);
        }
    }

    /* configuration of LCD display */
    Gpu_Hal_Wr16(phost, REG_HCYCLE, DispHCycle);
    Gpu_Hal_Wr16(phost, REG_HOFFSET, DispHOffset);
    Gpu_Hal_Wr16(phost, REG_HSYNC0, DispHSync0);
    Gpu_Hal_Wr16(phost, REG_HSYNC1, DispHSync1);
    Gpu_Hal_Wr16(phost, REG_VCYCLE, DispVCycle);
    Gpu_Hal_Wr16(phost, REG_VOFFSET, DispVOffset);
    Gpu_Hal_Wr16(phost, REG_VSYNC0, DispVSync0);
    Gpu_Hal_Wr16(phost, REG_VSYNC1, DispVSync1);
    Gpu_Hal_Wr8(phost, REG_SWIZZLE, DispSwizzle);
    Gpu_Hal_Wr8(phost, REG_PCLK_POL, DispPCLKPol);
    Gpu_Hal_Wr16(phost, REG_HSIZE, DispWidth);
    Gpu_Hal_Wr16(phost, REG_VSIZE, DispHeight);
    Gpu_Hal_Wr16(phost, REG_CSPREAD, DispCSpread);
    Gpu_Hal_Wr16(phost, REG_DITHER, DispDither);
    Gpu_Hal_Wr16(phost, REG_TOUCH_RZTHRESH, 1200);

    /* GPIO configuration */
#if defined(FT81X_ENABLE)
    Gpu_Hal_Wr16(phost, REG_GPIOX_DIR, 0xffff);
    Gpu_Hal_Wr16(phost, REG_GPIOX, 0xffff);
#else
    Gpu_Hal_Wr8(phost, REG_GPIO_DIR, 0xff);
    Gpu_Hal_Wr8(phost, REG_GPIO, 0xff);
#endif

    Gpu_ClearScreen(phost);

    /* after this display is visible on the LCD */
    Gpu_Hal_Wr8(phost, REG_PCLK, DispPCLK);

    phost->cmd_fifo_wp = Gpu_Hal_Rd16(phost, REG_CMD_WRITE);
}

void App_Common_Close(Gpu_Hal_Context_t *phost)
{
    Gpu_Hal_Close(phost);
    Gpu_Hal_DeInit();
}

/*************************************************************************************************
 *                                XentiQ Functions - 23JUN23
 *************************************************************************************************/

void Logo_XQ_trans(Gpu_Hal_Context_t *phost)
{
    Gpu_CoCmd_FlashFast(phost, 0);
    Gpu_CoCmd_Dlstart(phost);

    Gpu_Hal_WrCmd32(phost, CMD_INFLATE);
    Gpu_Hal_WrCmd32(phost, RAM_XQ_LOGO);
    Gpu_Hal_WrCmdBufFromFlash(phost, (uint8_t *)XQ_Logo_trans, sizeof(XQ_Logo_trans));

    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, END());
    App_WrCoCmd_Buffer(phost, BITMAP_HANDLE(0));
    Gpu_CoCmd_SetBitmap(phost, 0, RGB565, 100, 82);
    App_WrCoCmd_Buffer(phost, BEGIN(BITMAPS));
    App_WrCoCmd_Buffer(phost, PALETTE_SOURCE(0));
    App_WrCoCmd_Buffer(phost, VERTEX2F(1856, 1152));
    App_WrCoCmd_Buffer(phost, END());

    Disp_End(phost);
    Gpu_Hal_Sleep(800);
}


void SetMainMenuButton(uint8_t whichmenu)
{
	//					 	 SETTING START PAUSE STOP
	bool_t act_but[][6] = {	{1		,1		,0	,0}, //main menu
							{0		,0		,1	,1},//running menu
							{0		,1		,0	,1}, //pause menu
							{0		,0		,0	,1}, //error1 menu
							{0		,0		,0	,1}, //error2 menu
						};
	int i;
	char but[4]={SETTING,START,PAUSE,STOP};

	for (i=0;i<4;i++)
	{
		App_WrCoCmd_Buffer(phost, TAG(but[i]));

		if(act_but[whichmenu][i]) 
		{
			App_WrCoCmd_Buffer(phost, COLOR_A(255));
			App_WrCoCmd_Buffer(phost, TAG_MASK(1));
		}
		else
		{
			App_WrCoCmd_Buffer(phost, COLOR_A(60));
			App_WrCoCmd_Buffer(phost, TAG_MASK(0));
		}

		switch(but[i])
		{
			case START:		Gpu_CoCmd_FgColor(phost, 0x006400);
							if(whichmenu==PAUSEMENU)
							    Gpu_CoCmd_Button(phost, 25, 112, 90, 36, 28, 0, "RESUME");
							else
							    Gpu_CoCmd_Button(phost, 25, 112, 90, 36, 28, 0, "START");
							break;
			case PAUSE:		Gpu_CoCmd_FgColor(phost, 0xADAF3C);
						    Gpu_CoCmd_Button(phost, 207, 112, 90, 36, 28, 0, "PAUSE");
							break;
			case STOP:	    Gpu_CoCmd_FgColor(phost, 0xAA0000);
							if(whichmenu==TXRXERROR)
							{
								Gpu_CoCmd_Button(phost, 25, 157, 273, 36, 28, 0, "Communication error");
							} else if(whichmenu==HOMEERROR)
								{
								    Gpu_CoCmd_Button(phost, 25, 157, 273, 36, 28, 0, "Home error");
								}
								else Gpu_CoCmd_Button(phost, 25, 157, 273, 36, 28, 0, "STOP");
							break;
			case SETTING:   Gpu_CoCmd_FgColor(phost, 0x00A2E8);
						    Gpu_CoCmd_Button(phost, 208, 5, 90, 36, 28, OPT_FORMAT, "Settings");
		}
		App_WrCoCmd_Buffer(phost, TAG_MASK(0));
	}
}

void Home_Menu(Gpu_Hal_Context_t *phost, uint8_t whichmenu )
{

	int32_t filling_tube = 0;
	char buf[100];  

    Gpu_CoCmd_FlashFast(phost, 0);
    Gpu_CoCmd_Dlstart(phost);

    App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_FgColor(phost, 0x00A2E8);

	
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0));
//    Gpu_CoCmd_Text(phost, 160, 52, 31, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "XQ");
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(128, 128, 128));
    Gpu_CoCmd_Text(phost, 159, 80, 30, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "SDB-1R");
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(193, 64, 0));
    App_WrCoCmd_Buffer(phost, BEGIN(LINES));
    App_WrCoCmd_Buffer(phost, VERTEX2F(0, 1648));
    App_WrCoCmd_Buffer(phost, VERTEX2F(5120, 1648));
    App_WrCoCmd_Buffer(phost, END());
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    App_WrCoCmd_Buffer(phost, TAG_MASK(255));


	SetMainMenuButton(whichmenu); 
	
	sprintf(buf,"Profile Name: %s", CurProf.profileName);
    Gpu_CoCmd_Text(phost, 25, 195, 20,   OPT_FORMAT, buf);//OPT_CENTER | OPT_RIGHTX |
    sprintf(buf, "No. of Cycles: %d", CurProf.Cycles);
    Gpu_CoCmd_Text(phost, 25, 208, 20,   OPT_FORMAT, buf);

    filling_tube = CurProf.Tube_No_x * CurProf.Tube_No_y;

	sprintf(buf,"Filling tube: %d", filling_tube);
    Gpu_CoCmd_Text(phost, 25, 220, 20,  OPT_FORMAT, buf);

	sprintf(buf,"Current Tube : R%2d C%2d",CurY+1,CurX+1);
	Gpu_CoCmd_Text(phost, 292, 208, 20,   OPT_RIGHTX | OPT_FORMAT, buf);

	sprintf(buf,"Tube left : %3d",TotalTubeLeft);
	Gpu_CoCmd_Text(phost, 294, 220, 20,   OPT_RIGHTX | OPT_FORMAT, buf);

    Disp_End(phost);
}


void Disp_End(Gpu_Hal_Context_t *phost)
{
    App_WrCoCmd_Buffer(phost, DISPLAY());
    // swap the current display list with the new display list
    Gpu_CoCmd_Swap(phost);
    App_Flush_Co_Buffer(phost);
    Gpu_Hal_WaitCmdfifo_empty(phost);

    /* End of command sequence */
    // This code below will cause flickering on the Tray_Screen
    // Gpu_CoCmd_Dlstart(phost);
    // Gpu_Copro_SendCmd(phost, CMD_STOP);
    // App_Flush_Co_Buffer(phost);
    // Gpu_Hal_WaitCmdfifo_empty(phost);
}

void DisplayKeyboard(Gpu_Hal_Context_t *phost, uint8_t keypressed,char *displaytext,char *displaytitle, bool numlock,bool caplock, bool errorcode)
{
	char buf[PROFILE_NAME_MAX_LEN+8];

	// Display List start
	Gpu_CoCmd_Dlstart(phost);
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
	App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
	App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation

	Gpu_CoCmd_FgColor(phost, 0x703800);
	Gpu_CoCmd_BgColor(phost, 0x703800);
	
	Gpu_CoCmd_FgColor(phost, 0xAA0000);

	App_WrCoCmd_Buffer(phost, TAG(CLEAR_KEY));		 // Back		 Return to Home
	Gpu_CoCmd_Button(phost, (DispWidth * 0.855), (DispHeight * 0.83), (DispWidth * 0.146), (DispHeight * 0.112),
					 font, (keypressed==CLEAR_KEY)? OPT_FLAT : 0, "Clear");
	Gpu_CoCmd_FgColor(phost, 0x703800);


	Gpu_CoCmd_FgColor(phost, 0xADAF3C);
	App_WrCoCmd_Buffer(phost, TAG(BACK_SPACE)); // BackSpace
	Gpu_CoCmd_Button(phost, (DispWidth * 0.875), (DispHeight * 0.70), (DispWidth * 0.125),
					 (DispHeight * 0.112), font, (keypressed==BACK_SPACE)? OPT_FLAT : 0, "<-");
	Gpu_CoCmd_FgColor(phost, 0x703800);



	Gpu_CoCmd_FgColor(phost, 0x00a2e8);
    App_WrCoCmd_Buffer(phost, TAG(KBBACK)); // Back		 Return to Home
    Gpu_CoCmd_Button(phost, (DispWidth * 0.115), (DispHeight * 0.83), (DispWidth * 0.192), (DispHeight * 0.112),
                     font, (keypressed==KBBACK)? OPT_FLAT : 0, "Back");
	
	Gpu_CoCmd_FgColor(phost, 0x202020);//0x703800);
	App_WrCoCmd_Buffer(phost, TAG(' ')); // Space
	Gpu_CoCmd_Button(phost, (DispWidth * 0.315), (DispHeight * 0.83), (DispWidth * 0.33), (DispHeight * 0.112),
					 font, (keypressed==' ')? OPT_FLAT : 0, "Space");

    if (!numlock)
    {
        Gpu_CoCmd_Keys(phost, 0, (DispHeight * 0.442), DispWidth, (DispHeight * 0.112), font, keypressed,
                       ((caplock) ? "QWERTYUIOP" : "qwertyuiop"));
        Gpu_CoCmd_Keys(phost, (DispWidth * 0.042), (DispHeight * 0.57), (DispWidth * 0.96), (DispHeight * 0.112),
                       font, keypressed, ((caplock) ? "ASDFGHJKL" : "asdfghjkl"));
        Gpu_CoCmd_Keys(phost, (DispWidth * 0.125), (DispHeight * 0.70), (DispWidth * 0.73), (DispHeight * 0.112),
                       font, keypressed, ((caplock) ? "ZXCVBNM" : "zxcvbnm"));


        App_WrCoCmd_Buffer(phost, TAG(CAPS_LOCK)); // Capslock
        Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.70), (DispWidth * 0.10), (DispHeight * 0.112), font, 
						(keypressed==CAPS_LOCK)? OPT_FLAT : 0,
                         ((caplock) ?"a^" : "A^"));
        App_WrCoCmd_Buffer(phost, TAG(NUMBER_LOCK)); // Numberlock
        Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.83), (DispWidth * 0.10), (DispHeight * 0.112), font, 
						(keypressed==NUMBER_LOCK)? OPT_FLAT : 0,
                         "12*");
    }
	else
    {
        Gpu_CoCmd_Keys(phost, (DispWidth * 0), (DispHeight * 0.442), DispWidth, (DispHeight * 0.112), font,
                       keypressed, "1234567890");
        Gpu_CoCmd_Keys(phost, (DispWidth * 0.042), (DispHeight * 0.57), (DispWidth * 0.96), (DispHeight * 0.112),
                       font, keypressed, "-@#$%^&*(");
        Gpu_CoCmd_Keys(phost, (DispWidth * 0.125), (DispHeight * 0.70), (DispWidth * 0.73), (DispHeight * 0.112),
                       font, keypressed, ")_+[]{}");
        App_WrCoCmd_Buffer(phost, TAG(NUMBER_LOCK)); // Numberlock
        Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.83), (DispWidth * 0.10), (DispHeight * 0.112), font, 
						(keypressed==NUMBER_LOCK)? OPT_FLAT : 0,
                         "AB*");
    }
	Gpu_CoCmd_FgColor(phost, 0x006400);
	App_WrCoCmd_Buffer(phost, TAG(SAVE_KEY)); // Enter
	Gpu_CoCmd_Button(phost, (DispWidth * 0.653), (DispHeight * 0.83), (DispWidth * 0.192), (DispHeight * 0.112), font, 
					(keypressed==SAVE_KEY)? OPT_FLAT : 0, "Enter");
	Gpu_CoCmd_FgColor(phost, 0x703800);
	App_WrCoCmd_Buffer(phost, TAG_MASK(0)); // Disable the tag buffer updates
	App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 0));
	App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(DispWidth, (uint16_t)(DispHeight * 0.41)));
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
	App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255)); // Text Color

    if(errorcode)
	{
		sprintf(buf,"%s\n%s",displaytext,"error");
		Gpu_CoCmd_Text(phost, 0, 0, font, 0, buf);
    }
	else
		Gpu_CoCmd_Text(phost, 0, 0, font, 0, displaytext);

	App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 77));
	App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(DispWidth, (uint16_t)(DispHeight * 0.1)));
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
	App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 0));
	Gpu_CoCmd_Text(phost, 160 - (strlen(displaytitle)*5), 78, 27, 0, displaytitle);

	Disp_End(phost);

	
}
uint8_t GetKeyPressed(void)
{
	uint8_t tempkey[2];
	
	tempkey[0]=Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
	//delay(10);
	tempkey[1]=Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);

	return (tempkey[0] == tempkey[1])? tempkey[0] : 0;
}

void WaitKeyRelease(void)
{
	while(Gpu_Hal_Rd8(phost, REG_TOUCH_TAG)>0);
}

void Keyboard(Gpu_Hal_Context_t *phost,char *curtext,char *curtitle,bool password)
{
	uint8_t keypressed = 0;

    delay(200); // Added to create smooth transition between screen
    //phost = &host;
    /*local variables*/
    uint16_t noofchars = 0, line2disp = 0, nextline = 0;
    uint8_t font = 27;
	//char curtext[PROFILE_NAME_MAX_LEN]="hello";
	char buf[PROFILE_NAME_MAX_LEN+20];
	uint8_t i;
	uint8_t curpos=0;
	bool wait4key=TRUE;
	bool numlock=FALSE;
	bool caplock=FALSE;


	for(i=0;i<PROFILE_NAME_MAX_LEN;i++) buf[i]='\0';
    strcpy(buf, curtext);

	
    /*initial setup*/
    curpos = strlen(buf);                                   // starting pos
   	buf[curpos]=0;
    Flag.Numeric = OFF;                                         // Disable the numbers and spcial charaters

	DisplayKeyboard(phost, keypressed,buf,curtitle,numlock,caplock,0);
	//for(i=0;i<14;i++) Dprint(buf[i]);
	Dprint("end");

	do{
		keypressed = GetKeyPressed();
	
		if(keypressed>0)
		{
			//DisplayKeyboard(phost, keypressed,buf,numlock,caplock,0);
			WaitKeyRelease();

            switch (keypressed)
            {
                case BACK_SPACE:
                    if (curpos>0) // check in the line there is any characters are present, cursor not included
                    {
	                    Dprint("BKSP");
                        curpos--;                                                             // clear the character in the buffer
                        buf[curpos]=0;
						if(password) curtext[curpos]=0;
						Dprint(curpos);      
                    }
					keypressed=0;
                    break;

                case CAPS_LOCK:
					Dprint("cap");
					keypressed=0;
                    caplock=!caplock; // toggle the caps lock on when the key detect
                    break;

                case NUMBER_LOCK:
					Dprint("num");
					keypressed=0;
                    numlock=(numlock)? FALSE:TRUE; // toggle the number lock on when the key detect
                    break;
				case CLEAR_KEY: keypressed=0;
							curpos=0;
							buf[curpos]=0;
							if(password) curtext[curpos]=0;
							break;
				case KBBACK: 	keypressed=0;
								wait4key=FALSE;
								break;
					
				case SAVE_KEY:	keypressed=0;
								wait4key=FALSE;
								strcpy(curtext,buf);
								break;
				default:	if(curpos<PROFILE_NAME_MAX_LEN)
							{
								//Dprint(curpos);
								buf[curpos]=keypressed;
								if(password)
								{
									curtext[curpos]='*';
									curtext[curpos+1]=0;
								}
								buf[++curpos]=0;
								keypressed=0;
							}
							else 
							{
								DisplayKeyboard(phost, keypressed,buf,curtitle,numlock,caplock,TRUE);
								keypressed=0;
								delay(500);
							}
							break;
								
            }
			if(wait4key)
			{
				if(password)
					DisplayKeyboard(phost, keypressed,curtext,curtitle,numlock,caplock,0);
				else
					DisplayKeyboard(phost, keypressed,buf,curtitle,numlock,caplock,0);
			}
		}
   }while (wait4key);

}



void DisplayProfileMenu(Profile curprof, uint8_t keypressed, uint8_t curprofnum)
{
	char buf[100]; // a buffer to format your text before printing.

	Gpu_CoCmd_FlashFast(phost, 0);
	Gpu_CoCmd_Dlstart(phost);
	
	App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
	App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
	
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
	Gpu_CoCmd_Text(phost, 84, 16, 27, 0, "Profile Selector");
	
	Gpu_CoCmd_FgColor(phost, 0x00A2E8);
	App_WrCoCmd_Buffer(phost, TAG(PROFILEBACK));
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
	Gpu_CoCmd_Button(phost, 233, 11, 76, 26, 21, (keypressed==PROFILEBACK)? OPT_FLAT :0 , "Back");
	
	Gpu_CoCmd_FgColor(phost, 0x00A2E8);
	App_WrCoCmd_Buffer(phost, TAG(PROFILELOAD));
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
	Gpu_CoCmd_Button(phost, 8, 205, 76, 26, 21, (keypressed==PROFILELOAD)? OPT_FLAT :0 , "Load");
	
	Gpu_CoCmd_FgColor(phost, 0x00A2E8);
	App_WrCoCmd_Buffer(phost, TAG(PROFILEPASS)); //disable advanced button
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));		
	Gpu_CoCmd_Button(phost, 180, 205, 132, 26, 21, (keypressed==PROFILEPASS)? OPT_FLAT :0 , "Change Password");
	
	Gpu_CoCmd_FgColor(phost, 0x00A2E8);
	App_WrCoCmd_Buffer(phost, TAG(PROFILEUP));
	//App_WrCoCmd_Buffer(phost, TAG(350)); //disbale Up
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
	Gpu_CoCmd_Button(phost, 265, 76, 44, 29, 21, (keypressed==PROFILEUP)? OPT_FLAT :0 , "Up");
	
	Gpu_CoCmd_FgColor(phost, 0x00A2E8);
	App_WrCoCmd_Buffer(phost, TAG(PROFILEDOWN));// disbale Up and down
	//App_WrCoCmd_Buffer(phost, TAG(300)); //disbale Down and down
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
	Gpu_CoCmd_Button(phost, 265, 121, 44, 29, 21, (keypressed==PROFILEDOWN)? OPT_FLAT :0 , "Down");
	
	App_WrCoCmd_Buffer(phost, BEGIN(RECTS)); // Profile Name Field
	App_WrCoCmd_Buffer(phost, VERTEX2F(160, 1008));
	App_WrCoCmd_Buffer(phost, VERTEX2F(4944, 704));
	
	App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0));
	
	Gpu_CoCmd_Text(phost, 160, 54, 27, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, (const char *)curprof.profileName);
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 127));    

    // Print profileId
    sprintf(buf, "Profile ID:    %d", curprofnum+1);
    Gpu_CoCmd_Text(phost, 8, 65, 21, 0, buf);

    // // Print profileName
    // sprintf(buffer, "Profile Name: %s", curprof.profileName);
    // Gpu_CoCmd_Text(phost, 84, 56, 27, 0, buffer);

    // Print Tube_No_x
    sprintf(buf, "Columns:     %d", curprof.Tube_No_x);
    Gpu_CoCmd_Text(phost, 8, 80, 21, 0, buf);

    // Print Tube_No_y
    sprintf(buf, "Rows:         %d", curprof.Tube_No_y);
    Gpu_CoCmd_Text(phost, 8, 95, 21, 0, buf);

    // Print pitch_x
    dtostrf(curprof.pitch_x, 4, 1, buf);
    Gpu_CoCmd_Text(phost, 8, 110, 21, 0, "Pitch(Col):");
    Gpu_CoCmd_Text(phost, 82, 110, 21, 0, buf);

    // Print pitch_y
    dtostrf(curprof.pitch_y, 4, 1, buf);
    Gpu_CoCmd_Text(phost, 8, 125, 21, 0, "Pitch(Ros):");
    Gpu_CoCmd_Text(phost, 83, 125, 21, 0, buf);

    // Print trayOriginX
    dtostrf(curprof.trayOriginX, 4, 1, buf);
    Gpu_CoCmd_Text(phost, 8, 140, 21, 0, "OriginX:");
    Gpu_CoCmd_Text(phost, 80, 140, 21, 0, buf);

    // Print trayOriginY
    dtostrf(curprof.trayOriginY, 4, 1, buf);
    Gpu_CoCmd_Text(phost, 8, 155, 21, 0, "OriginY:");
    Gpu_CoCmd_Text(phost, 80, 155, 21, 0, buf);

    // Print Cycles
    sprintf(buf, "Cycles:       %d" , curprof.Cycles);
    Gpu_CoCmd_Text(phost, 8, 170, 21, 0, buf);

    // Print vibrationEnabled
    sprintf(buf, "Vibration:    %s", curprof.vibrationEnabled ? "True" : "False");
    Gpu_CoCmd_Text(phost, 8, 185, 21, 0, buf);

	
	Disp_End(phost);

}
void Profile_Menu(Gpu_Hal_Context_t *phost)
{
   	Profile selectprof;
	int8_t selectprofnum;

    uint8_t keypressed = 0;
	bool wait4key=TRUE;

	selectprofnum=CurProfNum;
	ReadProfileEEPROM(selectprofnum,selectprof);
	DisplayProfileMenu(selectprof,0,selectprofnum);

    do{
        keypressed = GetKeyPressed();
        if (keypressed >0 )
        {
        	DisplayProfileMenu(selectprof,keypressed,selectprofnum);
			WaitKeyRelease();

			switch(keypressed)
			{
				case PROFILEBACK: 	wait4key=FALSE;
									keypressed=0;
									break;
				case PROFILELOAD: wait4key=FALSE;
									keypressed=0;
									CurProfNum=selectprofnum;
									WriteCurIDEEPROM(selectprofnum);
									ReadProfileEEPROM(CurProfNum,CurProf);
									break;
				case PROFILEUP: keypressed=0;
								if(selectprofnum<MAX_PROFILES-1)
								{
									++selectprofnum;
								}
								else
								{
									selectprofnum=0;
								}
								ReadProfileEEPROM(selectprofnum,selectprof);
								DisplayProfileMenu(selectprof,keypressed,selectprofnum);
								break;
				case PROFILEDOWN: 	keypressed=0;
								if(selectprofnum>0)
								{
									--selectprofnum;
								}
								else
								{
									selectprofnum=MAX_PROFILES-1;
								}
								ReadProfileEEPROM(selectprofnum,selectprof);
								
								DisplayProfileMenu(selectprof,keypressed,selectprofnum);
								break;
				case PROFILEPASS: 								
								Password[2][0]=0;
								Keyboard(phost,Password[2],"Enter New Password",FALSE);
								if(Password[2][0]!=0)							
								{
									Password[3][0]=0;
									Keyboard(phost,Password[3],"Enter New Password again",FALSE);
									if(strcmp(Password[2],Password[3])==0)
									{
										strcpy(Password[1],Password[2]);
										WritePassEEPROM(Password[1]);
										Keyboard(phost,"Password changed","Press Back to continue", FALSE);
									}
									else
									{
										Keyboard(phost,"Error : Different password entered","Press Back to continue", FALSE);
									}
								}
								else
								{
									Keyboard(phost,"Error : No password entered","Press Back to continue", FALSE);
								}
								//keypressed=0;
								break;
				default : break;
				
			}
        }

		DisplayProfileMenu(selectprof,keypressed,selectprofnum);

    }while(wait4key);
}
void UpdateConfigSet(Gpu_Hal_Context_t *phost, Profile curprof)
{


}
	
void DisplayConfig(Gpu_Hal_Context_t *phost)
{

	int16_t vibstatus;
	char buf[PROFILE_NAME_MAX_LEN];

    Gpu_CoCmd_FlashFast(phost, 0);
    Gpu_CoCmd_Dlstart(phost);

    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_Text(phost, 138, 20, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Configuration Settings");
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(193, 64, 0));
    Gpu_CoCmd_FgColor(phost, 0x00A2E8);
    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
    App_WrCoCmd_Buffer(phost, TAG(6));
    Gpu_CoCmd_Button(phost, 233, 9, 76, 26, 21, 0, "Home");
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));
    Gpu_CoCmd_Text(phost, 41, 82, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "");
    Gpu_CoCmd_Text(phost, 124, 82, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Column");
    Gpu_CoCmd_Text(phost, 180, 82, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Row");
    Gpu_CoCmd_Text(phost, 255, 82, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Cycles:");
    Gpu_CoCmd_Text(phost, 46, 108, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "No. of Tube:");
    Gpu_CoCmd_Text(phost, 41, 142, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Pitch(mm):");
    Gpu_CoCmd_Text(phost, 262, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Vibration:");
    Gpu_CoCmd_Text(phost, 44, 176, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Origin(mm):");
	//	  App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
	App_WrCoCmd_Buffer(phost, BEGIN(RECTS));

	// profile name(edit name)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(13));
	App_WrCoCmd_Buffer(phost, VERTEX2F(160, 1008));
	App_WrCoCmd_Buffer(phost, VERTEX2F(4944, 704));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// No. of tube(column)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(14));
	App_WrCoCmd_Buffer(phost, VERTEX2F(1664, 1776));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2336, 1536));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// No. of tube(row)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(15));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2576, 1776));
	App_WrCoCmd_Buffer(phost, VERTEX2F(3248, 1536));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// pitch(row)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(16));
	App_WrCoCmd_Buffer(phost, VERTEX2F(1664, 2352));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2336, 2096));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// pitch(column)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(17));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2576, 2352));
	App_WrCoCmd_Buffer(phost, VERTEX2F(3248, 2096));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// origin(row)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(18));
	App_WrCoCmd_Buffer(phost, VERTEX2F(1664, 2928));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2336, 2624));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// origin(column)
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(19));
	App_WrCoCmd_Buffer(phost, VERTEX2F(2576, 2928));
	App_WrCoCmd_Buffer(phost, VERTEX2F(3248, 2624));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	// Cycles
	App_WrCoCmd_Buffer(phost, TAG_MASK(1));
	App_WrCoCmd_Buffer(phost, TAG(20));
	App_WrCoCmd_Buffer(phost, VERTEX2F(3744, 1776));
	App_WrCoCmd_Buffer(phost, VERTEX2F(4480, 1536));
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));

	App_WrCoCmd_Buffer(phost, END());

	App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0));
	Gpu_CoCmd_Text(phost, 150, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, (const char *)CurProf.profileName);

	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_BgColor(phost, 0x00A2E8);
    // Check the vibration status and set the foreground color accordingly
    vibstatus= CurProf.vibrationEnabled ? 0 : 65535;
    if (vibstatus == 0) {  
    Gpu_CoCmd_FgColor(phost, 0x007300);  // Green when ON
    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 128, 0)); //text color
    } else {
    Gpu_CoCmd_FgColor(phost, 0xFF0000);  // Red when OFF
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); //text color
    }

    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
    App_WrCoCmd_Buffer(phost, TAG(25));
    Gpu_CoCmd_Toggle(phost, 242, 169, 40, 21, OPT_FLAT | OPT_FORMAT, vibstatus, "On\xFFOff");
    Gpu_CoCmd_BgColor(phost, 0x00A2E8);
    Gpu_CoCmd_FgColor(phost,0x007300);
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0));

    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    sprintf(buf,"%d",CurProf.Tube_No_x);
    Gpu_CoCmd_Text(phost, 124, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);
    sprintf(buf,"%d",CurProf.Tube_No_y);
    Gpu_CoCmd_Text(phost, 182, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);

	dtostrf(CurProf.pitch_x,4,1,buf);
    Gpu_CoCmd_Text(phost, 124, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);
	
	dtostrf(CurProf.pitch_y,4,1,buf);
    Gpu_CoCmd_Text(phost, 182, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);

  	dtostrf(CurProf.trayOriginX,4,1,buf);
    Gpu_CoCmd_Text(phost, 125, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);

	dtostrf(CurProf.trayOriginY,4,1,buf);
	Gpu_CoCmd_Text(phost, 182, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);

	sprintf(buf,"%d",CurProf.Cycles);
    Gpu_CoCmd_Text(phost, 255, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, buf);

    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
    Gpu_CoCmd_FgColor(phost, 0x00A2E8);

    // Load Button
    //App_WrCoCmd_Buffer(phost, TAG_MASK(disableButtons ? 0 : 1)); // Enable or disable based on the flag
    App_WrCoCmd_Buffer(phost, TAG(11));
    Gpu_CoCmd_Button(phost, 116, 205, 76, 26, 21, 0, "Load");

    // Save Button
    //App_WrCoCmd_Buffer(phost, TAG_MASK(disableButtons ? 0 : 1)); // Enable or disable based on the flag
    App_WrCoCmd_Buffer(phost, TAG(12));
    Gpu_CoCmd_Button(phost, 8, 205, 76, 26, 21, 0, "Save");
 

	
	App_WrCoCmd_Buffer(phost, TAG(CONFIGADVANCE)); //disable advanced button	
	Gpu_CoCmd_Button(phost, 233, 205, 76, 26, 21, 0 , "Advanced");
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));
    Disp_End(phost);

}

void Round1Dec(float *x)
{
	char buf[PROFILE_NAME_MAX_LEN];

	dtostrf(*x,3,1,buf);
	*x=atof(buf);
}



void Config_Settings(Gpu_Hal_Context_t *phost)
{
	bool wait4key=TRUE;
	uint8_t keypressed;
	char buf[PROFILE_NAME_MAX_LEN];
	float maxval=0;
	int i;
	
	do{
		keypressed = GetKeyPressed();

		if(keypressed>0)
		{
			WaitKeyRelease();
			switch(keypressed)
			{
	    		case 6: // Home button
						Serial.println("Button Pressed: HOME");
						//delay(500);
						//while( Gpu_Hal_Rd32(phost, REG_TOUCH_SCREEN_XY) != 0x80008000);
						keypressed=0;
						wait4key=FALSE;
						break;
				case 11: // Load (Config Screen)
						 Serial.println("Button Pressed: LOAD");
						 keypressed=0;

						 if(strcmp(CurProf.profileName,"xqreset")==0) 
						 {//special mode to preload eeprom
							Dprint("write preset data to eeprom");
							if(CurProfNum==0)
							{						
								for(i=1;i<MAX_PROFILES;i++) 
								{
									sprintf(CurProf.profileName,"%s %d","Profile",i+1);
									WriteProfileEEPROM(i,CurProf);
								}
							}
							else PreLoadEEPROM();
							sprintf(CurProf.profileName,"EEprom reseted"); 
							DisplayConfig(phost);
							delay(3000);
							sprintf(CurProf.profileName,"Profile 1 "); 
							DisplayConfig(phost);
						 }
						 if(strcmp(CurProf.profileName,"xqver")==0) 
						 {//special mode to preload eeprom
							Dprint("show version");
							sprintf(CurProf.profileName,"version : %s",FWVER); 
							DisplayConfig(phost);
							delay(3000);
							sprintf(CurProf.profileName,"xqver "); 
							DisplayConfig(phost);
							
						 }
						 if(strcmp(CurProf.profileName,"xqhome")==0) 
						 {//special mode to preload eeprom
							Dprint("home");
							Homing();
							DisplayConfig(phost);
							
						 }
						 if(strcmp(CurProf.profileName,"xqblank")==0) 
						 {//special mode to preload eeprom
							Dprint("blankeeprom");
							BlankEEPROM();
							sprintf(CurProf.profileName,"EEprom blank"); 
							DisplayConfig(phost);
							delay(3000);
							sprintf(CurProf.profileName,"xqblank "); 
							DisplayConfig(phost);
							delay(3000);
							
						 }
						 Profile_Menu(&host);
						 DisplayConfig(phost);
						 //wait4key=FALSE;
						 break;
				case CONFIGADVANCE: // Advavanced button
		          		  Serial.println("Button Pressed: ADVANCED");
						  keypressed=0;
		            		//            confirmAdvanceSetting(phost);
 		            	  Tray_Screen(phost, CurProf);
						  //Dprint("returntray","\n");
						  DisplayConfig(phost);
						  //wait4key=FALSE;
		            	  break;
		        case 12:
		                   Serial.println("Button Pressed: SAVE");
						   keypressed=0;
						   Dprint("curprofnum=",CurProfNum);
						   WriteCurIDEEPROM(CurProfNum);
						   WriteProfileEEPROM(CurProfNum,CurProf);
						   strcpy(buf,CurProf.profileName); 
						   strcpy(CurProf.profileName,"Profile saved");
						   DisplayConfig(phost);
						   delay(3000);
						   strcpy(CurProf.profileName,buf); 
						   wait4key=FALSE;
						   break;
		        case 13:
				            Serial.println("Button Pressed: PROFILE");
							keypressed=0;
		            		Keyboard(phost,CurProf.profileName,"Enter Profile Name",FALSE);
							DisplayConfig(phost);
		    		        break;

		        case 14: //no. of Tubes row
						keypressed=0;
						maxval=(int)((MAXXMM-CurProf.trayOriginX)/CurProf.pitch_x)+1;
						if(maxval>MAXNUMX) maxval=MAXNUMX;
						
						Dprint("max val=",maxval);
						CurProf.Tube_No_x=Keypad(&host, CurProf.Tube_No_x, MINNUMX,maxval,FALSE);
						DisplayConfig(phost);
		                break;
		        case 15://no. of Tubes col
						keypressed=0;
						maxval=(int)((MAXYMM-CurProf.trayOriginY)/CurProf.pitch_y)+1;
						if(maxval>MAXNUMY) maxval=MAXNUMY;
						Dprint("max val=",maxval);
						CurProf.Tube_No_y=Keypad(&host, CurProf.Tube_No_y, MINNUMY,maxval,FALSE);
						DisplayConfig(phost);
		                break;
		        case 16://pitch row
						keypressed=0;
						if(CurProf.Tube_No_x==0) maxval=MAXXMM-CurProf.trayOriginX;
						else maxval=(MAXXMM-CurProf.trayOriginX)/(CurProf.Tube_No_x-1);
						Round1Dec(&maxval);
						Dprint("max val=",maxval);
						if(maxval>MAXPITCHX) maxval=MAXPITCHX;
						CurProf.pitch_x=Keypad(&host, CurProf.pitch_x, MINPITCHX,maxval,TRUE);
						DisplayConfig(phost);
		                break;
		        case 17://pitch col
						keypressed=0;
						if(CurProf.Tube_No_y==0) maxval=MAXYMM-CurProf.trayOriginY;
						else maxval=(MAXYMM-CurProf.trayOriginY)/(CurProf.Tube_No_y-1);
						Round1Dec(&maxval);
						if(maxval>MAXPITCHY) maxval=MAXPITCHY;
						Dprint("max val=",maxval);
						CurProf.pitch_y=Keypad(&host, CurProf.pitch_y, MINPITCHY,maxval,TRUE);
						DisplayConfig(phost);
		                break;
		        case 18://origin row
						keypressed=0;
						if(CurProf.Tube_No_x==0) maxval=MAXXMM;
						else maxval=MAXXMM-(CurProf.pitch_x*(CurProf.Tube_No_x-1));
						Round1Dec(&maxval);
						if(maxval>MAXORGX) maxval=MAXORGX;
						Dprint("max val=",maxval);
						CurProf.trayOriginX=Keypad(&host, CurProf.trayOriginX, 0,maxval,TRUE);
						DisplayConfig(phost);
		                break;
		        case 19://origin col
						keypressed=0;
						if(CurProf.Tube_No_y==0) maxval=MAXYMM;
						else maxval=MAXYMM-(CurProf.pitch_y*(CurProf.Tube_No_y-1));
						Round1Dec(&maxval);
						if(maxval>MAXORGY) maxval=MAXORGY;
						Dprint("max val=",maxval);
						CurProf.trayOriginY=Keypad(&host, CurProf.trayOriginY, 0, maxval,TRUE);
						DisplayConfig(phost);
		                break;
		        case 20:
						keypressed=0;
						CurProf.Cycles=Keypad(&host, CurProf.Cycles, MINCYCLE,MAXCYCLE,FALSE);
						DisplayConfig(phost);
		                break;

			    case 25:
				 		keypressed=0;
          		  	  	CurProf.vibrationEnabled = !CurProf.vibrationEnabled; // Toggle the state
						DisplayConfig(phost);
	            		break;
		    	//case 246: // No button for Advanced Setting
		        //	  			keypressed=0;
				//		  		DisplayConfig(phost);
		        //    			break;
		          
			    //case 247: // Yes button for Advanced Setting
				//		        keypressed=0;
			     //   		    Tray_Screen(phost, CurProf);
		        //	    		break;
				default: 	keypressed=0;
					  			break;
			}
			//if(wait4key) while( GetKeyPressed()>0);
		}
		
	 }while(wait4key);
}


void confirmAdvanceSetting(Gpu_Hal_Context_t *phost)
{
    Gpu_CoCmd_FlashFast(phost, 0);
    Gpu_CoCmd_Dlstart(phost);
    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));

    Gpu_CoCmd_Text(phost, 156, 35, 28, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Proceed Advanced Setting?");
    App_WrCoCmd_Buffer(phost, TAG(246));
    Gpu_CoCmd_Button(phost, 179, 120, 60, 30, 28, 0, "No");
    App_WrCoCmd_Buffer(phost, TAG(247));
    Gpu_CoCmd_Button(phost, 68, 120, 60, 30, 28, 0, "Yes");

    Disp_End(phost);
}
void DisplayKeypad(Gpu_Hal_Context_t *phost, int32_t keypressed,char* displaynum,int8_t errorcode)
{
	char buf[KEYPAD_MAX_LEN+20];

    Gpu_CoCmd_Dlstart(phost);
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation
	//Gpu_CoCmd_FgColor(phost, 0xB40000);     // 0x703800 0xD00000
  	//Gpu_CoCmd_BgColor(phost, 0xB40000);
	Gpu_CoCmd_FgColor(phost, 0x202020);
	Gpu_CoCmd_BgColor(phost, 0x202020);

	Gpu_CoCmd_FgColor(phost, 0x00a2e8);
    App_WrCoCmd_Buffer(phost, TAG(BACK)); // Back		 Return to Home
    Gpu_CoCmd_Button(phost, 207, 37, 67, 46, font, (keypressed==BACK)? OPT_FLAT :0 , "Back");

	Gpu_CoCmd_FgColor(phost, 0xADAF3C);
    App_WrCoCmd_Buffer(phost, TAG(BACK_SPACE)); // BackSpace
    Gpu_CoCmd_Button(phost, 207, 87, 67, 50, 31, (keypressed==BACK_SPACE)? OPT_FLAT :0 , "<-");

	Gpu_CoCmd_FgColor(phost, 0x006400);
    App_WrCoCmd_Buffer(phost, TAG(NUM_ENTER)); // Enter
    Gpu_CoCmd_Button(phost, 207, 141, 67, 90, font, (keypressed==NUM_ENTER)? OPT_FLAT :0 , "Enter");

	Gpu_CoCmd_FgColor(phost, 0x202020);
	Gpu_CoCmd_BgColor(phost, 0x202020);

    Gpu_CoCmd_Keys(phost, 47, 35, 150, 50, 29, keypressed, "789");
    Gpu_CoCmd_Keys(phost, 47, 88, 150, 50, 29, keypressed, "456");
    Gpu_CoCmd_Keys(phost, 47, 140, 150, 50, 29, keypressed, "123");
    Gpu_CoCmd_Keys(phost, 47, 191, 150, 40, 29, keypressed, "0.");

    
	App_WrCoCmd_Buffer(phost, TAG_MASK(0)); // Disable the tag buffer updates
	App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 0));
	App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(320, 34));
	
	App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0,0,0));
	App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255)); // Text Color

    //App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    if(errorcode)
		sprintf(buf,"%s  %s",displaynum,"out of range");
	else
		sprintf(buf,displaynum);
	Gpu_CoCmd_Text(phost, 0, 0, 30, 0, buf);

    App_WrCoCmd_Buffer(phost, DISPLAY());
    Gpu_CoCmd_Swap(phost);
    App_Flush_Co_Buffer(phost);

}

void LoadBuffer( char *buf, float curval)
{
	float temp;

	temp=(int8_t)curval;
	if(curval-temp>0)
	{
		dtostrf(curval,3,1,buf);
	}
	else
		sprintf(buf,"%d",(int16_t)curval);

}

float Keypad(Gpu_Hal_Context_t *phost, float curval, float minval, float maxval ,bool isfloat)
{
    phost = &host;
    uint8_t keypressed = 0;
	bool wait4key=TRUE;

	char buf[KEYPAD_MAX_LEN+1];
	int8_t curpos;
	float tempval;

	for(int i=0;i<10;i++) buf[i]=0;
	LoadBuffer(buf, curval);
	curpos=strlen(buf);
	buf[curpos]=0;

	Dprint("T112");
	Dprint(buf[0]);
	Dprint(buf[1]);
	Dprint(buf[2]);
	Dprint(buf[3]);
	Dprint(buf[4]);

        // Display List start
	DisplayKeypad(phost,keypressed,buf,0);
	Dprint(curpos);

	do{
		keypressed = GetKeyPressed();

		if(keypressed>0)
		{
			//Dprintln(keypressed);
			DisplayKeypad(phost,keypressed,buf,0);
			WaitKeyRelease();
			Dprint("T10");
			switch(keypressed)
			{
				case BACK_SPACE:    if(curpos>=0)
									{
										if(curpos>0) curpos--;
										buf[curpos]=0;									
										Dprint(curpos);
									}
									keypressed=0;
									Dprint("T11");
									break;
				case NUM_ENTER: keypressed=0;
								wait4key=FALSE;
								break;
				case BACK: 	keypressed=0;
							wait4key=FALSE;
							LoadBuffer(buf,curval);
							break;
				default: 	if(curpos<KEYPAD_MAX_LEN-1)
							{
								Dprint(curpos);
								buf[curpos]=keypressed;
								buf[++curpos]=0;
								keypressed=0;
							}
							else
							{//max entry
								DisplayKeypad(phost,keypressed,buf,1);
							}
							break;
									
									
			}
			Dprint(buf[0]);
			Dprint(buf[1]);
			Dprint(buf[2]);
			Dprint(buf[3]);
			Dprint(buf[4]);
			if(!wait4key)
			{
				tempval=atof(buf);
				Dprint("tempval=",tempval);
				if(tempval<minval || tempval>maxval)
				{//out of range error
					DisplayKeypad(phost,keypressed,buf,1);
					delay(2000);
					if(tempval>maxval) LoadBuffer(buf,maxval);
					else LoadBuffer(buf,minval);
					curpos=strlen(buf);
					DisplayKeypad(phost,keypressed,buf,0);
					wait4key=TRUE;//error re enter again
					
				}
				else
				{
					curval=tempval;//accept entry
					LoadBuffer(buf,curval);
					DisplayKeypad(phost,keypressed,buf,0);
				}
			}
			else 
			{
				DisplayKeypad(phost,keypressed,buf,0);
			}
			if(wait4key) WaitKeyRelease();
		}
		
	}while(wait4key);

	return curval;
}

void displayTextMatrixName(int startRow, int startCol)
{
    int id;

    if (startRow >= 1 && startRow <= 7)
    {
        id = (startCol <= 7) ? 1 : 2;
    }
    else if (startRow >= 8 && startRow <= 14)
    {
        id = (startCol <= 7) ? 3 : 4;
    }
    else
    { // startRow between 15 and 17
        id = (startCol <= 7) ? 5 : 6;
    }

    String matrixName;

    switch (id)
    {
    case 1:
        matrixName = "Matrix 1"; // R1-R7 C1-C7
        break;
    case 2:
        matrixName = "Matrix 2"; // R1-R7 C8-C13
        break;
    case 3:
        matrixName = "Matrix 3"; // R8-R14 C1-C7
        break;
    case 4:
        matrixName = "Matrix 4"; // R8-R14 C8-C13
        break;
    case 5:
        matrixName = "Matrix 5"; // R15-R17 C1-C7
        break;
    case 6:
        matrixName = "Matrix 6"; // R15-R17 C8-C13
        break;
    default:
        matrixName = "Invalid range";
    }

    Gpu_CoCmd_Text(phost, 280, 230, 20, OPT_CENTER, matrixName.c_str()); // Display matrix name
}
void toggleButton(Gpu_Hal_Context_t *phost, Profile &profile, int tag, int x, int y, int buttonSize, int row, int col, int i, int j)
{
    bool buttonState = profile.buttonStates[col - 1][row - 1];
	char buf[10];

    App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
    App_WrCoCmd_Buffer(phost, POINT_SIZE(buttonSize * 10));

	//Dprint("x=",x);
	//Dprint("y=",y);
	App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 127));
	if (i == 0) // First column on each page
	{
		sprintf(buf, "C%d",col);
		Gpu_CoCmd_Text(phost, x+20, y , 20, OPT_CENTER, buf);
	}
	if (j == 0) // First row on each page
	{
		sprintf(buf, "R%d",row);
		Gpu_CoCmd_Text(phost, x , y-18, 20, OPT_CENTER, buf);
	}

    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    if (buttonState)
    {
        // If button state is ON
        App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
        App_WrCoCmd_Buffer(phost, POINT_SIZE(200));//buttonSize * 10));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 128, 0)); // Display green circle
        App_WrCoCmd_Buffer(phost, TAG(tag));
        Gpu_CoCmd_Track(phost, x + 22 + buttonSize / 2, y - 18 + buttonSize / 2, 0, 0, tag); // Enable touch tracking for TAG
        App_WrCoCmd_Buffer(phost, VERTEX2II(x+22, y - 18, 0, 0));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Text(phost, x+22, y - 18, 20, OPT_CENTER, "ON");


    }
	else
    {
        // If button state is OFF
        App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
        App_WrCoCmd_Buffer(phost, POINT_SIZE(200));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(200, 0, 0)); // Display red circle
        App_WrCoCmd_Buffer(phost, TAG(tag));
        Gpu_CoCmd_Track(phost, x + 22 + buttonSize / 2, y -18 + buttonSize / 2, 0, 0, tag); // Enable touch tracking for TAG
        App_WrCoCmd_Buffer(phost, VERTEX2II(x+22, y - 18, 0, 0));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Text(phost, x+22, y - 18, 20, OPT_CENTER, "OFF");
    }



    App_WrCoCmd_Buffer(phost, END());
}

void Tray_Screen(Gpu_Hal_Context_t *phost, Profile &profile)
{
	char buf[10];
	bool showonce=FALSE;
	uint8_t touch_tag;


	int32_t startRow = 1;
	int32_t startCol = 1;


    profile.Tube_No_x = min(profile.Tube_No_x, 13);
    profile.Tube_No_y = min(profile.Tube_No_y, 17);

    int32_t totalNumButtons = profile.Tube_No_x * profile.Tube_No_y;

    const int maxRowsPerPage = 7;
    const int maxColsPerPage = 7;


    int numRows = minimum(minimum(profile.Tube_No_y, maxRowsPerPage), 17);
    int numCols = minimum(minimum(profile.Tube_No_x, maxColsPerPage), 13);

    const int numButtonsPerPage = numRows * numCols;

    int32_t numPages = (totalNumButtons + numButtonsPerPage - 1) / numButtonsPerPage;
    int32_t currentPage = 0;

    int currentRowPage = 0;
    int currentColPage = 0;

    // Calculate button size and gaps once and use them on subsequent pages to mantain the same buttonsize across all pages
    int32_t buttonSize = 20;//min((240 - ((numCols + 1) * 16)) / numCols, (240 - ((numRows + 1) * 16)) / numRows);
    int32_t rowGap = 12;//(240 - (numRows * buttonSize)) / (numRows + 1);
    int32_t colGap = 12;//(240 - (numCols * buttonSize)) / (numCols + 1);

	
	
    // Calculate starting position for buttons
    int32_t startX = 10;//26;//(STARTPOS - ((numCols * (buttonSize + colGap)) - colGap)) / 2;
    int32_t startY = 230;//26;// (STARTPOS - ((numRows * (buttonSize + rowGap)) - rowGap)) / 2;



    while (1)
    {
        touch_tag = GetKeyPressed();

        Gpu_CoCmd_FlashFast(phost, 0);
        Gpu_CoCmd_Dlstart(phost);

        App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation

        // Draw a rectangle for the button area
        Gpu_CoCmd_FgColor(phost, 0xE5DCD9);
        Gpu_CoCmd_BgColor(phost, 0xE5DCD9);
        App_WrCoCmd_Buffer(phost, LINE_WIDTH(16));
        App_WrCoCmd_Buffer(phost, BEGIN(RECTS));
        App_WrCoCmd_Buffer(phost, VERTEX2F(0, 0));
        //App_WrCoCmd_Buffer(phost, VERTEX2F(3840, 3840));
        App_WrCoCmd_Buffer(phost, END());

        //Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        //App_WrCoCmd_Buffer(phost, TAG(11));
        //App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        //Gpu_CoCmd_Button(phost, 248, 90, 62, 26, 21, 0, "Load"); // Tray Load button

        //Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        //App_WrCoCmd_Buffer(phost, TAG(21));
        //App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        //Gpu_CoCmd_Button(phost, 248, 46, 62, 26, 21, 0, "Save"); // Tray Save button

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(22));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 0, 62, 26, 21, 0, "Back"); // Tray Home button

        // Draw Left navigation buttons
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(23));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 160, 25, 25, 24, (touch_tag==23)? OPT_FLAT :0 , "<"); // Previous page button

        // Draw right navigation buttons
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(24));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 288, 160, 25, 25, 24, (touch_tag==24)? OPT_FLAT :0, ">"); // Next page button

        // Draw up navigation button
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(26));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 268, 130, 25, 25, 24, (touch_tag==26)? OPT_FLAT :0, "^");

        // Draw down navigation button
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(27));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 268, 190, 25, 25, 24, (touch_tag==27)? OPT_FLAT :0, "v");

        // Calculate starting index and ending index for current page
        int startIndex = currentPage * numButtonsPerPage;
        // int endIndex = min(startIndex + numButtonsPerPage, totalNumButtons);// (optional to detect the last index)

        int currentPageRows = minimum(profile.Tube_No_y, numRows);
        int currentPageCols = minimum(profile.Tube_No_x, numCols);

        // Draw buttons for current page
        int x =startX;
        int y =startY;

        int tag = TAG_START + startIndex; // Start tag at 28 (currentPage * numButtonsPerPage)
        for (int i = 0; i < currentPageRows; i++)
        {
            for (int j = 0; j < currentPageCols; j++, tag++)
            {
                int row = startRow + i;
                int col = startCol + j;
                if (col <= profile.Tube_No_x && row <= profile.Tube_No_y) // Add this line to prevent drawing buttons beyond column 13
                {
                    if (touch_tag == tag)
                    {
                        profile.buttonStates[col - 1][row - 1] = !profile.buttonStates[col - 1][row - 1];
                        Serial.println("Button at (" + String(row) + ", " + String(col) + ") is now " + (profile.buttonStates[col - 1][row - 1] ? "OFF" : "ON"));
                    }
                    toggleButton(phost, profile, tag, x, y, buttonSize, row, col, i, j);
                }
                x += buttonSize + colGap;
            }
            x = startX;
            y -= buttonSize + rowGap;
        }
        displayTextMatrixName(startRow,startCol);

 		if (touch_tag == 22) // Home button
        {
            //break;
            return;
        }

        else if (touch_tag == 24) // Next page button
        {
        	//Dprint(">","\n");


            if (currentColPage < ((profile.Tube_No_x / maxColsPerPage) - ((profile.Tube_No_x % maxColsPerPage) ? 0 : 1)))
			{
				Dprint("cin");
                currentColPage++;
                startCol = currentColPage * numCols + 1;
            }
			delay(50);
			//Dprint("curcolpage=",currentColPage);
        }

        else if (touch_tag == 23) // Previous page button
        {
        	//Dprint("<","\n");

            if (currentColPage > 0)
            {
                currentColPage--;
                startCol = currentColPage * numCols + 1;
            }
			delay(50);
			//Dprint("curcolpage=",currentColPage);
        }

        else if (touch_tag == 27) // Up button
        {
        	//Dprint("^","\n");


            if (currentRowPage > 0)
            {
                currentRowPage--;
                startRow = currentRowPage * numRows + 1;
            }
			delay(50);
			//Dprint("currowpage=",currentRowPage);
        }

        else if (touch_tag == 26) // Down button
        {
        	//Dprint("v","\n");

            if (currentRowPage < ((profile.Tube_No_y / maxRowsPerPage) - ((profile.Tube_No_y % maxRowsPerPage) ? 0 : 1))) // Change here
            {
        				Dprint("rin");
                currentRowPage++;
                startRow = currentRowPage * numRows + 1;
            }
			delay(50);
			//Dprint("currowpage=",currentRowPage);
        }

        if (currentPage == numPages - 1)
        { // last page
            currentPageRows = totalNumButtons % numRows;
            currentPageCols = totalNumButtons % numCols;
        }
        else
        {
            currentPageRows = numRows;
            currentPageCols = numCols;
        }

        Disp_End(phost);
		if(touch_tag !=0) WaitKeyRelease();//delay(100);
    }
}
