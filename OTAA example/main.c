/***** Includes *****/
#include "lmic.h"
#include "debug.h"
#include <stdio.h>
#include <stdint.h>
/***** Includes *****/

#include "lmic.h"

/***** Definitions *****/

/***** Globals *****/

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t APPEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t DEVEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getDevEui (u1_t* buf) { memcpy(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t APPKEY[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
void os_getDevKey (u1_t* buf) {  memcpy(buf, APPKEY, 16);}

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

uint8_t mydata[12];
uint8_t count = 1;

static osjob_t sendjob;

/***** Functions *****/
void setup();
void loop();
void onEvent(ev_t ev);
void do_send(osjob_t* j);

int main(void) {
	setup();
	while (1) {
		loop();
	}
}

void setup() {

	// LMIC init
	os_init();
	// Reset the MAC state. Session and pending data transfers will be discarded.
	LMIC_reset();

	// Start job
	do_send(&sendjob);
}

void loop() {
	os_runloop();
}

void onEvent(ev_t ev) {
	debug_val("OS Time: ", os_getTime());
	debug_str(": ");
	switch (ev) {
	case EV_SCAN_TIMEOUT:
		debug_str("EV_SCAN_TIMEOUT \n");
		break;
	case EV_BEACON_FOUND:
		debug_str("EV_BEACON_FOUND \n");
		break;
	case EV_BEACON_MISSED:
		debug_str("EV_BEACON_MISSED \n");
		break;
	case EV_BEACON_TRACKED:
		debug_str("EV_BEACON_TRACKED \n");
		break;
	case EV_JOINING:
		debug_str("EV_JOINING \n");
		break;
	case EV_JOINED:
		debug_str("EV_JOINED \n");
		break;
	case EV_RFU1:
		debug_str("EV_RFU1 \n");
		break;
	case EV_JOIN_FAILED:
		debug_str("EV_JOIN_FAILED \n");
		break;
	case EV_REJOIN_FAILED:
		debug_str("EV_REJOIN_FAILED \n");
		break;
	case EV_TXCOMPLETE:
		debug_str("EV_TXCOMPLETE (includes waiting for RX windows) \n");
		if (LMIC.txrxFlags & TXRX_ACK)
			debug_str("Received ack \n");
		if (LMIC.dataLen) {
			debug_val("Received ", (int) LMIC.dataLen);
			debug_str(" bytes of payload \n");
		}
		// Schedule next transmission
		os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL),
				do_send);
		break;
	case EV_LOST_TSYNC:
		debug_str("EV_LOST_TSYNC \n");
		break;
	case EV_RESET:
		debug_str("EV_RESET \n");
		break;
	case EV_RXCOMPLETE:
		// data received in ping slot
		debug_str("EV_RXCOMPLETE \n");
		break;
	case EV_LINK_DEAD:
		debug_str("EV_LINK_DEAD \n");
		break;
	case EV_LINK_ALIVE:
		debug_str("EV_LINK_ALIVE \n");
		break;
	default:
		printf("Unknown event \n");
		break;
	}
}

void do_send(osjob_t* j) {
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & OP_TXRXPEND) {
		debug_str("OP_TXRXPEND, not sending \n");
	} else {
		// Prepare upstream data transmission at the next possible time.
		sprintf((char *)mydata, "Package %03d", count);
		LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 1);
		count++;
		debug_str("Packet queued \n");
	}
	// Next TX is scheduled after TX_COMPLETE event.
}




