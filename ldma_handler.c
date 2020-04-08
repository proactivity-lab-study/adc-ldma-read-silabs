/*
 * ldma_handler.c
 * 
 * Here LDMA is initialized and different LDMA channels are configured and started.
 * Also LDMA IRQ handler is here, because there is only one LDMA IRQ. Interrupts 
 * branch out from here based on the channels they apply to.
 *
 *     Created: 05 Jul 2019
 *      Author: Johannes Ehala
 */

#include "em_cmu.h" //for clocks
#include "adc_handler.h"
#include "ldma_handler.h"

/******************************************************************************
 * @brief
 *   LDMA IRQ handler.
 ******************************************************************************/

void LDMA_IRQHandler(void)
{
	/* Get all pending and enabled interrupts. */
	uint32_t pending = LDMA_IntGetEnabled();

	/* Loop here on an LDMA error to enable debugging. */
	while (pending & LDMA_IF_ERROR) {}

	if(pending & (1 << ADC_DMA_CHANNEL))
	{
		/* Clear interrupt flag. */
		LDMA->IFC = (1 << ADC_DMA_CHANNEL);

		//notify mic controller-driver
		adcLDMAIrq();
	}
}

/******************************************************************************
 * @brief Initialize the LDMA controller
 ******************************************************************************/
void ldmainit(void)
{	
	/* Initialize the LDMA controller */
	LDMA_Init_t init = LDMA_INIT_DEFAULT;//only priority based arbitration, no round-robin
	LDMA_Init(&init);

	CMU_ClockEnable(cmuClock_LDMA, true);
	//CMU_ClockEnable(cmuClock_PRS, true);peripheral reflex system must be enabled
}

/******************************************************************************
 * @brief Start LDMA for ADC to memory transfer
 ******************************************************************************/
void adcLDMAstart(LDMA_Descriptor_t* adcDescriptor)
{
	/* Macro for scan mode ADC */
	LDMA_TransferCfg_t adcScanTx = LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_ADC0_SCAN);
	
	LDMA_IntEnable(1 << ADC_DMA_CHANNEL);
	NVIC_ClearPendingIRQ(LDMA_IRQn);
	NVIC_EnableIRQ(LDMA_IRQn);

	/* Start ADC LMDA transfer */
	LDMA_StartTransfer(ADC_DMA_CHANNEL, &adcScanTx, adcDescriptor);
}
