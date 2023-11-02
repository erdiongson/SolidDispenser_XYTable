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

int32_t tempvarnum;
int32_t tempvarx;
int32_t tempvary;
int32_t tempvarx_inc;
int32_t tempvary_inc;
int32_t cycle_var;
int32_t PageNum;
uint16_t But_opt;
uint8_t font = 27;
int32_t Read_sfk = 0;
uint16_t back_flag = 0;
uint8_t Line = 0;
uint16_t line2disp = 0;

int tube_state = 0;
uint8_t buttonTag = 255;
const int STARTPOS = 255; // GUI screen coordinates
int32_t startRow = 1;
int32_t startCol = 1;

inline int32_t minimum(int32_t a, int32_t b) { return a < b ? a : b; }

struct
{
    uint8_t Key_Detect : 1;
    uint8_t Caps : 1;
    uint8_t Numeric : 1;
    uint8_t Exit : 1;
} Flag;

struct Notepad_buffer
{
    char8_t *temp;
    char8_t notepad[MAX_LINES + 1][800];
} Buffer;

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

//soon B
void Activate_But(uint8_t whichmenu, uint8_t but)
{
	//					 	 NONE NONE SETTING START PAUSE STOP
	bool_t act_but[][6] = {	{1,		1,	1		,1		,0	,0}, //main menu
							{1,		1,	0		,0		,1	,1},//running menu
							{1,		1,	0		,1		,0	,1}, //pause menu
						};

	App_WrCoCmd_Buffer(phost, TAG(but));

	if(act_but[whichmenu][but]) 
	{
		App_WrCoCmd_Buffer(phost, COLOR_A(255));//soon
		App_WrCoCmd_Buffer(phost, TAG_MASK(1));//soon
	}
	else
	{
		App_WrCoCmd_Buffer(phost, COLOR_A(60));//soon
		App_WrCoCmd_Buffer(phost, TAG_MASK(0));//soon
	}



	switch(but)
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
					    Gpu_CoCmd_Button(phost, 25, 157, 273, 36, 28, 0, "STOP");
						break;
		case SETTING:   Gpu_CoCmd_FgColor(phost, 0x00A2E8);
					    Gpu_CoCmd_Button(phost, 208, 5, 90, 36, 28, OPT_FORMAT, "Settings");
	}
	App_WrCoCmd_Buffer(phost, TAG_MASK(0));
}

void Home_Menu(Gpu_Hal_Context_t *phost, Profile &profile, uint8_t whichmenu)
{
    operating = 1;
    parameter = 0;
    count = 0;
    stCurrentLen = 0;
    memset(stCurrent, 0, sizeof(stCurrent));
    PageNum = 0;
	

    uint16_t But_opt1;
    uint16_t Read_sfk = 0;

    int32_t filling_tube = 0;

    Read_sfk = Read_Keypad();

    Gpu_CoCmd_FlashFast(phost, 0);
    Gpu_CoCmd_Dlstart(phost);

    App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
    App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
    App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_FgColor(phost, 0x00A2E8);
    App_WrCoCmd_Buffer(phost, TAG(2));
	Activate_But(whichmenu, SETTING); //soon
	

    Gpu_CoCmd_Button(phost, 208, 5, 90, 36, 28, OPT_FORMAT, "Settings");
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


	Activate_But(whichmenu, START); //soon
	Activate_But(whichmenu, PAUSE); //soon
	Activate_But(whichmenu, STOP); //soon

    Gpu_CoCmd_Text(phost, 52, 201, 20, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Profile ID: %d", profile.profileId);
    Gpu_CoCmd_Text(phost, 62, 214, 20, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "No. of Cycles: %d", profile.Cycles);

    filling_tube = profile.Tube_No_x * profile.Tube_No_y;

    Gpu_CoCmd_Text(phost, 60, 228, 20, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Filling tube: %d", filling_tube);


    Disp_End(phost);
}

//soon E


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

void renderButtons(Gpu_Hal_Context_t *phost, int Read_sfk, int font)
{
    uint32_t But_opt;

    But_opt = (Read_sfk == BACK) ? OPT_FLAT : 0; // button color change if the button during press
    App_WrCoCmd_Buffer(phost, TAG(BACK));        // Back		 Return to Home
    Gpu_CoCmd_Button(phost, 207, 112, 67, 50, font, But_opt, "Back");

    But_opt = (Read_sfk == BACK_SPACE) ? OPT_FLAT : 0;
    App_WrCoCmd_Buffer(phost, TAG(BACK_SPACE)); // BackSpace
    Gpu_CoCmd_Button(phost, 207, 181, 67, 50, font, But_opt, "Del");

    But_opt = (Read_sfk == NUMBER_LOCK) ? OPT_FLAT : 0;
    App_WrCoCmd_Buffer(phost, TAG(253)); // Enter
    Gpu_CoCmd_Button(phost, 207, 36, 67, 50, font, But_opt, "Enter");
}

bool KeyPassCode(Gpu_Hal_Context_t *phost, Profile &profile, ScreenType screenType)
{
    PageNum = 3;
    phost = &host;
    /*local variables*/
    uint8_t Line = 0;
    uint16_t Disp_pos = 0;
    int32_t Read_sfk = 0, tval;
    uint16_t noofchars = 0, line2disp = 0, nextline = 0;

    // Clear Linebuffer
    for (tval = 0; tval < MAX_LINES; tval++)
        memset(&Buffer.notepad[tval], '\0', sizeof(Buffer.notepad[tval]));

    /*intial setup*/
    Line = 0;                                                   // Starting line
    Disp_pos = LINE_STARTPOS;                                   // starting pos
    Flag.Numeric = ON;                                          // Disable the numbers and spcial charaters
    memset((Buffer.notepad[Line] + 0), '_', 1);                 // For Cursor
    Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][0], Font); // Update the Disp_Pos
    noofchars += 1;                                             // for cursor
                                                                /*enter*/
    Flag.Exit = 0;

    do
    {
        Read_sfk = Read_Keypad(); // read the keys

        if (Flag.Key_Detect)
        {                        // check if key is pressed
            Flag.Key_Detect = 0; // clear it
            if (Read_sfk >= SPECIAL_FUN)
            { // check any special function keys are pressed
                switch (Read_sfk)
                {
                case BACK_SPACE:
                    if (noofchars > 1) // check in the line there is any characters are present,cursor not include
                    {
                        noofchars -= 1;                                                             // clear the character inthe buffer
                        Disp_pos -= Gpu_Rom_Font_WH(*(Buffer.notepad[Line] + noofchars - 1), Font); // Update the Disp_Pos
                    }
                    else
                    {
                        if (Line >= (MAX_LINES - 1))
                            Line--;
                        else
                            Line = 0;                                                      // check the lines
                        noofchars = strlen(Buffer.notepad[Line]);                          // Read the len of the line
                        for (tval = 0; tval < noofchars; tval++)                           // Compute the length of the Line
                            Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                    }
                    Buffer.temp = (Buffer.notepad[Line] + noofchars); // load into temporary buffer
                    Buffer.temp[-1] = '_';                            // update the string
                    Buffer.temp[0] = '\0';
                    break;
                }
            }
            else
            {
                Disp_pos += Gpu_Rom_Font_WH(Read_sfk, Font);                       // update dispos Font
                Buffer.temp = Buffer.notepad[Line] + strlen(Buffer.notepad[Line]); // load into temporary buffer
                Buffer.temp[-1] = Read_sfk;                                        // update the string
                Buffer.temp[0] = '_';
                Buffer.temp[1] = '\0';
                noofchars = strlen(Buffer.notepad[Line]); // get the string len
                if (Disp_pos > LINE_ENDPOS)               // check if there is place to put a character in a specific line
                {
                    Buffer.temp = Buffer.notepad[Line] + (strlen(Buffer.notepad[Line]) - 1);
                    Buffer.temp[0] = '\0';
                    noofchars -= 1;
                    Disp_pos = LINE_STARTPOS;
                    Line++;
                    if (Line >= MAX_LINES)
                        Line = 0;
                    memset((Buffer.notepad[Line]), '\0', sizeof(Buffer.notepad[Line])); // Clear the line buffer
                    for (; noofchars >= 1; noofchars--, tval++)
                    {
                        if (Buffer.notepad[Line - 1][noofchars] == ' ' || Buffer.notepad[Line - 1][noofchars] == '.') // In case of space(New String) or end of statement(.)
                        {
                            memset(Buffer.notepad[Line - 1] + noofchars, '\0', 1);
                            noofchars += 1; // Include the space
                            memcpy(&Buffer.notepad[Line], (Buffer.notepad[Line - 1] + noofchars), tval);
                            break;
                        }
                    }
                    noofchars = strlen(Buffer.notepad[Line]);
                    Buffer.temp = Buffer.notepad[Line] + noofchars;
                    Buffer.temp[0] = '_';
                    Buffer.temp[1] = '\0';
                    for (tval = 0; tval < noofchars; tval++)
                        Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                }
            }
        }
        // Display List start
        Gpu_CoCmd_Dlstart(phost);
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation
        Gpu_CoCmd_FgColor(phost, 0xB40000);     // 0x703800 0xD00000
        Gpu_CoCmd_BgColor(phost, 0xB40000);

        But_opt = (Read_sfk == BACK) ? OPT_FLAT : 0; // button color change if the button during press

        if (Read_sfk == BACK && screenType != HOME_SCREEN)
        {
            Home_Menu(&host, profile,MAINMENU); //soon
            break; // Without the break the screen will flicker back to Home and key pass code.
        }
        renderButtons(phost, Read_sfk, font);

        if (Read_sfk == 253)
        {
            const char *correctPasscode = "1234"; // Replace with your desired passcode
            char enteredPasscode[MAX_CHAR_PER_LINE];
            strncpy(enteredPasscode, Buffer.notepad[0], MAX_CHAR_PER_LINE - 1);
            enteredPasscode[MAX_CHAR_PER_LINE - 1] = '\0';
            enteredPasscode[strlen(enteredPasscode) - 1] = '\0'; // Remove the underscore

            if (checkPasscode(phost, enteredPasscode, correctPasscode))
            {
                switch (screenType)
                {
                case HOME_SCREEN:
                    return true; // Will continue the Home_Menu in the xy_init
                    break;
                case TRAY_SCREEN:
                    Tray_Screen(phost, profile);
                    break;
                }
                return false; // Without this then the Home button in the Tray_Screen will not return to Home Menu
            }
            else
            {
                Gpu_CoCmd_Text(phost, 100, 100, font, OPT_CENTER, "Try again");
                App_WrCoCmd_Buffer(phost, DISPLAY());
                Gpu_CoCmd_Swap(phost);
                App_Flush_Co_Buffer(phost);
                delay(1000); // Show the message for 1 second

                // Redraw the screen with buttons
                Gpu_CoCmd_Dlstart(phost);
                App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
                App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
                App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
                renderButtons(phost, Read_sfk, font);
                App_WrCoCmd_Buffer(phost, DISPLAY());
                Gpu_CoCmd_Swap(phost);
                App_Flush_Co_Buffer(phost);
            }
        }

        if (Flag.Numeric == ON)
        {
            Gpu_CoCmd_Keys(phost, 47, 35, 150, 50, 29, Read_sfk, "789");
            Gpu_CoCmd_Keys(phost, 47, 88, 150, 50, 29, Read_sfk, "456");
            Gpu_CoCmd_Keys(phost, 47, 140, 150, 50, 29, Read_sfk, "123");
            Gpu_CoCmd_Keys(phost, 47, 191, 150, 40, 29, Read_sfk, "0#");
        }
        App_WrCoCmd_Buffer(phost, TAG_MASK(0)); // Disable the tag buffer updates
        App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 0));
        App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(320, 25));

        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Text Color
        line2disp = 0;
        while (line2disp <= Line)
        {
            // nextline = 3 + (line2disp * (DispHeight * .073));
            Gpu_CoCmd_Text(phost, line2disp, nextline, font, 0, (const char *)&Buffer.notepad[line2disp]);
            line2disp++;
        }
        App_WrCoCmd_Buffer(phost, DISPLAY());
        Gpu_CoCmd_Swap(phost);
        App_Flush_Co_Buffer(phost);
    } while (1);
}

uint8_t Read_Keypad()
{
    static uint8_t Read_tag = 0, temp_tag = 0, touch_detect = 1; // ret_tag = 0,
    Read_tag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
    // ret_tag = 0;
    if (istouch() == 0)
        touch_detect = 0;
    if (Read_tag != 0) // Allow if the Key is released
    {
        if (temp_tag != Read_tag && touch_detect == 0)
        {
            temp_tag = Read_tag; // Load the Read tag to temp variable
            touch_detect = 1;
        }
    }
    else
    {
        if (temp_tag != 0)
        {
            Flag.Key_Detect = 1;
            Read_tag = temp_tag;
        }
        temp_tag = 0;
    }
    return Read_tag;
}

uchar8_t istouch()
{
    return !(Gpu_Hal_Rd16(phost, REG_TOUCH_RAW_XY) & 0x8000);
}

uint8_t Gpu_Rom_Font_WH(uint8_t Char, uint8_t font)
{
    uint32_t ptr, Wptr;
    uint8_t Width = 0;

    ptr = Gpu_Hal_Rd8(phost, ROMFONT_TABLEADDRESS);
    // read Width of the character
    Wptr = (ptr + (148 * (font - 16))) + Char; // (table starts at font 16)
    Width = Gpu_Hal_Rd8(phost, Wptr);
    return Width;
}

void AlphaNumeric(Gpu_Hal_Context_t *phost, int32_t tube_no_x, int32_t tube_no_y, float pitch_row_x, float pitch_col_y, float trayOriginX_row, float trayOriginY_col, int32_t cyclesNo, Profile &profile)
{
    delay(50); // Added to create smooth transition between screen
    phost = &host;
    /*local variables*/
    uint8_t Line = 0;
    uint16_t Disp_pos = 0, But_opt;
    uint16_t Read_sfk = 0, tval;
    uint16_t noofchars = 0, line2disp = 0, nextline = 0;
    uint8_t font = 27;

    // Clear Linebuffer
    for (tval = 0; tval < MAX_LINES; tval++)
        memset(&Buffer.notepad[tval], '\0', sizeof(Buffer.notepad[tval]));

    /*initial setup*/
    Line = 0;                                                   // Starting line
    Disp_pos = LINE_STARTPOS;                                   // starting pos
    Flag.Numeric = OFF;                                         // Disable the numbers and spcial charaters
    memset((Buffer.notepad[Line] + 0), '\0', 1);                // Initialize Null character
    Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][0], Font); // Update the Disp_Pos
    noofchars += 1;                                             // for cursor
                                                                /*enter*/
    Flag.Exit = 0;
    do
    {
        Read_sfk = Read_Keypad(); // read the keys

        if (Flag.Key_Detect)
        {                        // check if key is pressed
            Flag.Key_Detect = 0; // clear it
            if (Read_sfk >= SPECIAL_FUN)
            { // check any special function keys are pressed
                switch (Read_sfk)
                {
                case BACK_SPACE:
                    if (noofchars > 1) // check in the line there is any characters are present, cursor not included
                    {
                        noofchars -= 1;                                                             // clear the character in the buffer
                        Disp_pos -= Gpu_Rom_Font_WH(*(Buffer.notepad[Line] + noofchars - 1), Font); // Update the Disp_Pos
                    }
                    else
                    {
                        if (Line >= (MAX_LINES - 1))
                            Line--;
                        else
                            Line = 0;                                                      // check the lines
                        noofchars = strlen(Buffer.notepad[Line]);                          // Read the len of the line
                        for (tval = 0; tval < noofchars; tval++)                           // Compute the length of the Line
                            Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                    }
                    Buffer.temp = (Buffer.notepad[Line] + noofchars); // load into temporary buffer
                    if (Buffer.temp > Buffer.notepad[Line])           // check if temp is not pointing to the first element
                    {
                        Buffer.temp[-1] = ' '; // update the string
                    }
                    Buffer.temp[0] = '\0';
                    break;

                case CAPS_LOCK:
                    Flag.Caps ^= 1; // toggle the caps lock on when the key detect
                    break;

                case NUMBER_LOCK:
                    Flag.Numeric ^= 1; // toggle the number lock on when the key detect
                    break;

                case BACK:
                    for (tval = 0; tval < MAX_LINES; tval++)
                        memset(&Buffer.notepad[tval], '\0', sizeof(Buffer.notepad[tval]));

                    // Line = 0;                                                   // Starting line
                    // Disp_pos = LINE_STARTPOS;                                   // starting pos
                    // memset((Buffer.notepad[Line] + 0), ' ', 1);                 // For Cursor
                    // Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][0], Font); // Update the Disp_Pos
                    // noofchars += 1;

                    // Check if the buffer can accommodate an additional character
                    if (noofchars < (sizeof(Buffer.notepad[Line]) - 1))
                    {
                        memset((Buffer.notepad[Line] + 0), ' ', 1);                 // For Cursor
                        Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][0], Font); // Update the Disp_Pos
                        noofchars += 1;
                    }
                    break;
                }
            }
            else
            {
                Disp_pos += Gpu_Rom_Font_WH(Read_sfk, Font);                       // update dispos
                Buffer.temp = Buffer.notepad[Line] + strlen(Buffer.notepad[Line]); // load into temporary buffer
                Buffer.temp[-1] = Read_sfk;                                        // update the string
                Buffer.temp[0] = ' ';
                Buffer.temp[1] = '\0';
                noofchars = strlen(Buffer.notepad[Line]); // get the string len
                if (Disp_pos > LINE_ENDPOS)               // check if there is place to put a character in a specific line
                {
                    Buffer.temp = Buffer.notepad[Line] + (strlen(Buffer.notepad[Line]) - 1);
                    Buffer.temp[0] = '\0';
                    noofchars -= 1;
                    Disp_pos = LINE_STARTPOS;
                    Line++;
                    if (Line >= MAX_LINES)
                        Line = 0;
                    memset((Buffer.notepad[Line]), '\0', sizeof(Buffer.notepad[Line])); // Clear the line buffer
                    for (; noofchars >= 1; noofchars--, tval++)
                    {
                        if (Buffer.notepad[Line - 1][noofchars] == ' ' || Buffer.notepad[Line - 1][noofchars] == '.') // In case of space(New String) or end of statement(.)
                        {
                            memset(Buffer.notepad[Line - 1] + noofchars, '\0', 1);
                            noofchars += 1; // Include the space
                            memcpy(&Buffer.notepad[Line], (Buffer.notepad[Line - 1] + noofchars), tval);
                            break;
                        }
                    }
                    noofchars = strlen(Buffer.notepad[Line]);
                    Buffer.temp = Buffer.notepad[Line] + noofchars;
                    Buffer.temp[0] = ' ';
                    Buffer.temp[1] = '\0';
                    for (tval = 0; tval < noofchars; tval++)
                        Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                }
            }
        }

        // Display List start
        Gpu_CoCmd_Dlstart(phost);
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(100, 100, 100));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation

        Gpu_CoCmd_FgColor(phost, 0x703800);
        Gpu_CoCmd_BgColor(phost, 0x703800);
        But_opt = (Read_sfk == BACK) ? OPT_FLAT : 0; // button color change if the button during press
        App_WrCoCmd_Buffer(phost, TAG(BACK));        // Back		 Return to Home
        Gpu_CoCmd_Button(phost, (DispWidth * 0.855), (DispHeight * 0.83), (DispWidth * 0.146), (DispHeight * 0.112),
                         font, But_opt, "Clear");
        But_opt = (Read_sfk == BACK_SPACE) ? OPT_FLAT : 0;
        App_WrCoCmd_Buffer(phost, TAG(BACK_SPACE)); // BackSpace
        Gpu_CoCmd_Button(phost, (DispWidth * 0.875), (DispHeight * 0.70), (DispWidth * 0.125),
                         (DispHeight * 0.112), font, But_opt, "<-");

        But_opt = (Read_sfk == ' ') ? OPT_FLAT : 0;
        App_WrCoCmd_Buffer(phost, TAG(' ')); // Space
        Gpu_CoCmd_Button(phost, (DispWidth * 0.315), (DispHeight * 0.83), (DispWidth * 0.33), (DispHeight * 0.112),
                         font, But_opt, "Space");

        But_opt = (Read_sfk == BACK_KEY) ? OPT_FLAT : 0; // button color change if the button during press

        if (Read_sfk == BACK_KEY)
        {
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
        }
        App_WrCoCmd_Buffer(phost, TAG(BACK_KEY)); // Back		 Return to Home
        Gpu_CoCmd_Button(phost, (DispWidth * 0.115), (DispHeight * 0.83), (DispWidth * 0.192), (DispHeight * 0.112),
                         font, But_opt, "Back");

        But_opt = (Read_sfk == SAVE_KEY) ? OPT_FLAT : 0;
        if (Read_sfk == SAVE_KEY)
        {
            char profileNameArray[30];
            if (ProfileNameTag == true)
            {
                strncpy(profileNameArray, Buffer.notepad[0], sizeof(profileNameArray) - 1);      // ensure not to exceed buffer size
                profileNameArray[sizeof(profileNameArray) - 1] = '\0';                           // ensure null termination
                strncpy(profile.profileName, profileNameArray, sizeof(profile.profileName) - 1); // ensure not to exceed buffer size
                profile.profileName[sizeof(profile.profileName) - 1] = '\0';                     // ensure null termination
            }
            if (strlen(profile.profileName) == 0)
            {
                strcpy(profile.profileName, "Profile 1"); // Assign default name "Untitled"
            }
            // profile.profileId = getLastUsedProfileId();
//             saveProfile(profile, profile.profileId);
            saveProfile(profile);
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
        }

        if (Flag.Numeric == OFF)
        {
            Gpu_CoCmd_Keys(phost, 0, (DispHeight * 0.442), DispWidth, (DispHeight * 0.112), font, Read_sfk,
                           (Flag.Caps == ON ? "QWERTYUIOP" : "qwertyuiop"));
            Gpu_CoCmd_Keys(phost, (DispWidth * 0.042), (DispHeight * 0.57), (DispWidth * 0.96), (DispHeight * 0.112),
                           font, Read_sfk, (Flag.Caps == ON ? "ASDFGHJKL" : "asdfghjkl"));
            Gpu_CoCmd_Keys(phost, (DispWidth * 0.125), (DispHeight * 0.70), (DispWidth * 0.73), (DispHeight * 0.112),
                           font, Read_sfk, (Flag.Caps == ON ? "ZXCVBNM" : "zxcvbnm"));

            But_opt = (Read_sfk == CAPS_LOCK) ? OPT_FLAT : 0;
            App_WrCoCmd_Buffer(phost, TAG(CAPS_LOCK)); // Capslock
            Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.70), (DispWidth * 0.10), (DispHeight * 0.112), font, But_opt,
                             "a^");
            But_opt = (Read_sfk == NUMBER_LOCK) ? OPT_FLAT : 0;
            App_WrCoCmd_Buffer(phost, TAG(NUMBER_LOCK)); // Numberlock
            Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.83), (DispWidth * 0.10), (DispHeight * 0.112), font, But_opt,
                             "12*");
        }
        if (Flag.Numeric == ON)
        {
            Gpu_CoCmd_Keys(phost, (DispWidth * 0), (DispHeight * 0.442), DispWidth, (DispHeight * 0.112), font,
                           Read_sfk, "1234567890");
            Gpu_CoCmd_Keys(phost, (DispWidth * 0.042), (DispHeight * 0.57), (DispWidth * 0.96), (DispHeight * 0.112),
                           font, Read_sfk, "-@#$%^&*(");
            Gpu_CoCmd_Keys(phost, (DispWidth * 0.125), (DispHeight * 0.70), (DispWidth * 0.73), (DispHeight * 0.112),
                           font, Read_sfk, ")_+[]{}");
            But_opt = (Read_sfk == NUMBER_LOCK) ? OPT_FLAT : 0;
            App_WrCoCmd_Buffer(phost, TAG(253)); // Numberlock
            Gpu_CoCmd_Button(phost, 0, (DispHeight * 0.83), (DispWidth * 0.10), (DispHeight * 0.112), font, But_opt,
                             "AB*");
        }

        App_WrCoCmd_Buffer(phost, TAG(SAVE_KEY)); // Enter
        Gpu_CoCmd_Button(phost, (DispWidth * 0.653), (DispHeight * 0.83), (DispWidth * 0.192), (DispHeight * 0.112), font, But_opt, "Enter");
        App_WrCoCmd_Buffer(phost, TAG_MASK(0)); // Disable the tag buffer updates
        App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 0));
        App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(DispWidth, (uint16_t)(DispHeight * 0.405)));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 255));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Text Color
        line2disp = 0;
        while (line2disp <= Line)
        {
            nextline = 3 + (line2disp * (DispHeight * .073));
            Gpu_CoCmd_Text(phost, line2disp, nextline, font, 0, (const char *)&Buffer.notepad[line2disp]);
            line2disp++;
        }
        Disp_End(phost);
    } while (1);
}

//int32_t getLastSavedProfileId()
//{
//    Profile profile;
//    // initializeProfile(profile);
//    // use profile
//
//    for (int32_t i = 0; i < MAX_PROFILES; i++)
//    {
//        if (!loadProfile(profile, i))
//        {
//            if (i == 0)
//            {
//                return -1; // No profiles have been saved yet
//            }
//            else
//            {
//                return i - 1;
//            }
//        }
//    }
//    return MAX_PROFILES - 1; // All profiles are saved, return the last one
//}

void Print_Profile(Gpu_Hal_Context_t *phost, Profile &profile)
{
    char buffer[100]; // a buffer to format your text before printing.

    // Print profileId
    sprintf(buffer, "Profile ID:    %" PRId32, profile.profileId);
    Gpu_CoCmd_Text(phost, 8, 65, 21, 0, buffer);

    // // Print profileName
    // sprintf(buffer, "Profile Name: %s", profile.profileName);
    // Gpu_CoCmd_Text(phost, 84, 56, 27, 0, buffer);

    // Print Tube_No_x
    sprintf(buffer, "Columns:     %" PRId32, profile.Tube_No_x);
    Gpu_CoCmd_Text(phost, 8, 80, 21, 0, buffer);

    // Print Tube_No_y
    sprintf(buffer, "Rows:         %" PRId32, profile.Tube_No_y);
    Gpu_CoCmd_Text(phost, 8, 95, 21, 0, buffer);

    // Print pitch_x
    char pitch_x_str[10];
    dtostrf(profile.pitch_x, 4, 1, pitch_x_str);
    Gpu_CoCmd_Text(phost, 8, 110, 21, 0, "Pitch(Col):");
    Gpu_CoCmd_Text(phost, 82, 110, 21, 0, pitch_x_str);

    // Print pitch_y
    char pitch_y_str[10];
    dtostrf(profile.pitch_y, 4, 1, pitch_y_str);
    Gpu_CoCmd_Text(phost, 8, 125, 21, 0, "Pitch(Ros):");
    Gpu_CoCmd_Text(phost, 83, 125, 21, 0, pitch_y_str);

    // Print trayOriginX
    char trayOriginX_str[10];
    dtostrf(profile.trayOriginX, 4, 1, trayOriginX_str);
    Gpu_CoCmd_Text(phost, 8, 140, 21, 0, "OriginX:");
    Gpu_CoCmd_Text(phost, 80, 140, 21, 0, trayOriginX_str);

    // Print trayOriginY
    char trayOriginY_str[10];
    dtostrf(profile.trayOriginY, 4, 1, trayOriginY_str);
    Gpu_CoCmd_Text(phost, 8, 155, 21, 0, "OriginY:");
    Gpu_CoCmd_Text(phost, 80, 155, 21, 0, trayOriginY_str);

    // Print Cycles
    sprintf(buffer, "Cycles:       %" PRId32, profile.Cycles);
    Gpu_CoCmd_Text(phost, 8, 170, 21, 0, buffer);

    // Print vibrationEnabled
    sprintf(buffer, "Vibration:    %s", profile.vibrationEnabled ? "True" : "False");
    Gpu_CoCmd_Text(phost, 8, 185, 21, 0, buffer);
}

void Handle_UpDownButtons(Gpu_Hal_Context_t *phost, uint8_t protag, Profile &profile)
{
    static uint8_t prev_protag = 0;
    static int32_t selectedProfileIndex = 0; // This is now an index, not an ID

    if (protag != prev_protag) // Check if the tag has changed
    {
        prev_protag = protag;

        switch (protag)
        {
        case 9: // Up button
            if (selectedProfileIndex < MAX_PROFILES - 1)
            {
                selectedProfileIndex++;
            }
            else
            {
                selectedProfileIndex = 0;
            }
            getProfileName(selectedProfileIndex, profile.profileName, sizeof(profile.profileName));
            getProfileByIndex(profile, selectedProfileIndex);
            Gpu_Hal_Sleep(100);
            break;

        case 10: // Down button
            if (selectedProfileIndex > 0)
            {
                selectedProfileIndex--;
            }
            else
            {
                selectedProfileIndex = MAX_PROFILES - 1;
            }
            getProfileName(selectedProfileIndex, profile.profileName, sizeof(profile.profileName));
            getProfileByIndex(profile, selectedProfileIndex);
            Gpu_Hal_Sleep(100);
            break;
        default:
            break;
        }
    }
}

void Profile_Menu(Gpu_Hal_Context_t *phost, Profile &profile)
{
    getLastProfileAndCopy(); // Get the last profile

    uint8_t protag = 0;

    PageNum = 1;

    while (1)
    {
        protag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);
        if (protag != 0 && (protag < 9 || protag > 10))
        {
            if (protag == 7) // Profile_Menu handle screen //
            {
                SelectProfile_flag = true;
                protag = 0; // Reset the protag value after loading the profile
                break;
            }
            return protag;
        }

        else
        {
            // Handle Up, Down, and Load buttons
            Handle_UpDownButtons(phost, protag, profile);
        }

        Gpu_CoCmd_FlashFast(phost, 0);
        Gpu_CoCmd_Dlstart(phost);

        App_WrCoCmd_Buffer(phost, CLEAR_TAG(255));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));

        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(0, 0, 0));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Text(phost, 84, 16, 27, 0, "Profile Selector");

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(6));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 233, 11, 76, 26, 21, 0, "Home");

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(7));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 8, 205, 76, 26, 21, 0, "Select");

//        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
//        App_WrCoCmd_Buffer(phost, TAG(800)); //disable advanced button
//        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
//        Gpu_CoCmd_Button(phost, 233, 205, 76, 26, 21, 0, "Advanced");

//        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
////        App_WrCoCmd_Buffer(phost, TAG(9));
//        App_WrCoCmd_Buffer(phost, TAG(350)); //disbale Up
//        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
//        Gpu_CoCmd_Button(phost, 265, 76, 44, 29, 21, 0, "Up");
//
//        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
////        App_WrCoCmd_Buffer(phost, TAG(10)); disbale Up and down
////        App_WrCoCmd_Buffer(phost, TAG(300)); disbale Down and down
//        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
//        Gpu_CoCmd_Button(phost, 265, 121, 44, 29, 21, 0, "Down");

        App_WrCoCmd_Buffer(phost, BEGIN(RECTS)); // Profile Name Field
        App_WrCoCmd_Buffer(phost, VERTEX2F(160, 1008));
        App_WrCoCmd_Buffer(phost, VERTEX2F(4944, 704));

        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 255));

        Gpu_CoCmd_Text(phost, 150, 54, 27, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, (const char *)profile.profileName);
        Print_Profile(phost, profile);

        Disp_End(phost);
    }
}

void Config_Settings(Gpu_Hal_Context_t *phost, int32_t tube_no_x, int32_t tube_no_y, float pitch_row_x, float pitch_col_y, float trayOriginX_row, float trayOriginY_col, int32_t cyclesNo, Profile &profile)
{
//    profile.profileId = getLastUsedProfileId();
    // getLastUsedProfileName();
    // loadProfile(profile, profile.profileId); //Bug, not able to rename the Profile name field

    bool disableButtons = false;

    float normalTravelDistX = (13 - 1) * 30.0; // Normal travel distance based on provided pitch and tube numbers
    float normalTravelDistY = (17 - 1) * 30.0; // Normal travel distance based on provided pitch and tube numbers

    float totalTravelDistX = (profile.Tube_No_x - 1) * profile.pitch_x;
    float totalTravelDistY = (profile.Tube_No_y - 1) * profile.pitch_y;

    PageNum = 1;

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
//    App_WrCoCmd_Buffer(phost, TAG_MASK(1));

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
    Gpu_CoCmd_Text(phost, 150, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, (const char *)profile.profileName);

//    static uint16_t lastVibrationStatus = 0xFFFF;
//    uint16_t vibrationStatus = profile.vibrationEnabled ? 0 : 65535;
//
//    if (vibrationStatus != lastVibrationStatus)
//    {
//        // Store the new status for comparison in the next loop
//        lastVibrationStatus = vibrationStatus;
//    }
//    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
//    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
//    App_WrCoCmd_Buffer(phost, TAG(25));
//    Gpu_CoCmd_Toggle(phost, 242, 169, 40, 21, OPT_FLAT | OPT_FORMAT, vibrationStatus, "On\xFFOff");
//    App_WrCoCmd_Buffer(phost, TAG_MASK(0));
//    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0));

    static uint16_t lastVibrationStatus = 0xFFFF;
    uint16_t vibrationStatus = profile.vibrationEnabled ? 0 : 65535;

    if (vibrationStatus != lastVibrationStatus)
    {
        // Store the new status for comparison in the next loop
        lastVibrationStatus = vibrationStatus;
		delay(500);//soon
    }
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    Gpu_CoCmd_BgColor(phost, 0x00A2E8);
    // Check the vibration status and set the foreground color accordingly
    if (vibrationStatus == 0) {  
    Gpu_CoCmd_FgColor(phost, 0x007300);  // Green when ON
    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 128, 0)); //text color
    } else {
    Gpu_CoCmd_FgColor(phost, 0xFF0000);  // Red when OFF
    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); //text color
    }

    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
    App_WrCoCmd_Buffer(phost, TAG(25));
    Gpu_CoCmd_Toggle(phost, 242, 169, 40, 21, OPT_FLAT | OPT_FORMAT, vibrationStatus, "On\xFFOff");
    Gpu_CoCmd_BgColor(phost, 0x00A2E8);
    Gpu_CoCmd_FgColor(phost,0x007300);
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));
    App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0));
    
    // Check the conditions and display the warning text if necessary
    if ((profile.Tube_No_x > 13) || (profile.Tube_No_y > 17))
    {
        // Reset values to maximum if they are above 13/17
        if (profile.Tube_No_x >= 14)
        {
            profile.Tube_No_x = 13;
        }
        if (profile.Tube_No_y >= 18)
        {
            profile.Tube_No_y = 17;
        }
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
        Gpu_CoCmd_Text(phost, 30, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error:");
        Gpu_CoCmd_Text(phost, 265, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Out of range!");
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }

    // Gpu_CoCmd_Text(phost, 124, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Tube_No_x);
    // Gpu_CoCmd_Text(phost, 182, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Tube_No_y);

    // Display the text with color depending on the conditions
    if (profile.Tube_No_x > 13)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 124, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Tube_No_x);

    if (profile.Tube_No_y > 17)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 182, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Tube_No_y);

    // Check if the new travel distance exceeds the normal travel distance
    if (totalTravelDistX > normalTravelDistX || totalTravelDistY > normalTravelDistY)
    {
        // Reset values to maximum if they are above 13/17
        if (profile.pitch_x >= 18.7)
        {
            profile.pitch_x = 18.6;
        }
        if (profile.pitch_y >= 18.7)
        {
            profile.pitch_y = 18.6;
        }
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
        Gpu_CoCmd_Text(phost, 30, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error:");
        Gpu_CoCmd_Text(phost, 265, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Out of range!");
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }

    // Convert input pitch values to decimal
    char pitch_x_str[10];
    char pitch_y_str[10];

    dtostrf(profile.pitch_x, 4, 1, pitch_x_str);
    dtostrf(profile.pitch_y, 4, 1, pitch_y_str);

    // Gpu_CoCmd_Text(phost, 124, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, pitch_x_str);
    // Gpu_CoCmd_Text(phost, 182, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, pitch_y_str);

    // Display the text with color depending on the conditions
    if (profile.pitch_x >= 18.7)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 124, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, pitch_x_str);

    if (profile.pitch_y >= 18.7)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 182, 139, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, pitch_y_str);

    // Check the conditions and display the error text if necessary
    if (profile.trayOriginX > 99.9 || profile.trayOriginY > 99.9)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
        // Gpu_CoCmd_Text(phost, 160, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error: Value is above limits!");
        Gpu_CoCmd_Text(phost, 30, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error:");
        Gpu_CoCmd_Text(phost, 265, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Out of range!");
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black

        // Reset values if they are above 100
        if (profile.trayOriginX >= 100)
        {
            profile.trayOriginX = 99.9;
        }
        if (profile.trayOriginY >= 100)
        {
            profile.trayOriginY = 99.9;
        }
    }

    // Convert input origin values to decimal
    char trayOriginX_str[10];
    char trayOriginY_str[10];

    dtostrf(profile.trayOriginX, 4, 1, trayOriginX_str);
    dtostrf(profile.trayOriginY, 4, 1, trayOriginY_str);

    // Gpu_CoCmd_Text(phost, 125, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, trayOriginX_str);
    // Gpu_CoCmd_Text(phost, 182, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, trayOriginY_str);

    // Display the text with color depending on the conditions
    if (profile.trayOriginX > 99.9)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 125, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, trayOriginX_str);

    if (profile.trayOriginY > 99.9)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 182, 174, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, trayOriginY_str);

    // Check the conditions and display the error text if necessary
    if (profile.Cycles > 99)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
        // Gpu_CoCmd_Text(phost, 160, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error: Value is above limits!");
        Gpu_CoCmd_Text(phost, 30, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Error:");
        Gpu_CoCmd_Text(phost, 265, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "Out of range!");
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black

        // Reset values if they are above 100
        if (profile.Cycles >= 100)
        {
            profile.Cycles = 99;
        }
    }
    // Gpu_CoCmd_Text(phost, 255, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Cycles);

    // Change the text color based on the condition
    if (profile.Cycles > 99)
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 0, 0)); // Change the color to red
    }
    else
    {
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Change the color back to black
    }
    Gpu_CoCmd_Text(phost, 255, 104, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, "%d", profile.Cycles);
    // Gpu_CoCmd_Text(phost, 150, 53, 21, OPT_CENTER | OPT_RIGHTX | OPT_FORMAT, (const char *)profile.profileName);

    // Check the conditions and set the flag if necessary
    if ((profile.Tube_No_x > 13) || (profile.Tube_No_y > 17) || totalTravelDistX > normalTravelDistX || totalTravelDistY > normalTravelDistY || profile.trayOriginX > 99.9 || profile.trayOriginY > 99.9 || profile.Cycles > 99)
    {
        disableButtons = true;
    }
    else
    {
        disableButtons = false;
    }

    App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
    App_WrCoCmd_Buffer(phost, TAG_MASK(1));
    Gpu_CoCmd_FgColor(phost, 0x00A2E8);

    // Load Button
    App_WrCoCmd_Buffer(phost, TAG_MASK(disableButtons ? 0 : 1)); // Enable or disable based on the flag
    App_WrCoCmd_Buffer(phost, TAG(11));
    Gpu_CoCmd_Button(phost, 116, 205, 76, 26, 21, 0, "Load");
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));

    // Save Button
    App_WrCoCmd_Buffer(phost, TAG_MASK(disableButtons ? 0 : 1)); // Enable or disable based on the flag
    App_WrCoCmd_Buffer(phost, TAG(12));
    Gpu_CoCmd_Button(phost, 8, 205, 76, 26, 21, 0, "Save");
    App_WrCoCmd_Buffer(phost, TAG_MASK(0));

    // Advance Button
//    App_WrCoCmd_Buffer(phost, TAG_MASK(disableButtons ? 0 : 1)); // Enable or disable based on the flag
//    App_WrCoCmd_Buffer(phost, TAG(800)); //disable advanced button
//    Gpu_CoCmd_Button(phost, 233, 205, 76, 26, 21, 0, "Advanced");
//    App_WrCoCmd_Buffer(phost, TAG_MASK(0));

    Disp_End(phost);
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

// Numeric Key
void Keyboard(Gpu_Hal_Context_t *phost, Profile &profile, int32_t tube_no_x, int32_t tube_no_y, float pitch_row_x, float pitch_col_y, float trayOriginX_row, float trayOriginY_col, int32_t cyclesNo)
{
    delay(50); // Added to create smooth transition between screen
    PageNum = 3;
    phost = &host;
    /*local variables*/
    uint8_t Line = 0;
    uint16_t Disp_pos = 0;
    int32_t Read_sfk = 0, tval;
    uint16_t noofchars = 0, line2disp = 0, nextline = 0;

    char tube_no_x_str[MAX_CHAR_PER_LINE];
    char tube_no_y_str[MAX_CHAR_PER_LINE];
    char pitch_row_x_str[MAX_CHAR_PER_LINE];
    char pitch_col_y_str[MAX_CHAR_PER_LINE];
    char trayOriginX_row_str[MAX_CHAR_PER_LINE];
    char trayOriginY_col_str[MAX_CHAR_PER_LINE];
    char cyclesNo_str[MAX_CHAR_PER_LINE];

    // Clear Linebuffer
    //tube_no_x_str[0]=0;
   // tube_no_y_str[0]=0;
    //pitch_row_x_str[0]=0;
   // pitch_col_y_str[0]=0;
   // trayOriginX_row_str[0]=0;
  //  trayOriginY_col_str[0]=0;
  //  cyclesNo_str[0]=0;
    for (tval = 0; tval < MAX_LINES; tval++)
		//Buffer.notepad[tval][0]=0;//try014b
        memset(&Buffer.notepad[tval], '\0', sizeof(Buffer.notepad[tval]));//try014b

    /*intial setup*/
    Line = 0;                                                   // Starting line
    Disp_pos = LINE_STARTPOS;                                   // starting pos
    Flag.Numeric = ON;                                          // Disable the numbers and spcial charaters

    memset((Buffer.notepad[Line] + 0), ' ', 1);                 // For Cursor

    Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][0], Font); // Update the Disp_Pos
    noofchars += 1;                                             // for cursor
                                                                /*enter*/
    Flag.Exit = 0;
	delay(500);//soon

    do
    {
        Read_sfk = Read_Keypad(); // read the keys

        if (Flag.Key_Detect)
        {                        // check if key is pressed
            Flag.Key_Detect = 0; // clear it
            if (Read_sfk >= SPECIAL_FUN)
            { // check any special function keys are pressed
                switch (Read_sfk)
                {
                case BACK_SPACE:
                    if (noofchars > 1) // check in the line there is any characters are present,cursor not include
                    {
                        noofchars -= 1;                                                             // clear the character inthe buffer
                        Disp_pos -= Gpu_Rom_Font_WH(*(Buffer.notepad[Line] + noofchars - 1), Font); // Update the Disp_Pos
                    }
                    else
                    {
                        if (Line >= (MAX_LINES - 1))
                            Line--;
                        else
                            Line = 0;                                                      // check the lines
                        noofchars = strlen(Buffer.notepad[Line]);                          // Read the len of the line
                        for (tval = 0; tval < noofchars; tval++)                           // Compute the length of the Line
                            Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                    }
                    Buffer.temp = (Buffer.notepad[Line] + noofchars); // load into temporary buffer
                    Buffer.temp[-1] = ' ';                            // update the string
                    Buffer.temp[0] = '\0';
                    break;
                }
            }
            else
            {
                Disp_pos += Gpu_Rom_Font_WH(Read_sfk, Font);                       // update dispos Font

                Buffer.temp = Buffer.notepad[Line] + strlen(Buffer.notepad[Line]); // load into temporary buffer
                Buffer.temp[-1] = Read_sfk;                                        // update the string
                Buffer.temp[0] = ' ';
                Buffer.temp[1] = '\0';
                noofchars = strlen(Buffer.notepad[Line]); // get the string len
                if (Disp_pos > LINE_ENDPOS)               // check if there is place to put a character in a specific line
                {
                    Buffer.temp = Buffer.notepad[Line] + (strlen(Buffer.notepad[Line]) - 1);
                    Buffer.temp[0] = '\0';
                    noofchars -= 1;
                    Disp_pos = LINE_STARTPOS;
                    Line++;
                    if (Line >= MAX_LINES)
                        Line = 0;

                    memset((Buffer.notepad[Line]), '\0', sizeof(Buffer.notepad[Line])); // Clear the line buffer


                    for (; noofchars >= 1; noofchars--, tval++)
                    {
                        if (Buffer.notepad[Line - 1][noofchars] == ' ' || Buffer.notepad[Line - 1][noofchars] == '.') // In case of space(New String) or end of statement(.)
                        {
                            memset(Buffer.notepad[Line - 1] + noofchars, '\0', 1);
                            noofchars += 1; // Include the space
                            memcpy(&Buffer.notepad[Line], (Buffer.notepad[Line - 1] + noofchars), tval);


                            break;
                        }
                    }
                    noofchars = strlen(Buffer.notepad[Line]);
                    Buffer.temp = Buffer.notepad[Line] + noofchars;
                    Buffer.temp[0] = ' ';
                    Buffer.temp[1] = '\0';
                    for (tval = 0; tval < noofchars; tval++)
                        Disp_pos += Gpu_Rom_Font_WH(Buffer.notepad[Line][tval], Font); // Update the Disp_Pos
                }
            }
        }
        // Display List start
        Gpu_CoCmd_Dlstart(phost);
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(120, 120, 120));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, TAG_MASK(1)); // enable tagbuffer updation
        Gpu_CoCmd_FgColor(phost, 0xB40000);     // 0x703800 0xD00000
        Gpu_CoCmd_BgColor(phost, 0xB40000);

        But_opt = (Read_sfk == BACK) ? OPT_FLAT : 0; // button color change if the button during press

        if (Read_sfk == BACK)
        {
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
        }
        App_WrCoCmd_Buffer(phost, TAG(BACK)); // Back		 Return to Home
        Gpu_CoCmd_Button(phost, 207, 112, 67, 50, font, But_opt, "Back");

        But_opt = (Read_sfk == BACK_SPACE) ? OPT_FLAT : 0;
        App_WrCoCmd_Buffer(phost, TAG(BACK_SPACE)); // BackSpace
        Gpu_CoCmd_Button(phost, 207, 181, 67, 50, font, But_opt, "Del");

        But_opt = (Read_sfk == NUM_ENTER) ? OPT_FLAT : 0;
        App_WrCoCmd_Buffer(phost, TAG(NUM_ENTER)); // Enter
        Gpu_CoCmd_Button(phost, 207, 36, 67, 50, font, But_opt, "Enter");

        if (Read_sfk == 247)
        {

            if (TubeRowTag == true)
            {
                strncpy(tube_no_x_str, Buffer.notepad[0], MAX_CHAR_PER_LINE); // Copy inputted text to tube_no_str
                tube_no_x_str[MAX_CHAR_PER_LINE] = '\0';                      // Ensure null-terminated
                int32_t tube_no_x = atoi(tube_no_x_str);                      // Convert inputted text to integer value
                profile.Tube_No_x = tube_no_x;                                // Update Tube_No_x parameter in profile
                // Serial.println("tube_no_x:");
                // Serial.println(tube_no_x);
            }

            if (TubeColumnTag == true)
            {
                strncpy(tube_no_y_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                tube_no_y_str[MAX_CHAR_PER_LINE] = '\0';
                int32_t tube_no_y = atoi(tube_no_y_str);
                profile.Tube_No_y = tube_no_y;
            }

            if (PitchRowTag == true)
            {
                strncpy(pitch_row_x_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                pitch_row_x_str[MAX_CHAR_PER_LINE] = '\0';
                float pitch_row_x = atof(pitch_row_x_str);
                profile.pitch_x = pitch_row_x;
            }

            if (PitchColumnTag == true)
            {
                strncpy(pitch_col_y_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                pitch_col_y_str[MAX_CHAR_PER_LINE] = '\0';
                float pitch_col_y = atof(pitch_col_y_str);
                profile.pitch_y = pitch_col_y;
            }

            if (OriginRowTag == true)
            {
                strncpy(trayOriginX_row_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                trayOriginX_row_str[MAX_CHAR_PER_LINE] = '\0';
                float trayOriginX_row = atof(trayOriginX_row_str);
                profile.trayOriginX = trayOriginX_row;
            }

            if (OriginColumnTag == true)
            {
                strncpy(trayOriginY_col_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                trayOriginY_col_str[MAX_CHAR_PER_LINE] = '\0';
                float trayOriginY_col = atof(trayOriginY_col_str);
                profile.trayOriginY = trayOriginY_col;
            }

            if (CyclesTag == true)
            {
                strncpy(cyclesNo_str, Buffer.notepad[0], MAX_CHAR_PER_LINE);
                cyclesNo_str[MAX_CHAR_PER_LINE] = '\0';
                int32_t cyclesNo = atoi(cyclesNo_str);
                profile.Cycles = cyclesNo;
            }
            Config_Settings(phost, tube_no_x, tube_no_y, pitch_row_x, pitch_col_y, trayOriginX_row, trayOriginY_col, cyclesNo, profile);
            break;
        }

        if (Flag.Numeric == ON)
        {
            Gpu_CoCmd_Keys(phost, 47, 35, 150, 50, 29, Read_sfk, "789");
            Gpu_CoCmd_Keys(phost, 47, 88, 150, 50, 29, Read_sfk, "456");
            Gpu_CoCmd_Keys(phost, 47, 140, 150, 50, 29, Read_sfk, "123");
            Gpu_CoCmd_Keys(phost, 47, 191, 150, 40, 29, Read_sfk, "0.");
        }
        App_WrCoCmd_Buffer(phost, TAG_MASK(0)); // Disable the tag buffer updates
        App_WrCoCmd_Buffer(phost, SCISSOR_XY(0, 0));
        App_WrCoCmd_Buffer(phost, SCISSOR_SIZE(320, 25));

        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, CLEAR(1, 1, 1));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 0)); // Text Color
        line2disp = 0;
		//if(Buffer.notepad[line2disp][0]!=0) Buffer.notepad[line2disp][0] =0;//try013


        while (line2disp <= Line)
        {
            // nextline = 3 + (line2disp * (DispHeight * .073));
            
            Gpu_CoCmd_Text(phost, line2disp, nextline, font, 0, (const char *)&Buffer.notepad[line2disp]);
            line2disp++;
            tempvarx = ((const char *)&Buffer.notepad[line2disp]);
        }
        App_WrCoCmd_Buffer(phost, DISPLAY());
        Gpu_CoCmd_Swap(phost);
        App_Flush_Co_Buffer(phost);
    } while (1);
}

void displayTextMatrixName()
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
    bool buttonState = profile.buttonStates[row - 1][col - 1];

    App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
    App_WrCoCmd_Buffer(phost, POINT_SIZE(buttonSize * 10));

    if (buttonState)
    {
        // If button state is OFF
        App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
        App_WrCoCmd_Buffer(phost, POINT_SIZE(buttonSize * 10));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(200, 0, 0)); // Display red circle
        App_WrCoCmd_Buffer(phost, TAG(tag));
        Gpu_CoCmd_Track(phost, x + buttonSize / 2, y + buttonSize / 2, 0, 0, tag); // Enable touch tracking for TAG
        App_WrCoCmd_Buffer(phost, VERTEX2II(x, y, 0, 0));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Text(phost, x, y, 20, OPT_CENTER, "OFF");

        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 255));

        if (i == 0) // First column on each page
        {
            String colStr = "C" + String(col);
            Gpu_CoCmd_Text(phost, x, y - 16, 20, OPT_CENTER, colStr.c_str());
        }

        if (j == 0) // First row on each page
        {
            String rowStr = "R" + String(row);
            Gpu_CoCmd_Text(phost, x - 16, y, 20, OPT_CENTER, rowStr.c_str());
        }
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, END());
    }
    else
    {
        // If button state is ON
        App_WrCoCmd_Buffer(phost, BEGIN(FTPOINTS));
        App_WrCoCmd_Buffer(phost, POINT_SIZE(buttonSize * 10));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 128, 0)); // Display green circle
        App_WrCoCmd_Buffer(phost, TAG(tag));
        Gpu_CoCmd_Track(phost, x + buttonSize / 2, y + buttonSize / 2, 0, 0, tag); // Enable touch tracking for TAG
        App_WrCoCmd_Buffer(phost, VERTEX2II(x, y, 0, 0));
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Text(phost, x, y, 20, OPT_CENTER, "ON");

        App_WrCoCmd_Buffer(phost, COLOR_RGB(0, 0, 255));

        if (i == 0) // First column on each page
        {
            String colStr = "C" + String(col);
            Gpu_CoCmd_Text(phost, x, y - 16, 20, OPT_CENTER, colStr.c_str());
        }

        if (j == 0) // First row on each page
        {
            String rowStr = "R" + String(row);
            Gpu_CoCmd_Text(phost, x - 16, y, 20, OPT_CENTER, rowStr.c_str());
        }
        App_WrCoCmd_Buffer(phost, COLOR_RGB(255, 255, 255));
        App_WrCoCmd_Buffer(phost, END());
    }
}

void Tray_Screen(Gpu_Hal_Context_t *phost, Profile &profile)
{
    initializeProfile(profile);

//    profile.profileId = getLastUsedProfileId();
    getLastUsedProfileName();

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
    int32_t buttonSize = min((240 - ((numCols + 1) * 16)) / numCols, (240 - ((numRows + 1) * 16)) / numRows);
    int32_t rowGap = (240 - (numRows * buttonSize)) / (numRows + 1);
    int32_t colGap = (240 - (numCols * buttonSize)) / (numCols + 1);

    // Calculate starting position for buttons
    int32_t startX = (STARTPOS - ((numCols * (buttonSize + colGap)) - colGap)) / 2;
    int32_t startY = (STARTPOS - ((numRows * (buttonSize + rowGap)) - rowGap)) / 2;

    while (1)
    {
        uint8_t touch_tag = Gpu_Hal_Rd8(phost, REG_TOUCH_TAG);

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
        App_WrCoCmd_Buffer(phost, VERTEX2F(3840, 3840));
        App_WrCoCmd_Buffer(phost, END());

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(11));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 90, 62, 26, 21, 0, "Load"); // Tray Load button

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(21));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 46, 62, 26, 21, 0, "Save"); // Tray Save button

        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(22));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 0, 62, 26, 21, 0, "Home"); // Tray Home button

        // Draw Left navigation buttons
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(23));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 248, 178, 20, 20, 21, 0, "<"); // Previous page button

        // Draw right navigation buttons
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(24));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 288, 178, 20, 20, 21, 0, ">"); // Next page button

        // Draw up navigation button
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(26));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 268, 158, 20, 20, 21, 0, "^");

        // Draw down navigation button
        Gpu_CoCmd_FgColor(phost, 0x00A2E8);
        App_WrCoCmd_Buffer(phost, TAG(27));
        App_WrCoCmd_Buffer(phost, CLEAR_COLOR_RGB(255, 255, 255));
        Gpu_CoCmd_Button(phost, 268, 198, 20, 20, 20, 0, "v");

        // Calculate starting index and ending index for current page
        int startIndex = currentPage * numButtonsPerPage;
        // int endIndex = min(startIndex + numButtonsPerPage, totalNumButtons);// (optional to detect the last index)

        int currentPageRows = minimum(profile.Tube_No_y, numRows);
        int currentPageCols = minimum(profile.Tube_No_x, numCols);

        // Draw buttons for current page
        int x = startX;
        int y = startY;
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
                        profile.buttonStates[row - 1][col - 1] = !profile.buttonStates[row - 1][col - 1];
                        Serial.println("Button at (" + String(row) + ", " + String(col) + ") is now " + (profile.buttonStates[row - 1][col - 1] ? "OFF" : "ON"));
                    }
                    toggleButton(phost, profile, tag, x, y, buttonSize, row, col, i, j);
                }
                x += buttonSize + colGap;
            }
            x = startX;
            y += buttonSize + rowGap;
        }
        displayTextMatrixName();

        if (touch_tag == 11) // Load
        {
            // profile.profileId = getLastSavedProfileId();
//            profile.profileId = getLastUsedProfileId();
            getLastUsedProfileName();
//            loadProfile(profile, profile.profileId); // Call the loadProfile() function to load the profile at the given index
            loadProfile(profile);
//            Home_Menu(&host, profile);
            Profile_Menu(&host, profile);
            break;
//            return;
        }

        else if (touch_tag == 21) // Save button
        {
//            profile.profileId = getLastUsedProfileId();
            getLastUsedProfileName();
//            saveProfile(profile, profile.profileId);
            saveProfile(profile);
//            Profile_Menu(phost, profile);
//            break;
        }

        else if (touch_tag == 22) // Home button
        {
            Home_Menu(&host, profile,MAINMENU);//soon
            break;
//            return;
        }

        else if (touch_tag == 24) // Next page button
        {
            if (currentColPage < (profile.Tube_No_x / numCols))
            {
                currentColPage++;
                startCol = currentColPage * numCols + 1;
//                saveProfile(profile, profile.profileId); // Save profile on navigation
                saveProfile(profile);
            }
        }

        else if (touch_tag == 23) // Previous page button
        {
            if (currentColPage > 0)
            {
                currentColPage--;
                startCol = currentColPage * numCols + 1;
//                saveProfile(profile, profile.profileId); // Save profile on navigation
                saveProfile(profile);  
                
            }
        }

        else if (touch_tag == 26) // Up button
        {
            if (currentRowPage > 0)
            {
                currentRowPage--;
                startRow = currentRowPage * numRows + 1;
//                saveProfile(profile, profile.profileId); // Save profile on navigation
                saveProfile(profile);
            }
        }

        else if (touch_tag == 27) // Down button
        {
            if (currentRowPage < (profile.Tube_No_y / numRows)) // Change here
            {
                currentRowPage++;
                startRow = currentRowPage * numRows + 1;
//                saveProfile(profile, profile.profileId); // Save profile on navigation
                saveProfile(profile);
            }
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
    }
}
