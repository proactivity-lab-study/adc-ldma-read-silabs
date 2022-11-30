/**
 * @file adc_config.c
 *
 * @author Johannes Ehala
 * Copyright Prolab, TalTech
 * @license MIT
 */

#include "em_cmu.h"
#include "em_adc.h"
#include "em_prs.h"
#include "em_timer.h"
#include "cmsis_os2.h"

#include "adc_config.h"
#include "ldma_config.h"

// LDMA transfer descriptors
static LDMA_Descriptor_t desc_linked_list[NUM_DMA_DESC*2]; // *2 becaus ping-pong buffer

static osThreadId_t adc_thread_id;
static volatile uint16_t *adc_samples_buf;

static void adc_ldma_setup ();
static void adc_scan_setup ();

/**
 * @brief
 * ADC readout is handled by LDMA, so when ADC finishes LDMA interrupt is
 * actually generated not ADC interrupt. LDMA interrupt handler is elsewhere
 * but it routes ADC related interrupts to this function.
 */
void adc_ldma_irq()
{
    // There is no way to determine which buffer was filled, other than
    // we keep track of ping and pong since beginning.
    static volatile bool ping = true;
    
    if(ping)
    {
        ping = false;
        if (osThreadFlagsSet(adc_thread_id, ADC_THREAD_READ_DONE_PING_FLAG) != osOK)
        {
            // Handle error
            // PLATFORM_LedsSet(PLATFORM_LedsGet()^4);
        }
    }
    else
    {
        ping = true;
        if (osThreadFlagsSet(adc_thread_id, ADC_THREAD_READ_DONE_PONG_FLAG) != osOK)
        {
            // Handle error
            // PLATFORM_LedsSet(PLATFORM_LedsGet()^4);
        }
    }
}

void adc_init(osThreadId_t thread_id, volatile uint16_t *samples_buf)
{
    adc_thread_id = thread_id;
    adc_samples_buf = samples_buf;

    // Enable clocks 
    CMU_ClockEnable(cmuClock_ADC0, true);
    CMU_ClockEnable(cmuClock_TIMER0, true);
    //CMU_ClockEnable(cmuClock_PRS, true);

    // Configure DMA transfer from ADC to memory. 
    adc_ldma_setup();

    // Configure ADC stream sampling and TIMER trigger through PRS.
    adc_scan_setup();
}

/**
 * @brief   Starts ADC scan measurements.
 * 
 * @param   thread_id 
 *          ID of thread that should be resumed once ADC measurements have 
 *          finished
 * 
 * @param   samples_buf
 *          pointer to memory area where to store ADC measurements
 */
void adc_start_sampling()
{

    // Start LDMA for ADC to memory transfer.
    ldma_adc_start(&desc_linked_list[0]);

    // ADC is started by starting the timer.
    TIMER_Enable(TIMER0, true);
}

/**
 * @brief Create a list of linked descriptors to handle all of adc_samples_buf.
 *        Using ping-pong buffer scheme. First half of adc_samples_buf is ping,
 *        second half is pong.
 */
static void adc_ldma_setup(void)
{
    static uint16_t i = 0;

    // Create needed number of linked descriptors for adc_samples_buf.
    for (i = 0; i <= (NUM_DMA_DESC*2 - 1); i++)
    {
        desc_linked_list[i].xfer.structType   = ldmaCtrlStructTypeXfer;
        desc_linked_list[i].xfer.structReq    = 0;
        desc_linked_list[i].xfer.byteSwap     = 0;
        desc_linked_list[i].xfer.xferCnt      = DMA_MAX_TRANSFERS - 1;
        // Block size is 4 because ADC FIFO has 4 samples.
        desc_linked_list[i].xfer.blockSize    = ldmaCtrlBlockSizeUnit4; 
        desc_linked_list[i].xfer.doneIfs      = 0;
        desc_linked_list[i].xfer.reqMode      = ldmaCtrlReqModeBlock;
        desc_linked_list[i].xfer.decLoopCnt   = 0;
        // Start transfer only when ADC FIFO is full.
        desc_linked_list[i].xfer.ignoreSrec   = 1;
        desc_linked_list[i].xfer.srcInc       = ldmaCtrlSrcIncNone;
        // ADC sample is 16 bits (though 12 are significant).
        desc_linked_list[i].xfer.size         = ldmaCtrlSizeHalf;
        desc_linked_list[i].xfer.dstInc       = ldmaCtrlDstIncOne;
        desc_linked_list[i].xfer.srcAddrMode  = ldmaCtrlSrcAddrModeAbs;
        desc_linked_list[i].xfer.dstAddrMode  = ldmaCtrlDstAddrModeAbs;
        desc_linked_list[i].xfer.srcAddr      = (uint32_t)(&ADC0->SCANDATA);
        desc_linked_list[i].xfer.linkMode     = ldmaLinkModeRel;
        desc_linked_list[i].xfer.link         = 1;

        if (0 == i) // First descriptor of ping.
        {
            desc_linked_list[i].xfer.dstAddrMode    = ldmaCtrlDstAddrModeAbs;
            desc_linked_list[i].xfer.dstAddr        = (uint32_t)adc_samples_buf; // Memory address
        }
        else if (NUM_DMA_DESC == i) // First descriptor of pong.
        {
            desc_linked_list[i].xfer.dstAddrMode    = ldmaCtrlDstAddrModeAbs;
            desc_linked_list[i].xfer.dstAddr        = (uint32_t)(adc_samples_buf) + ADC_SAMPLES_PER_BATCH * 2; // Memory address
        }
        else // All the rest.
        {
            desc_linked_list[i].xfer.dstAddrMode    = ldmaCtrlDstAddrModeRel;
            desc_linked_list[i].xfer.dstAddr        = 0; // Offset from source address of previous transfer.
        }

        // Interrupt should be set for last descriptor of ping and pong buffers.
        // Number of transfers for last descriptor might be less than 2048.
        if ((NUM_DMA_DESC - 1) == i)
        {
            desc_linked_list[i].xfer.doneIfs  = 1;
            if((ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) != 0)
            desc_linked_list[i].xfer.xferCnt  = (ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) - 1;
        }
        if ((NUM_DMA_DESC*2 - 1) == i)
        {
            desc_linked_list[i].xfer.doneIfs  = 1;
            if((ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) != 0)
            desc_linked_list[i].xfer.xferCnt  = (ADC_SAMPLES_PER_BATCH % DMA_MAX_TRANSFERS) - 1;
        }

        // Link this discriptor to the next one or to beginning if it is the last.
        if ((NUM_DMA_DESC*2 - 1) == i)
        {
            /*Points to first descriptor in list*/
            desc_linked_list[i].xfer.linkAddr = -((NUM_DMA_DESC*2)-1) * 4;
        }
        else
        {
            desc_linked_list[i].xfer.linkAddr     = (1) * 4;
        }
    }
}

/**
 * @brief Configure TIMER to trigger ADC through PRS at a set sample rate.
 */
static void adc_scan_setup()
{
    static ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
    static ADC_InitScan_TypeDef scan_init = ADC_INITSCAN_DEFAULT;
    static TIMER_Init_TypeDef timer_init = TIMER_INIT_DEFAULT;

    // Initialise common ADC parameters.
    init.prescale = ADC_PrescaleCalc(16000000, 0);
    ADC_Init(ADC0, &init);

    // Initialise scan conversion.
    ADC_ScanSingleEndedInputAdd(&scan_init, adcScanInputGroup0, ADC_CHANNEL_LOC);

    scan_init.prsSel = ADC_PRS_CHANNEL;
    scan_init.reference = adcRefVDD;
    scan_init.prsEnable = true;
    scan_init.fifoOverwrite = true;
    ADC_InitScan(ADC0, &scan_init);

    // Set scan data valid level (DVL) to trigger.
    ADC0->SCANCTRLX |= (ADC_SCAN_DVL - 1) << _ADC_SCANCTRLX_DVL_SHIFT;

    // Clear the FIFOs and pending interrupts.
    ADC0->SCANFIFOCLEAR = ADC_SCANFIFOCLEAR_SCANFIFOCLEAR;

    // Configure and initialise TIMER.
    timer_init.enable = false;
    TIMER_Init(TIMER0, &timer_init);
    TIMER_TopSet(TIMER0,  CMU_ClockFreqGet(cmuClock_TIMER0)/ADC_SAMPLES_SEC);

    // Connect PRS channel 0 to TIMER overflow.
    PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_TIMER0, PRS_CH_CTRL_SIGSEL_TIMER0OF, prsEdgeOff);
}
