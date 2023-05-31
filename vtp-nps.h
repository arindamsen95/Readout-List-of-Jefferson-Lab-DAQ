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

#define NPS_SEED_THR_MAX      NPS_ECALCLUSTER_CTRL_SEED_THR_MASK
#define NPS_HIT_DT_MAX        (NPS_ECALCLUSTER_CTRL_HIT_DT_MASK >> 16)
#define NPS_NHIT_MIN_MAX      (NPS_ECALCLUSTER_CTRL_HIT_MIN_MASK >> 24)

#define NPS_CLUSTER_THR_MAX   NPS_ECALCLUSTER_THRESHOLD_TRIGGER_MASK

#define NPS_CRATE_ID_MAX      NPS_ECALCLUSTER_CRATE_ID_MASK

#define NPS_SCINT_DT_MAX	(NPS_COSMIC_CTRL_SCINT_DT_MASK>>8)
#define NPS_COLUMN_DT_MAX	(NPS_COSMIC_CTRL_COLUMN_DT_MASK>>4)
#define NPS_COLUMN_MULTMIN_MAX	NPS_COSMIC_CTRL_COLUMN_MULTMIN_MASK

#define NPS_FADCMASK_OFFSET_MAX	NPS_FADCMASK_CTRL_MASKOFFSET_MASK
#define NPS_FADCMASK_WIDTH_MAX	(NPS_FADCMASK_CTRL_MASKWIDTH_MASK>>16)

#define NPS_N_TRIGGER_BITS    32

int32_t vtpNPSSetEcalCluster(uint32_t seed_thr, uint32_t hit_dt, uint32_t cluster_thr, uint32_t readout_thr, uint32_t nhit_min);
int32_t vtpNPSGetEcalCluster(uint32_t *seed_thr, uint32_t *hit_dt, uint32_t *cluster_thr, uint32_t *readout_thr, uint32_t *nhit_min);
int32_t vtpNPSSetCrateID(uint32_t crate_id);
int32_t vtpNPSGetCrateID(uint32_t *crate_id);
int32_t vtpNPSSetCosmic(uint32_t scint_dt, uint32_t column_dt, uint32_t column_multmin, uint32_t column_veto_en);
int32_t vtpNPSGetCosmic(uint32_t *scint_dt, uint32_t *column_dt, uint32_t *column_multmin, uint32_t *column_veto_en);
int32_t vtpNPSSetFadcMask(uint32_t offset, uint32_t width);
int32_t vtpNPSGetFadcMask(uint32_t *offset, uint32_t *width);
