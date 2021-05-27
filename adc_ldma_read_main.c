/**
 * @file adc_ldma_read_main.c
 *
 * @brief
 * A thread sets up ADC for scan measurements and LDMA for ADC results
 * transfer from ADC register to memory. Then starts ADC and waits for
 * results buffer to be filled. Once ADC measurements and LDMA transfer
 * have finished energy of the measurement signal is calculated and 
 * logged. Then a new batch is measured and the loop repeats.
 *
 *
 * Copyright Thinnect Inc. 2019
 *
 * Copyright Prolab, TalTech
 * @author Johannes Ehala
 * 
 * @license MIT
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "retargetserial.h"
#include "em_chip.h"
#include "em_rmu.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_msc.h"
#include "em_adc.h"
#include "em_usart.h"
#include "cmsis_os2.h"

#include "platform.h"
#include "SignatureArea.h"
#include "DeviceSignature.h"

#include "basic_rtos_logger_setup.h"

#include "loglevels.h"
#define __MODUUL__ "main"
#define __LOG_LEVEL__ (LOG_LEVEL_main & BASE_LOG_LEVEL)
#include "log.h"

// Include the information header binary
#include "incbin.h"
INCBIN(Header, "header.bin");

#include "adc_config.h"
#include "ldma_config.h"

// Buffer to hold ADC samples. 
static volatile uint16_t adc_samples_buf[ADC_SAMPLES_PER_BATCH];
static osThreadId_t adc_thread_id;

static float calc_signal_energy ();

/**
 * @brief
 * Configure ADC for scan measurement and periodically perform measurements. 
 * Calculate signal energy.
 *
 * @details 
 * LDMA and ADC are initialised and an ADC scan measurment is then started. 
 * LDMA transfer then fills the buffer adc_samples_buf[] with ADC measurements,
 * while this thread waits for a thread flag to be set by adc_ldma_irq(). 
 * Once the flag is set signal energy of ADC measurements is calculated and 
 * sent to log.
 */
void adc_loop ()
{
    #define TIME_DELAY_BETWEEN_MEASUREMENTS      2000U // OS ticks
    static float signal_energy;

    // Initialise LDMA for ADC->memory data transfer.
    ldma_init();

    // ADC initialisation, also creates LDMA descriptor linked list for ADC->memory transfer.
    adc_init();

    for (;;)
    {
        adc_start_sampling(adc_thread_id, adc_samples_buf);
        info1("ADC started");

        // Wait for measurements.
        osThreadFlagsClear(ADC_THREAD_READ_DONE_FLAG);
        osThreadFlagsWait(ADC_THREAD_READ_DONE_FLAG, osFlagsWaitAny, osWaitForever);

        info1("ADC stopped");
        
        signal_energy = calc_signal_energy();

        info1("Signal energy %lu", (uint32_t) signal_energy);

        // Wait a bit until next measurements.
        osDelay(TIME_DELAY_BETWEEN_MEASUREMENTS);
    }
}

/**
 * @brief 
 * Calculate energy of measured signal. 
 * 
 * @details 
 * Energy is calculated by subtracting bias from every sample and then adding 
 * together the square values of all samples. Energy is small if there is no 
 * signal (just measurement noise) and larger when a signal is present. 
 *
 * Disclaimer: The signal measured by the ADC is an elecrical signal, and its
 * unit would be joule, but since I don't know the exact load that the signal
 * is driving I can't account for the load. And so the energy I calculate here  
 * just indicates the presence or absence of a signal (and its relative 
 * strength), not the actual electrical energy in joules. 
 *
 * Read about signal energy 
 * https://www.gaussianwaves.com/2013/12/power-and-energy-of-a-signal/
 *
 * @warning 
 * ADC reference voltage is assumed to be 3.3V and ADC conversion is assumed
 * to be 16 bits. How could the application know the real values?
 *
 * @return Energy value.
 */
float calc_signal_energy()
{
    #define ADCREFVOL 3.3f // Assuming ADC reference voltage is Vdd and that Vdd = 3.3 V. 
    #define ADCBITS12 4095 // Assuming 12 bit ADC conversion is used.
    
    #warning "Assuming ADC ref. voltage is 3.3 V and ADC conversion is 16 bits"
    
    static float energy;
    static uint32_t i;
    static float adc_bias, vol_energy, vol;

    adc_bias = vol_energy = vol = 0;

    for (i = 0; i < ADC_SAMPLES_PER_BATCH; i++)
    {
        adc_bias += adc_samples_buf[i];
    }
    adc_bias /= ADC_SAMPLES_PER_BATCH;

    for (i = 0; i < ADC_SAMPLES_PER_BATCH; i++)
    {
        vol = (float)(adc_samples_buf[i] - adc_bias) / ADCBITS12; // Subtract bias and normalize.
        vol_energy += vol * vol;
    }
    energy = vol_energy * ADCREFVOL * ADCREFVOL; // Account for actual voltage.

    return energy;
}

int main ()
{
    PLATFORM_Init();

    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_PRS, true);

    // LEDs
    PLATFORM_LedsInit();

    // Configure debug output
    basic_noos_logger_setup();

    info1("ADC-LDMA-read "VERSION_STR" (%d.%d.%d)", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    // Initialize OS kernel
    osKernelInitialize();

    // Create a thread to manage ADC sampling.
    const osThreadAttr_t adc_thread_attr = { .name = "adc-loop" };
    adc_thread_id = osThreadNew(adc_loop, NULL, &adc_thread_attr);

    if (osKernelReady == osKernelGetState())
    {
        basic_rtos_logger_setup();

        // Start the kernel
        osKernelStart();
    }
    else
    {
        err1("!osKernelReady");
    }

    for (;;);
}
