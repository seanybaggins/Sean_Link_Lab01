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

// To convert uppercase characters to lowercase
#include <ctype.h>

// Necessary to write data to a temporary character buffer to ultimately display
#include <stdio.h>

// Necessary for string comparison
#include <string.h>
// Necessary for ADC
#include "driverlib/adc.h"
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

// Note: These structs have been defined as globals due to stack overflow when
// defined inside functions
static tContext sContext;
static tRectangle sRect;

// Prototypes
char getCharacterFromComputer(void);
void UARTSend(const uint8_t *pui8Buffer);
void clearOLED(void);
void printMainMenu(void);
char displayInfoOnBoard(uint32_t pui32ADC0Value);

int main(void)
{

    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    FPULazyStackingEnable();

    // Set the clocking to run directly from the crystal.
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_XTAL_16MHZ |
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
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    // Set GPIO A0 and A1 as UART pins.
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));
    // Disabling the ADC to allow for configurations
    ADCSequenceDisable(ADC0_BASE,0);

    // Enable sample sequence 3 with a processor signal trigger.  Sequence 3
    // will do a single sample when the processor sends a signal to start the
    // conversion.  Each ADC module has 4 programmable sequences, sequence 0
    // to sequence 3.  This example is arbitrarily using sequence 3.
    //
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);

    //
    // Configure step 0 on sequence 3.  Sample channel 0 (ADC_CTL_CH0) in
    // single-ended mode (default) and configure the interrupt flag
    // (ADC_CTL_IE) to be set when the sample is done.  Tell the ADC logic
    // that this is the last conversion on sequence 3 (ADC_CTL_END).  Sequence
    // 3 has only one programmable step.  Sequence 1 and 2 have 4 steps, and
    // sequence 0 has 8 programmable steps.  Since we are only doing a single
    // conversion using sequence 3 we will only configure step 0.  For more
    // information on the ADC sequences and steps, reference the datasheet.
    //
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH7 | ADC_CTL_IE | // Do we need the ADC_CTL_IE
                             ADC_CTL_END);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH6 | ADC_CTL_IE |
                                 ADC_CTL_END);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH5 | ADC_CTL_IE |
                                     ADC_CTL_END);

    // Enabling ADC
    ADCSequenceEnable(ADC0_BASE, 0);

    // Clear the ADC interrupt flag
    ADCIntClear(ADC0_BASE,0);

    // Displaying message to Terminal
    printMainMenu();
    // Initializing variables
    uint32_t pui32ADC0Value[1];
    while(1)
    {
        //
        // Trigger the ADC conversion.
        //
        ADCProcessorTrigger(ADC0_BASE, 0);

        //
        // Wait for conversion to be completed.
        //
        while(!ADCIntStatus(ADC0_BASE, 0, false))
        {
        }

        //
        // Clear the ADC interrupt flag.
        //
        ADCIntClear(ADC0_BASE, 0);

        //
        // Read ADC Value.
        //
        ADCSequenceDataGet(ADC0_BASE, 0, pui32ADC0Value);

        //
        // Display the AIN0 (PE3) digital value on OLED.
        //
        displayInfoOnBoard(pui32ADC0Value[0]);
        //UARTprintf("AIN0 = %4d\r", pui32ADC0Value[0]);
    }
    //return 0;
}

void printMainMenu(void) {
    UARTSend("\r\n\nT - Toggle the LED\r\n");
    UARTSend("S - Splash Screen (2s)\r\n");
    UARTSend("A - ADC Data");
}
// Pulls Character from computer. If the there is a character to pull from the
// user the function will return the character. Otherwise the function will
// return the null character.
char getCharacterFromComputer(void) {
    while (UARTCharsAvail(UART0_BASE)) {
        int32_t localChar;
        localChar = UARTCharGetNonBlocking(UART0_BASE);
        if (localChar != -1) {
            return localChar;
        }
    }
    return '\0';
}

void UARTSend(const uint8_t *pui8Buffer)
{
    // Loop while there are more characters to send.
    for(uint32_t index = 0; index < strlen((const char *)pui8Buffer); index++)
    {
        // Write the next character to the UART.
        UARTCharPut(UART0_BASE, pui8Buffer[index]);
    }
}

void clearOLED(void) {
    for (int i = 20; i <= 40; i++) {
        GrStringDrawCentered(&sContext,"              ", -1,
                                                 GrContextDpyWidthGet(&sContext) / 2, i, true);
    }
}

char displayInfoOnBoard(uint32_t pui32ADC0Value) {

    char displayDataBuffer[16];

    sprintf(displayDataBuffer,"Ain0 = %d", pui32ADC0Value);

    GrStringDrawCentered(&sContext, displayDataBuffer, -1,
                                    GrContextDpyWidthGet(&sContext) / 2, 40, true);
    return pui32ADC0Value;

}
