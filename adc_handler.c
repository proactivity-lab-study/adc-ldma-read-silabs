/*
 * adc_handler.c
 *
 *  Created on: 08 April 2020
 *      Author: Johannes Ehala, ProLab
 */

//#include "em_chip.h"
#include "em_cmu.h"
#include "em_adc.h"
#include "em_prs.h"
#include "em_timer.h"
#include "cmsis_os2.h"

#include "adc_handler.h"
#include "ldma_handler.h"

/* Buffer to hold ADC samples. */
volatile uint16_t adc_samples_buf[ADC_SAMPLES_PER_BATCH];

/* LDMA transfer descriptors */
LDMA_Descriptor_t descLink[NUM_DMA_DESC];

osMessageQueueId_t mq_id;
static osThreadId_t energyCalc_thread_id;
void calcEnergy(void *argument);


/*****************************************************************************
 * @brief
 * ADC is readout is handled by LDMA, so when ADC finishes LDMA interrupt is
 * actually generated not ADC interrupt. LDMA interrupt handler is elsewhere
 * but it routes ADC related interrupts to this function.
 ******************************************************************************/
void adcLDMAIrq()
{
		//stop timer and ADC
		TIMER_Enable(TIMER0, false);

		if(osThreadFlagsSet(energyCalc_thread_id, 0x00000001U) != osOK)
		{
			//Handle error
		}
		//? LDMA_IntDisable(1 << MIC_ADC_DMA_CHANNEL);
		//? LDMA_StopTransfer(1 << MIC_ADC_DMA_CHANNEL);
}

void adcInit(void)
{
	/* Enable clocks */
	CMU_ClockEnable(cmuClock_ADC0, true);
	CMU_ClockEnable(cmuClock_TIMER0, true);
	CMU_ClockEnable(cmuClock_PRS, true);

	/* Configure DMA transfer from ADC to RAM */
	adcLdmaSetup();

	/* Configure ADC stream sampling and TIMER trigger through PRS */
	adcScanSetup();

	/* Creat thread that manipulates ADC results - this is optional */
	const osThreadAttr_t eneCalc_thread_attr = { .name = "eneCalc" };
    energyCalc_thread_id = osThreadNew(calcEnergy, NULL, &eneCalc_thread_attr); 
}

void adcStartSampling(osMessageQueueId_t measq_id)
{
	mq_id = measq_id;
	
	/* Start LDMA for ADC to memory transfer */
	adcLDMAstart(&descLink[0]);

	/* ADC is started by starting the timer */
	TIMER_Enable(TIMER0, true);
}

/*****************************************************************************
 * @brief Create a list of linked descriptors, to handle all of adc_samples_buf 
 ******************************************************************************/

void adcLdmaSetup(void)
{
	uint16_t i = 0;

	/* Create needed number of linked descriptors for adc_samples_buf */
	for (i=0; i <= (NUM_DMA_DESC - 1); i++)
	{
		descLink[i].xfer.structType   = ldmaCtrlStructTypeXfer;
		descLink[i].xfer.structReq    = 0;
		descLink[i].xfer.byteSwap     = 0;
		/* Block size is 4 because of ADC FIFO has 4 samples */
		descLink[i].xfer.blockSize    = ldmaCtrlBlockSizeUnit4; 
		descLink[i].xfer.doneIfs      = 0;
		descLink[i].xfer.reqMode      = ldmaCtrlReqModeBlock;
		descLink[i].xfer.decLoopCnt   = 0;
		/* Start transfer only when ADC FIFO is full */
		descLink[i].xfer.ignoreSrec   = 1;
		descLink[i].xfer.srcInc       = ldmaCtrlSrcIncNone;
		/* ADC sample is 16 bits (though 12 are significant) */
		descLink[i].xfer.size         = ldmaCtrlSizeHalf;
		descLink[i].xfer.dstInc       = ldmaCtrlDstIncOne;
		descLink[i].xfer.srcAddrMode  = ldmaCtrlSrcAddrModeAbs;
		descLink[i].xfer.dstAddrMode  = ldmaCtrlDstAddrModeAbs;
		descLink[i].xfer.srcAddr      = (uint32_t)(&ADC0->SCANDATA);
		/* Increase the destination data pointer for 16 bit values */
		descLink[i].xfer.dstAddr      = (uint32_t)(&adc_samples_buf) + i * DMA_MAX_TRANSFERS * 2;
		descLink[i].xfer.linkMode     = ldmaLinkModeRel;

		/* Number of transfers for this descriptor */
		if(i == (NUM_DMA_DESC - 1))
		{
			/* Last descriptor, will it be full or not? */
			if((ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) == 0)
			{
				descLink[i].xfer.xferCnt = DMA_MAX_TRANSFERS - 1;
			}
			else descLink[i].xfer.xferCnt = (ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) - 1;
		}
		else descLink[i].xfer.xferCnt = DMA_MAX_TRANSFERS - 1;

		/* Interrupt should be set for last descriptor of adc_samples_buf */
		if(i == (NUM_DMA_DESC - 1))descLink[i].xfer.doneIfs = 1;

		/* Link this discriptor to the next one or don't if it is the last */
		if(i == (NUM_DMA_DESC - 1))
		{
			descLink[i].xfer.link         = 0;
			descLink[i].xfer.linkAddr     = 0;
		}
		else
		{
			descLink[i].xfer.link         = 1;
			descLink[i].xfer.linkAddr     = (1) * 4;
		}
	}
}

/****************************************************************************
 * @brief Configure TIMER to trigger ADC through PRS at a set sample rate
 *****************************************************************************/
void adcScanSetup()
{
	ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
	ADC_InitScan_TypeDef scanInit = ADC_INITSCAN_DEFAULT;
	TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;

	/* Initialise common ADC parameters*/
	init.prescale = ADC_PrescaleCalc(16000000, 0);
	ADC_Init(ADC0, &init);

	/* Initialise scan conversion */
	ADC_ScanSingleEndedInputAdd(&scanInit, adcScanInputGroup0, ADC_CHANNEL_LOC);

	scanInit.prsSel = ADC_PRS_CHANNEL;
	scanInit.reference = adcRefVDD;
	scanInit.prsEnable = true;
	scanInit.fifoOverwrite = true;
	ADC_InitScan(ADC0, &scanInit);

	/* Set scan data valid level (DVL) to trigger */
	ADC0->SCANCTRLX |= (ADC_SCAN_DVL - 1) << _ADC_SCANCTRLX_DVL_SHIFT;

	/* Clear the FIFOs and pending interrupt */
	ADC0->SCANFIFOCLEAR = ADC_SCANFIFOCLEAR_SCANFIFOCLEAR;

	/* Configure and initialise TIMER */
	timerInit.enable = false;
	TIMER_Init(TIMER0, &timerInit);
	TIMER_TopSet(TIMER0,  CMU_ClockFreqGet(cmuClock_TIMER0)/ADC_SAMPLES_SEC);
	
	/* Connect PRS channel 0 to TIMER overflow */
	PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_TIMER0, PRS_CH_CTRL_SIGSEL_TIMER0OF, prsEdgeOff);
}

/****************************************************************************
 * @brief 
 *	This loop waits for thread flag to be set by adcLDMAIrq() then it becomes 
 *	active and calculates signal energy of ADC readout. This value is then 
 *	passed up to the application. 
 *
 *	Disclaimer: The signal measured by the ADC is an elecrical signal, and its
 * 	unit would be Joule, but since I don't know the exact load that the signal
 *	is driving I can't account for the load. And so the energy I calculate here  
 *  just indicates of the presence or absence of a signal (and its relative 
 *	strength), not the actual electrical energy. 
 *
 *	Read about signal energy 
 *	https://www.gaussianwaves.com/2013/12/power-and-energy-of-a-signal/
 *****************************************************************************/
void calcEnergy(void *argument)
{
	#define ADCREFVOL 3.3f /* Assuming ADC reference voltage is Vdd */
	#define ADCBITS12 4095 /* Assuming 12 bit ADC conversion is used */

	static float energy;
	uint32_t i;
	float adcBias, volEnergy, vol;

    for (;;)
    {
		osThreadFlagsClear(0x00000001U);
		osThreadFlagsWait(0x00000001U, osFlagsWaitAny, osWaitForever);

		/* Find energy of measured signal */
		/* Energy is found by subtracting bias from every sample and */
		/* then adding together the square values of all samples. */
		/* Energy is small if there is no signal (just measurement */
		/* noise) and larger when a signal is present. */

		adcBias = volEnergy = vol = 0;

		for(i=0;i<ADC_SAMPLES_PER_BATCH;i++)
		{
			adcBias += adc_samples_buf[i];
		}
		adcBias /= ADC_SAMPLES_PER_BATCH;

		for(i=0;i<ADC_SAMPLES_PER_BATCH;i++)
		{
			vol = (float)(adc_samples_buf[i]-adcBias) / ADCBITS12;//subtract bias and normalize
			volEnergy += vol * vol; //square
		}
		energy = volEnergy * ADCREFVOL * ADCREFVOL; //account for actual voltage

		if(osMessageQueuePut(mq_id, &energy, 0U, 0U) != osOK)
		{
			//ERROR
		}
	}
}
