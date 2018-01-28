/* Purpose of the following file is to achieve a set of tasks provided by the
 * instuctor using UART to communicate between the main computer and the
 * TM4C123G development board.
 *
 */

// Base includes
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"
#include "driverlib/gpio.h"

//Necessary for UART
#include "inc/hw_ints.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "drivers/cfal96x64x16.h"

// Necessary for button press
#include "driverlib/rom_map.h"
#include "drivers/buttons.h"

// To convert uppercase characters to lowercase
#include <ctype.h>

// Necessary to write data to a temporary character buffer to ultimately display
#include <stdio.h>

// Necessary for string comparison
#include <string.h>

// Necessary for blinking light clock cycles
#define FIVE_PERCENT_CYCLE_ON 200000
#define NIENTYFIVE_PERCENT_CYCLE_OFF 3800000


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

typedef struct {
    uint32_t upCount, downCount, leftCount, rightCount, selectCount;
    char lastPressed[7];
} Button;

//Globals
static int32_t globalNumberCharactersRecieved = 0;

// Note: These structs have been defined as globals due to stack overflow when
// defined inside functions
static tContext sContext;
static tRectangle sRect;

//prototypes
void floodCharacter(uint32_t floodCharacterEnable);
char getCharacterFromComputer(void);
void blinkLED(uint32_t enableLED);
char displayInfoOnBoard(uint8_t characterFromComputer, uint32_t loopCounter, Button button, uint32_t charactersRecieved);
void UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count);
void clearOLED(void);
Button readButtonPressFromBoard(Button button);


int main(void)
{

    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    ROM_FPULazyStackingEnable();

    // Set the clocking to run directly from the crystal.
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    // Initialize the display driver.
    CFAL96x64x16Init();

    // Initialize the graphics context.
            GrContextInit(&sContext, &g_sCFAL96x64x16);

    // Fill the top part of the screen with blue to create the banner.
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
    sRect.i16YMax = 9;
    GrContextForegroundSet(&sContext, ClrDarkBlue);
    GrRectFill(&sContext, &sRect);

    // Change foreground for white text.
    GrContextForegroundSet(&sContext, ClrWhite);

    // Put the lab name in the middle of the banner.
    GrContextFontSet(&sContext, g_psFontFixed6x8);
    GrStringDrawCentered(&sContext, "Sean Link Lab02", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 4, 0);

    // Enable the peripherals used in the uart_echo example.
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    // Enable processor interrupts.
    ROM_IntMasterEnable();

    // Set GPIO A0 and A1 as UART pins.
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    //Configuration for button
    ButtonsInit();

    //displaying message to Terminal
    UARTSend("\n\rI - Display Iterations\n\rB - Last Button Pressed\n\rN - Number of Characters Received\n\rT - Toggle Flashing Light\n\rF - Flood Character\n\rP - Toggle Program Execution\n\rQ - Quit Program\n\r\n\r", 185);

    //Initializing variables
    int32_t characterFromComputer = '\0';
    uint32_t loopCounter = 0;
    int32_t lastMenuSelection = '\0';
    uint32_t enableLED = 1;
    uint32_t charactersRecieved = 0;
    uint32_t floodCharacterEnable = 0;
    Button button;
    button.upCount = 0;
    button.downCount = 0;
    button.leftCount = 0;
    button.rightCount = 0;
    button.selectCount = 0;
    strcpy(button.lastPressed,"none");

    while(tolower(lastMenuSelection) != 'q')
    {
        blinkLED(enableLED);
        floodCharacter(floodCharacterEnable);
        characterFromComputer = getCharacterFromComputer();
        button = readButtonPressFromBoard(button);

        // Keeping track of the last valid input provided by the user
        if (characterFromComputer == '\0' && loopCounter > 0) {
            characterFromComputer = lastMenuSelection;
        }

        // if menu sections change, clear the board display and send message to UART indicating retrieval
        else if (characterFromComputer != lastMenuSelection && loopCounter > 0) {
            clearOLED();
            charactersRecieved++;
            UARTSend("\r\nMessage Received!", 19);
        }

        // Toggle the LED to blink or not if the the user input = t
        if (tolower(characterFromComputer) == 't') {
            enableLED = !enableLED;
        }

        // Toggling flood character functionality
        else if (tolower(characterFromComputer) == 'f') {
            floodCharacterEnable = !floodCharacterEnable;
        }

        //  Keeping track of last valid input and displaying to the OLED
        lastMenuSelection = displayInfoOnBoard(characterFromComputer, loopCounter, button, charactersRecieved);

        // Toggling Execution of program
        while (tolower(characterFromComputer == 'p')) {
            characterFromComputer = getCharacterFromComputer();
            if (characterFromComputer == '\0') {
                characterFromComputer = 'p';
            }
            else if (characterFromComputer == 'p') {
                UARTSend("\r\nMessage Received!", 19);
                characterFromComputer = !characterFromComputer;
                charactersRecieved++;
            }
        }

        loopCounter++;
    }
    return 0;
}

void floodCharacter(uint32_t floodCharacterEnable) {
    if (floodCharacterEnable == 1) {
        UARTSend("S",1);
    }
}

Button readButtonPressFromBoard(Button button) {
    if (MAP_GPIOPinRead(BUTTONS_GPIO_BASE, DOWN_BUTTON) != DOWN_BUTTON) {
        button.downCount++;
        strcpy(button.lastPressed,"down");
    }
    else if (MAP_GPIOPinRead(BUTTONS_GPIO_BASE, UP_BUTTON) != UP_BUTTON) {
        button.upCount++;
        strcpy(button.lastPressed,"up");
    }
    else if (MAP_GPIOPinRead(BUTTONS_GPIO_BASE, LEFT_BUTTON) != LEFT_BUTTON) {
        button.leftCount++;
        strcpy(button.lastPressed,"left");
    }
    else if (MAP_GPIOPinRead(BUTTONS_GPIO_BASE, RIGHT_BUTTON) != RIGHT_BUTTON) {
        button.rightCount++;
        strcpy(button.lastPressed, "right");
    }
    else if (MAP_GPIOPinRead(BUTTONS_GPIO_BASE, SELECT_BUTTON) != SELECT_BUTTON) {
        button.selectCount++;
        strcpy(button.lastPressed, "select");
    }
    return button;
}

char getCharacterFromComputer(void) {
    while (UARTCharsAvail(UART0_BASE)) {
        int32_t localChar;
        localChar = UARTCharGetNonBlocking(UART0_BASE);
        if (localChar != -1) {
            globalNumberCharactersRecieved++;
            return localChar;
        }
    }
    return '\0';
}

void blinkLED(uint32_t enable) {
    volatile uint32_t ui32Loop;

    // Turn on the LED.
    if (enable == 1) {
        GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);
    }

    // Delay for a bit.
    for(ui32Loop = 0; ui32Loop < FIVE_PERCENT_CYCLE_ON; ui32Loop++)
    {
    }

    // Turn off the LED.
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);

    // Delay for a bit.
    for(ui32Loop = 0; ui32Loop < NIENTYFIVE_PERCENT_CYCLE_OFF; ui32Loop++)
    {
    }
}


char displayInfoOnBoard(uint8_t characterFromComputer, uint32_t loopCounter, Button button, uint32_t charactersRecieved) {

    char displayDataBuffer[16];


    if (tolower(characterFromComputer) == 'i') {
        GrStringDrawCentered(&sContext,"Loop Count", -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 30, true);

        sprintf(displayDataBuffer,"%d", loopCounter);

        GrStringDrawCentered(&sContext, displayDataBuffer, -1,
                                        GrContextDpyWidthGet(&sContext) / 2, 40, true);
        return characterFromComputer;
    }

    else if (tolower(characterFromComputer) == 'b') {
        clearOLED();
        GrStringDrawCentered(&sContext,"Button ", -1,
                                         GrContextDpyWidthGet(&sContext) / 2, 20, true);
        if (strcmp(button.lastPressed, "down") == 0) {
            sprintf(displayDataBuffer,"Down: %d", button.downCount);
        }
        else if (strcmp(button.lastPressed, "up") == 0) {
            sprintf(displayDataBuffer,"Up: %d", button.upCount);
        }
        else if (strcmp(button.lastPressed, "left") == 0) {
            sprintf(displayDataBuffer,"left: %d", button.leftCount);
        }
        else if (strcmp(button.lastPressed, "right") == 0) {
            sprintf(displayDataBuffer, "right: %d", button.rightCount);
        }
        else if (strcmp(button.lastPressed, "select") == 0) {
            sprintf(displayDataBuffer, "select: %d", button.selectCount);
        }
        else {
            sprintf(displayDataBuffer," ");
        }
        GrStringDrawCentered(&sContext, displayDataBuffer, -1,
                                         GrContextDpyWidthGet(&sContext) / 2, 30, true);
        return characterFromComputer;
    }

    else if (tolower(characterFromComputer) == 'n') {
        GrStringDrawCentered(&sContext, "Chars Received", -1,
                                         GrContextDpyWidthGet(&sContext) / 2, 20, true);
        sprintf(displayDataBuffer, "%d", charactersRecieved);
        GrStringDrawCentered(&sContext, displayDataBuffer, -1,
                                         GrContextDpyWidthGet(&sContext) / 2, 30, true);
    }

    else if (tolower(characterFromComputer) == 'q') {
        GrStringDrawCentered(&sContext,"Terminated!", -1,
                                         GrContextDpyWidthGet(&sContext) / 2, 30, true);
        return characterFromComputer;
    }

    return '\0';


}

void UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count)
{
    // Loop while there are more characters to send.
    while(ui32Count--)
    {
        // Write the next character to the UART.
        ROM_UARTCharPut(UART0_BASE, *pui8Buffer++);
    }
}

void clearOLED(void) {
    for (int i = 20; i <= 40; i++) {
        GrStringDrawCentered(&sContext,"              ", -1,
                                                 GrContextDpyWidthGet(&sContext) / 2, i, true);
    }
}
