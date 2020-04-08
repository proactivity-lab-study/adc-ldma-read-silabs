/*
 * ldma_handler.h
 *
 *  Created on: 05 Jul 2019
 *      Author: Johannes Ehala
 */

#ifndef LDMA_CONFIGURE_H_
#define LDMA_CONFIGURE_H_

#include "em_ldma.h"

#define ADC_DMA_CHANNEL	0

void ldmainit(void);
void adcLDMAstart(LDMA_Descriptor_t* micDescriptor);

#endif /* LDMA_CONFIGURE_H_ */
