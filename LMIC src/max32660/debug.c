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
#include "led.h"
#include "board.h"
#include "uart.h"
#include "mxc_sys.h"
#include "../max32660/debug.h"

#include "lmic.h"

#define LED_PORT        PORT_0 // use GPIO PA8 (LED4 on IMST, P11/PPS/EXT1_10/GPS6 on Blipper)
#define LED_PIN         PIN_13
#define UART   			MXC_UART1
#define UART_MAP    	MAP_A
#define UART_FLOW		UART_FLOW_DISABLE
#define UART_BAUD		115200

mxc_uart_regs_t *DebugUART = UART;

const uart_cfg_t debug_uart_cfg = {
    UART_PARITY_DISABLE,
    UART_DATA_SIZE_8_BITS,
    UART_STOP_1,
    UART_FLOW_CTRL_DIS,
    UART_FLOW_POL_DIS,
	UART_BAUD
};

const sys_cfg_uart_t debug_uart_sys_cfg = {
	UART_MAP,
	UART_FLOW,
};

void debug_init () {
    // configure LED pin as output
	LED_Init();
    debug_led(0);

    UART_Init(DebugUART, &debug_uart_cfg, &debug_uart_sys_cfg);

    // print banner
    debug_str("\r\n============== DEBUG STARTED ==============\r\n");
}

void debug_led (u1_t val) {
	if (val == 0)
		LED_Off(0);
	else if (val == 1)
		LED_On(0);
}

void debug_char (u1_t c) {
    UART_WriteByte(DebugUART,c);
}

void debug_hex (u1_t b) {
    debug_char("0123456789ABCDEF"[b>>4]);
    debug_char("0123456789ABCDEF"[b&0xF]);
}

void debug_buf (const u1_t* buf, u2_t len) {
    while(len--) {
        debug_hex(*buf++);
        debug_char(' ');
    }
    debug_char('\r');
    debug_char('\n');
}

void debug_uint (u4_t v) {
    for(s1_t n=24; n>=0; n-=8) {
        debug_hex(v>>n);
    }
}

void debug_str (const u1_t* str) {
    while(*str) {
        debug_char(*str++);
    }
}

void debug_val (const u1_t* label, u4_t val) {
    debug_str(label);
    debug_uint(val);
    debug_char('\r');
    debug_char('\n');
}

void debug_event (int ev) {
    static const u1_t* evnames[] = {
        [EV_SCAN_TIMEOUT]   = "SCAN_TIMEOUT",
        [EV_BEACON_FOUND]   = "BEACON_FOUND",
        [EV_BEACON_MISSED]  = "BEACON_MISSED",
        [EV_BEACON_TRACKED] = "BEACON_TRACKED",
        [EV_JOINING]        = "JOINING",
        [EV_JOINED]         = "JOINED",
        [EV_RFU1]           = "RFU1",
        [EV_JOIN_FAILED]    = "JOIN_FAILED",
        [EV_REJOIN_FAILED]  = "REJOIN_FAILED",
        [EV_TXCOMPLETE]     = "TXCOMPLETE",
        [EV_LOST_TSYNC]     = "LOST_TSYNC",
        [EV_RESET]          = "RESET",
        [EV_RXCOMPLETE]     = "RXCOMPLETE",
        [EV_LINK_DEAD]      = "LINK_DEAD",
        [EV_LINK_ALIVE]     = "LINK_ALIVE",
    };
    debug_str(evnames[ev]);
    debug_char('\r');
    debug_char('\n');
}
