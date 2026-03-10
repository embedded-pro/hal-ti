//*****************************************************************************
//
// boot_demo_can.c - CAN boot loader example.
//
// Copyright (c) 2020 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_can.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/rom_map.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>CAN Boot Loader Demo(boot_demo_can)</h1>
//!
//! An example to demonstrate the use of a flash-based boot loader.  At
//! startup, the application will configure the CAN peripheral
//! and then branch to the boot loader to await the start of an
//! update.  It is important that the CAN baud rate be the same for all
//! nodes on the network. The baudrate is defined by CANBAUD in this file.
//!
//! The link address is set to 0x1000.  You may change this
//! address to a 1KB boundary higher than the last address occupied
//! by the CAN boot loader binary as long as you also rebuild the boot
//! loader itself after modifying its bl_config.h file to set APP_START_ADDRESS
//! to the same value.
//!
//!
//*****************************************************************************
#define CANID 0x1F028000
#define CANBAUD 125000
#define LEDf_PINS GPIO_PIN_0 | GPIO_PIN_4
#define LEDn_PINS GPIO_PIN_0 | GPIO_PIN_1
#if NUMBER_LEDS == 1
#define MYf_LEDS GPIO_PIN_0
#define MYn_LEDS 0
#elif NUMBER_LEDS == 2
#define MYf_LEDS (GPIO_PIN_0 | GPIO_PIN_4)
#define MYn_LEDS 0
#elif NUMBER_LEDS == 3
#define MYf_LEDS (GPIO_PIN_0 | GPIO_PIN_4)
#define MYn_LEDS GPIO_PIN_0
#elif NUMBER_LEDS == 4
#define MYf_LEDS (GPIO_PIN_0 | GPIO_PIN_4)
#define MYn_LEDS (GPIO_PIN_0 | GPIO_PIN_1)
#else
#error "Must define NUMBER_LEDS as 1, 2, 3 or 4"
#endif

uint32_t g_ui32SysClock;

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

//*****************************************************************************
//
// Passes control to the bootloader and initiates a remote software update.
//
// This function passes control to the bootloader and initiates an update of
// the main application firmware image via UART0 or USB depending
// upon the specific boot loader binary in use.
//
// \return Never returns.
//
//*****************************************************************************
void
JumpToBootLoader(void)
{
    //
    // Disable all processor interrupts.  Instead of disabling them
    // one at a time, a direct write to NVIC is done to disable all
    // peripheral interrupts.
    //
    HWREG(NVIC_DIS0) = 0xffffffff;
    HWREG(NVIC_DIS1) = 0xffffffff;

    //
    // Return control to the boot loader.  This is a call to the SVC
    // handler in the boot loader.
    //
    (*((void (*)(void))(*(uint32_t *)0x2c)))();
}

void InitCan(void)
{
    tCANMsgObject sCANMessage;
    //
    // For this example CAN0 is used with RX and TX pins on port A0 and A1.
    // GPIO port A needs to be enabled so these pins can be used.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Configure the GPIO pin muxing to select CAN0 functions for these pins.
    // This step selects which alternate function is available for these pins.
    //
    GPIOPinConfigure(GPIO_PA0_CAN0RX);
    GPIOPinConfigure(GPIO_PA1_CAN0TX);

    //
    // Enable the alternate function on the GPIO pins.  The above step selects
    // which alternate function is available.  This step actually enables the
    // alternate function instead of GPIO for these pins.
    //
    GPIOPinTypeCAN(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // The GPIO port and pins have been set up for CAN.  The CAN peripheral
    // must be enabled.
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);

    //
    // Initialize the CAN controller
    //
    CANInit(CAN0_BASE);

    //
    // Set up the bit rate for the CAN bus.  This function sets up the CAN
    // bus timing for a nominal configuration.  You can achieve more control
    // over the CAN bus timing by using the function CANBitTimingSet() instead
    // of this one, if needed.
    // In this example, the CAN bus is set to 125 kHz.  In the function below,
    // g_ui32SysClock is used to determine the
    // clock rate that is used for clocking the CAN peripheral.  This can be
    // replaced with a  fixed value if you know the value of the system clock,
    // saving the extra function call.  For some parts, the CAN peripheral is
    // clocked by a fixed 8 MHz regardless of the system clock in which case
    // the call to SysCtlClockGet() or ui32SysClock should be replaced with
    // 8000000.  Consult the data sheet for more information about CAN
    // peripheral clocking.
    //
    CANBitRateSet(CAN0_BASE, g_ui32SysClock, CANBAUD);

    //
    // Enable the CAN for operation.
    //
    CANEnable(CAN0_BASE);
    //
    // Initialize a message object to be used for receiving the CAN message
    // that sends us back to the CAN bootloader
    sCANMessage.ui32MsgID = CANID;
    sCANMessage.ui32MsgIDMask = 0;
    sCANMessage.ui32Flags = MSG_OBJ_RX_INT_ENABLE;

    //
    // Now load the message object into the CAN peripheral.
    //
    CANMessageSet(CAN0_BASE, 1, &sCANMessage, MSG_OBJ_TYPE_RX);


}

//*****************************************************************************
//
// Demonstrate the use of the boot loader.
//
//*****************************************************************************
int
main(void)
{
    tCANMsgObject sCANMessage;
    uint8_t pui8Data[8];

    //
    // Run from the PLL at 120 MHz.
    //
    g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);
    // Initialize CAN for future bootloader operations
    InitCan();
    //
    // Enable the GPIO port that is used for the on-board LED and switches.
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //
    // Check if the peripheral access is enabled.
    //
    while(!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
    {
    }

    // Configure input switches
    MAP_GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    MAP_GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    //
    // Enable the GPIO pin for the LED (PF0,4). Set the direction as output,
    //  and enable the GPIO pin for digital function.
    //
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, LEDf_PINS);
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, LEDn_PINS);
    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Turn on the LED.
        //
        MAP_GPIOPinWrite(GPIO_PORTF_BASE, LEDf_PINS, MYf_LEDS);
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, LEDn_PINS, MYn_LEDS);

        //
        // Delay for a bit.
        //
        MAP_SysCtlDelay(g_ui32SysClock / 4);

        //
        // Turn off the LED.
        //
        MAP_GPIOPinWrite(GPIO_PORTF_BASE, LEDf_PINS, 0x0);
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, LEDn_PINS, 0x0);

        //
        // Delay for a bit.
        //
        MAP_SysCtlDelay(g_ui32SysClock / 4);
        // Check if SW1 is pressed
        if(MAP_GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0)
        {
            JumpToBootLoader();
        }
        // Check if CAN message received
        if(CANIntStatus(CAN0_BASE, CAN_INT_STS_OBJECT) == 1)
        {
            sCANMessage.pui8MsgData = pui8Data;
            sCANMessage.ui32Flags = MSG_OBJ_NO_FLAGS;
            CANMessageGet(CAN0_BASE, 1, &sCANMessage, true);
            if(pui8Data[0] == NUMBER_LEDS)
            {
                JumpToBootLoader();
            }
        }

    }
}
