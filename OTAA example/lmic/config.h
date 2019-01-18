/*
 * config.h
 *
 *  Created on: Jan 16, 2019
 *      Author: Ruben
 */

#ifndef LMIC_CONFIG_H_
#define LMIC_CONFIG_H_

#define CFG_eu868 1
//#define CFG_us915 1

// This is the SX1272/SX1273 radio, which is also used on the HopeRF
// RFM92 boards.
//#define CFG_sx1272_radio 1
// This is the SX1276/SX1277/SX1278/SX1279 radio, which is also used on
// the HopeRF RFM95 boards.
#define CFG_sx1276_radio 1

#define OSTICKS_PER_SEC 46875

#endif /* LMIC_CONFIG_H_ */
