/*
 * adc_handler.h
 *
 *  Created on: 08 April 2020
 *      Author: Johannes Ehala, ProLab
 */

#include "cmsis_os2.h"

#ifndef ADC_HANDLER_H_
#define ADC_HANDLER_H_

/* ADC measuring channel location, datasheet p152 */
#define ADC_CHANNEL_LOC	adcPosSelAPORT4XCH9  //PA01 - microphone on tsb0 board

/* Number of ADC samples to measure */
#define ADC_SAMPLES_PER_BATCH		10240 	//10240 - ~1 second of data @ 10kHz sampling speed

/* Sampling rate */
#define ADC_SAMPLES_SEC				10000

/* PRS and DMA channels used */
#define ADC_PRS_CHANNEL				adcPRSSELCh0

/* Number of DMA transfers that one descriptor can handle */
#define DMA_MAX_TRANSFERS			2048

/* Needed number of DMA descriptors */
#define MY_CEIL(x,y) 				(x%y > 0 ? (x/y + 1) : (x/y))
#define NUM_DMA_DESC				MY_CEIL(ADC_SAMPLES_PER_BATCH, DMA_MAX_TRANSFERS)

/* Number of samples in ADC FIFO when DMA is triggered (max. 4) */
#define ADC_SCAN_DVL				4

void adcInit(void);
void adcStartSampling(osMessageQueueId_t measq_id);
void adcLdmaSetup(void);
void adcScanSetup();
void adcLDMAIrq();

#endif /* ADC_HANDLER_H_ */
