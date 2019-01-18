/*******************************************************************************
 * Copyright (c) 2014-2015 IBM Corporation.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    IBM Zurich Research Lab - initial API, implementation and documentation
 *******************************************************************************/

#include "gpio.h"
#include "spi.h"
#include "max32660.h"
#include "lmic.h"
#include "tmr.h"
#include "tmr_utils.h"

// -----------------------------------------------------------------------------
// I/O

//NSS cannot be on pin 7 otherwise we lose control of it to the SPI Lib
//Pin 7 cannot be used for anything else
#define NSS_PIN			   PIN_12

//#define RXTX_PIN		   PIN_10

#define RST_PIN            PIN_9

#define DIO0_PIN           PIN_2
#define DIO1_PIN           PIN_3
#define DIO2_PIN           PIN_8

#define MSEC_TO_RSSA(x) (0 - ((x * 256) / 1000)) /* Converts a time in milleseconds to the equivalent RSSA register value. */

// HAL state
static struct {
    int irqlevel;
    u4_t ticks;
} HAL;

void gpio_isr(void *cbdata);

// -----------------------------------------------------------------------------
// I/O

#ifdef NSS_PIN
const gpio_cfg_t nss_pin_config = {
	PORT_0,
	NSS_PIN,
	GPIO_FUNC_OUT,
	GPIO_PAD_NONE
};
#endif

#ifdef RXTX_PIN
const gpio_cfg_t rxtx_pin_config = {
	PORT_0,
	RXTX_PIN,
	GPIO_FUNC_OUT,
	GPIO_PAD_NONE
};
#endif

#ifdef RST_PIN
const gpio_cfg_t rst_pin_config = {
	PORT_0,
	RST_PIN,
	GPIO_FUNC_OUT,
	GPIO_PAD_PULL_UP
};
#endif

#ifdef DIO0_PIN
const gpio_cfg_t dio0_pin_config = {
	PORT_0,
	DIO0_PIN,
	GPIO_FUNC_IN,
	GPIO_PAD_NONE
};
#endif

#ifdef DIO1_PIN
const gpio_cfg_t dio1_pin_config = {
	PORT_0,
	DIO1_PIN,
	GPIO_FUNC_IN,
	GPIO_PAD_NONE
};
#endif

#ifdef DIO2_PIN
const gpio_cfg_t dio2_pin_config = {
	PORT_0,
	DIO2_PIN,
	GPIO_FUNC_IN,
	GPIO_PAD_NONE
};
#endif

static void hal_io_init () {

#ifdef NSS_PIN
	GPIO_Config(&nss_pin_config);
	GPIO_OutSet(&nss_pin_config);
#endif

#ifdef RXTX_PIN
	GPIO_Config(&rxtx_pin_config);
	GPIO_OutClr(&rxtx_pin_config);
#endif

#ifdef RST_PIN
	GPIO_Config(&rst_pin_config);
	GPIO_OutClr(&rst_pin_config);
#endif

#ifdef DIO0_PIN
	GPIO_Config(&dio0_pin_config);
	GPIO_RegisterCallback(&dio0_pin_config, gpio_isr, &dio0_pin_config);
	GPIO_IntConfig(&dio0_pin_config, GPIO_INT_EDGE, GPIO_INT_RISING);
	GPIO_IntEnable(&dio0_pin_config);
#endif

#ifdef DIO1_PIN
	GPIO_Config(&dio1_pin_config);
	GPIO_RegisterCallback(&dio1_pin_config, gpio_isr, &dio1_pin_config);
	GPIO_IntConfig(&dio1_pin_config, GPIO_INT_EDGE, GPIO_INT_RISING);
	GPIO_IntEnable(&dio1_pin_config);
#endif

#ifdef DIO2_PIN
	GPIO_Config(&dio2_pin_config);
	GPIO_RegisterCallback(&dio2_pin_config, gpio_isr, &dio2_pin_config);
	GPIO_IntConfig(&dio2_pin_config, GPIO_INT_EDGE, GPIO_INT_RISING);
	GPIO_IntEnable(&dio2_pin_config);
#endif

#if defined(DIO0_PIN) || defined(DIO1_PIN) || defined(DIO2_PIN)
	NVIC_EnableIRQ((IRQn_Type)MXC_GPIO_GET_IRQ(PORT_0));
#endif
}

// val ==1  => tx 1, rx 0 ; val == 0 => tx 0, rx 1
void hal_pin_rxtx (u1_t val) {
#ifdef RXTX_PIN
	ASSERT(val == 1 || val == 0);
	if (val == 0) GPIO_OutClr(&rxtx_pin_config);
	if (val == 1) GPIO_OutSet(&rxtx_pin_config);
#endif
}


// set radio NSS pin to given value
void hal_pin_nss (u1_t val) {
#ifdef NSS_PIN
	if (val == 0) GPIO_OutClr(&nss_pin_config);
	if (val == 1) GPIO_OutSet(&nss_pin_config);
#endif
}

// set radio RST pin to given value (or keep floating!)
void hal_pin_rst (u1_t val) {
#ifdef RST_PIN
    if(val == 0 || val == 1) { // drive pin
        GPIO_Config(&rst_pin_config);
        if (val == 0) GPIO_OutClr(&rst_pin_config);
        if (val == 1) GPIO_OutSet(&rst_pin_config);
    } else { // keep pin floating
        gpio_cfg_t rstFloating = {
        	PORT_0,
			RST_PIN,
			GPIO_FUNC_IN,
			GPIO_PAD_NONE
        };
        GPIO_Config(&rstFloating);
    }
#endif
}

extern void radio_irq_handler(u1_t dio);

// generic EXTI IRQ handler for all channels
void gpio_isr(void *cbdata)
{
    gpio_cfg_t* interruptSource = (gpio_cfg_t*)cbdata;

#ifdef DIO0_PIN
    if (interruptSource->mask == DIO0_PIN)
    	radio_irq_handler(0);
#endif

#ifdef DIO1_PIN
    if (interruptSource->mask == DIO1_PIN)
        radio_irq_handler(1);
#endif

#ifdef DIO2_PIN
    if (interruptSource->mask == DIO2_PIN)
        radio_irq_handler(2);
#endif
}

// -----------------------------------------------------------------------------
// SPI

// for sx1272 and 1276

#define SCK_PIN    PIN_6
#define MISO_PIN   PIN_4
#define MOSI_PIN   PIN_5

#define SPI    	   SPI0A

spi_req_t spi_req = {
	.bits = 8,
	.len = 1,
	.width = SPI17Y_WIDTH_1,
	.tx_num = 0,
	.rx_num = 0
};

static void hal_spi_init () {
    
    if (SPI_Init(SPI, 0, 125000) != E_NO_ERROR)
    	hal_failed();
    hal_spi(0);
}

// perform SPI transaction with radio
u1_t hal_spi (u1_t out) {
	uint8_t spi_rx_data[1];
	uint8_t spi_tx_data[1];

	memset(spi_rx_data, 0x0, 1);
	spi_tx_data[0] = out;

	spi_req.rx_data = spi_rx_data;
	spi_req.tx_data = spi_tx_data;

    SPI_MasterTrans(SPI, &spi_req);
    return spi_rx_data[0];
}


// -----------------------------------------------------------------------------
// TIME

void hal_tick_handler(void) {
	TMR_IntClear(MXC_TMR1);
	HAL.ticks++;
}

void hal_time_init () {

	NVIC_SetVector(TMR1_IRQn, hal_tick_handler);
	NVIC_EnableIRQ(TMR1_IRQn);
	//NVIC_SetPriority(TMR1_IRQn, 0x70);

	tmr_cfg_t tmrConfig;

	TMR_Disable(MXC_TMR1);
	TMR_Init(MXC_TMR1, TMR_PRES_1024, 0);

	tmrConfig.mode = TMR_MODE_CONTINUOUS;
	tmrConfig.cmp_cnt = 0xffff;
	tmrConfig.pol = 0;

	TMR_Config(MXC_TMR1, &tmrConfig);
	TMR_Enable(MXC_TMR1);

}

u4_t hal_ticks () {
    hal_disableIRQs();
    u4_t t = HAL.ticks;
	u4_t cnt = TMR_GetCount(MXC_TMR1);
	if (TMR_IntStatus(MXC_TMR1)) {
		cnt = TMR_GetCount(MXC_TMR1);
		t++;
	}
    hal_enableIRQs();
    return ((t << 16) | cnt);
}

// return modified delta ticks from now to specified ticktime (0 for past, FFFF for far future)
static u2_t deltaticks (u4_t time) {
    u4_t t = hal_ticks();
    s4_t d = time - t;
    if( d<=0 ) return 0;    // in the past
    if( (d>>16)!=0 ) return 0xFFFF; // far ahead
    return (u2_t)d;
}

void hal_waitUntil (u4_t time) {
    while( deltaticks(time) != 0 ); // busy wait until timestamp is reached
}

// check and rewind for target time
u1_t hal_checkTimer (u4_t time) {
    return deltaticks(time) <= 0;
}

// -----------------------------------------------------------------------------
// IRQ

void hal_disableIRQs () {
    __disable_irq();
    HAL.irqlevel++;
}

void hal_enableIRQs () {
    if(--HAL.irqlevel == 0) {
        __enable_irq();
    }
}

void hal_sleep () {
    //Currently not implemented
}

// -----------------------------------------------------------------------------

void hal_init () {
    memset(&HAL, 0x00, sizeof(HAL));
    hal_disableIRQs();

    // configure radio I/O and interrupt handler
    hal_io_init();
    // configure radio SPI
    hal_spi_init();
    // configure timer and interrupt handler
    hal_time_init();

    hal_enableIRQs();
}

void hal_failed () {
    // HALT...
    hal_disableIRQs();
    debug_str("HAL FAILURE");
    hal_sleep();
    while(1);
}

