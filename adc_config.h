/**
 * @file adc_config.h
 *
 * @author Johannes Ehala
 * Copyright Prolab, TalTech
 * @license MIT
 */

#include "cmsis_os2.h"

#ifndef ADC_HANDLER_H_
#define ADC_HANDLER_H_

// ADC measuring channel location, see datasheet p202 (EFR32MG12 Gecko Multi-ProtocolWireless SoC Family Data Sheet).
#define ADC_CHANNEL_LOC             adcPosSelAPORT4XCH9  // PA01 - microphone on tsb0 board

// Number of ADC samples to measure.
#define ADC_SAMPLES_PER_BATCH       2500	// 10000 samples @ 10kHz  = 1 second of signal

// Sampling rate
#define ADC_SAMPLES_SEC             5000

// PRS and DMA channels used.
#define ADC_PRS_CHANNEL             adcPRSSELCh0

// Number of DMA transfers that one descriptor can handle.
#define DMA_MAX_TRANSFERS           2048

// Needed number of DMA descriptors.
#define MY_CEIL(x,y)             ((x) % (y) > 0 ? ((x) / (y) + 1) : ((x) / (y)))
#define NUM_DMA_DESC             MY_CEIL(ADC_SAMPLES_PER_BATCH, DMA_MAX_TRANSFERS)
//#define NUM_DMA_DESC 3

#define ADC_THREAD_READ_DONE_PING_FLAG   0x00000001U
#define ADC_THREAD_READ_DONE_PONG_FLAG   0x00000002U

// Number of samples in ADC FIFO when DMA is triggered (max. 4).
#define ADC_SCAN_DVL                4

// Public functions
void adc_init (osThreadId_t thread_id, volatile uint16_t *samples_buf);
void adc_start_sampling ();
void adc_ldma_irq ();

#endif // ADC_CONFIG_H_ 
