/**
 * @file ldma_config.c
 * 
 * @brief
 * Here LDMA is initialized and different LDMA channels are configured and started.
 * Also LDMA IRQ handler is here. Interrupts branch out from here based on the 
 * channels they apply to. 
 *
 * Currently only one LDMA channel is used, it is used for ADC measurment transfer 
 * from ADC scan result register to designated memory. The LDMA descriptor with the 
 * details of the ADC-memory transfer is created on the ADC configuration side.
 *
 * @author Johannes Ehala
 * Copyright Prolab, TalTech
 * @license MIT
 */

#include "em_cmu.h"
#include "adc_config.h"
#include "ldma_config.h"

/**
 * @brief
 *   LDMA IRQ handler.
 */

void LDMA_IRQHandler(void)
{
    // Get all pending and enabled interrupts.
    uint32_t pending = LDMA_IntGetEnabled();

    // Loop here on an LDMA error to enable debugging.
    while (pending & LDMA_IF_ERROR) {}

    if (pending & (1 << ADC_DMA_CHANNEL))
    {
        // Clear interrupt flag.
        LDMA->IFC = (1 << ADC_DMA_CHANNEL);

        // Notify ADC controller-driver
        adc_ldma_irq();
    }
}

/**
 * @brief Initialize the LDMA controller.
 */
void ldma_init(void)
{	
    // Initialize the LDMA controller.
    static LDMA_Init_t init = LDMA_INIT_DEFAULT; // Only priority based arbitration, no round-robin.
    LDMA_Init(&init);

    CMU_ClockEnable(cmuClock_LDMA, true);
    //CMU_ClockEnable(cmuClock_PRS, true);peripheral reflex system must be enabled
}

/**
 * @brief Start LDMA for ADC to memory transfer.
 */
void ldma_adc_start(LDMA_Descriptor_t* ldma_adc_descriptor)
{
    // Macro for scan mode ADC.
    static LDMA_TransferCfg_t adc_scan_tx = LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_ADC0_SCAN);

    LDMA_IntEnable(1 << ADC_DMA_CHANNEL);
    NVIC_ClearPendingIRQ(LDMA_IRQn);
    NVIC_EnableIRQ(LDMA_IRQn);

    // Start ADC LMDA transfer.
    LDMA_StartTransfer(ADC_DMA_CHANNEL, &adc_scan_tx, ldma_adc_descriptor);
}
