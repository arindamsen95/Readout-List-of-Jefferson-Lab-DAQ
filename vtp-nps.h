#pragma once
/**
 * @copyright Copyright 2022, Jefferson Science Associates, LLC.
 *            Subject to the terms in the LICENSE file found in the
 *            top-level directory.
 *
 * @author    Bryan Moffit
 *            moffit@jlab.org                   Jefferson Lab, MS-12B3
 *            Phone: (757) 269-5660             12000 Jefferson Ave.
 *            Fax:   (757) 269-5800             Newport News, VA 23606
 *
 * @file      vtp-nps.h
 * @brief     Header of VTP Library for NPS register status and control
 *
 */
#include <stdint.h>

#define NPS_SEED_THR_MAX      0x3FFF
#define NPS_HIT_DT_MAX        0x7
#define NPS_CLUSTER_THR_MAX   0x1FFFF

#define NPS_CRATE_ID_MAX      0x7

int32_t vtpNPSSetEcalCluster(uint16_t seed_thr, uint16_t hit_dt, uint16_t cluster_thr);
int32_t vtpNPSGetEcalCluster(uint16_t *seed_thr, uint16_t *hit_dt, uint16_t *cluster_thr);
int32_t vtpNPSSetCrateID(uint8_t crate_id);
int32_t vtpNPSGetCrateID(uint8_t *crate_id);
