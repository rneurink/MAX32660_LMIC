/***** Includes *****/
#include "lmic.h"
#include "debug.h"
#include <stdio.h>
#include <stdint.h>
/***** Includes *****/

#include "lmic.h"

/***** Definitions *****/

/***** Globals *****/

// LoRaWAN NwkSKey, network session key
// This is the default Semtech key, which is used by the early prototype TTN
// network.
static const u1_t NWKSKEY[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };

// LoRaWAN AppSKey, application session key
// This is the default Semtech key, which is used by the early prototype TTN
// network.
static const u1_t APPSKEY[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };

// LoRaWAN end-device address (DevAddr)
static const u4_t DEVADDR = 0x03FF0001; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui(u1_t* buf) {
}
void os_getDevEui(u1_t* buf) {
}
void os_getDevKey(u1_t* buf) {
}

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

	// If not running an AVR with PROGMEM, just use the arrays directly
	LMIC_setSession(0x1, DEVADDR, NWKSKEY, APPSKEY);

	LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI); // g-band
	LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
	LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI); // g2-band
	// TTN defines an additional channel at 869.525Mhz using SF9 for class B
	// devices' ping slots. LMIC does not have an easy way to define set this
	// frequency and support for class B is spotty and untested, so this
	// frequency is not configured here.

	// Disable link check validation
	LMIC_setLinkCheckMode(0);

	// TTN uses SF9 for its RX2 window.
	LMIC.dn2Dr = DR_SF9;

	// Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
	LMIC_setDrTxpow(DR_SF7, 14);

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
		LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
		count++;
		debug_str("Packet queued \n");
	}
	// Next TX is scheduled after TX_COMPLETE event.
}
