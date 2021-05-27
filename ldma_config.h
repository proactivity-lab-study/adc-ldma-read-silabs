/**
 * @file ldma_config.h
 *
 * @author Johannes Ehala
 * Copyright Prolab, TalTech
 * @license MIT
 */

#ifndef LDMA_CONFIGURE_H_
#define LDMA_CONFIGURE_H_

#include "em_ldma.h"

#define ADC_DMA_CHANNEL 0

// Public functions
void ldma_init (void);
void ldma_adc_start (LDMA_Descriptor_t* ldma_adc_descriptor);

#endif // LDMA_CONFIGURE_H_
