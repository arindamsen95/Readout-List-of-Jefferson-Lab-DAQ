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
 * @file      vtp-nps.c
 * @brief     VTP Library for NPS register status and control
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "vtpLib.h"
#include "vtp-nps.h"


extern volatile ZYNC_REGS *vtp;
extern int VTP_FW_Version;
extern int VTP_FW_Type[2];

/* Mutex to guard VTP read/writes */
extern pthread_mutex_t   vtpMutex;
#define VLOCK     if(pthread_mutex_lock(&vtpMutex)<0) perror("pthread_mutex_lock");
#define VUNLOCK   if(pthread_mutex_unlock(&vtpMutex)<0) perror("pthread_mutex_unlock");

#define CHECKINIT {						\
    if(vtp == NULL) {						\
      printf("%s: ERROR: VTP not initialized\n",__func__);	\
      return ERROR;						\
    }								\
  }

#define CHECKTYPE(v,c) {						\
    if ((c!=0)&&(c!=1)) {						\
      printf("%s: ERROR: VTP wrong Chip ID (%d)\n",__func__,c);		\
      return ERROR;							\
    }									\
    if( (v != VTP_FW_TYPE_COMMON) && (v != VTP_FW_Type[c]) ) {		\
      printf("%s: ERROR: VTP wrong firmware type (%d)\n",__func__,v);	\
      return ERROR;							\
    }									\
  }

/**
 * @brief Set NPS ECal Cluster parameters
 * @param[in] seed_thr The thr of seed
 * @param[in] hit_dt Description
 * @param[in] cluster_thr The thr of cluster
 * @return OK if successful, otherwise ERROR:
 */
int32_t
vtpNPSSetEcalCluster(uint32_t seed_thr, uint32_t hit_dt, uint32_t cluster_thr, uint32_t readout_thr, uint32_t nhit_min)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  if(seed_thr > NPS_SEED_THR_MAX)
    {
      printf("%s: ERROR: Invalid seed_thr 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     seed_thr, seed_thr, NPS_SEED_THR_MAX, NPS_SEED_THR_MAX);
      return ERROR;
    }

  if(hit_dt > NPS_HIT_DT_MAX)
    {
      printf("%s: ERROR: Invalid hit_dt 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     hit_dt, hit_dt, NPS_HIT_DT_MAX, NPS_HIT_DT_MAX);
      return ERROR;
    }

  if(cluster_thr > NPS_CLUSTER_THR_MAX)
    {
      printf("%s: ERROR: Invalid cluster_thr 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     cluster_thr, cluster_thr, NPS_CLUSTER_THR_MAX, NPS_CLUSTER_THR_MAX);
      return ERROR;
    }

  if(readout_thr > NPS_CLUSTER_THR_MAX)
    {
      printf("%s: ERROR: Invalid readout_thr 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     readout_thr, readout_thr, NPS_CLUSTER_THR_MAX, NPS_CLUSTER_THR_MAX);
      return ERROR;
    }

  if(nhit_min > NPS_NHIT_MIN_MAX)
    {
      printf("%s: ERROR: Invalid hit_dt 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     nhit_min, nhit_min, NPS_NHIT_MIN_MAX, NPS_NHIT_MIN_MAX);
      return ERROR;
    }


  VLOCK;
  vtp->v7.npsEcalCluster.ctrl = (seed_thr) | (hit_dt << 16) | (nhit_min << 24);
  vtp->v7.npsEcalCluster.threshold = (cluster_thr) | (readout_thr << 16);
  VUNLOCK;

  return OK;
}

/**
 * @brief Return the NPS ECal Cluster parameters
 * @param[inout] seed_thr The thr of seed
 * @param[out] hit_dt Description
 * @param[out] cluster_thr The thr of cluster
 * @return OK if successful, otherwise ERROR
 */
int32_t
vtpNPSGetEcalCluster(uint32_t *seed_thr, uint32_t *hit_dt, uint32_t *cluster_thr, uint32_t *readout_thr, uint32_t *nhit_min)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  uint32_t ctrl = 0, threshold = 0;

  VLOCK;
  ctrl = vtp->v7.npsEcalCluster.ctrl;
  threshold = vtp->v7.npsEcalCluster.threshold;
  VUNLOCK;

  *seed_thr = ctrl & NPS_ECALCLUSTER_CTRL_SEED_THR_MASK;
  *hit_dt = (ctrl & NPS_ECALCLUSTER_CTRL_HIT_DT_MASK) >> 16;
  *nhit_min = (ctrl & NPS_ECALCLUSTER_CTRL_HIT_MIN_MASK) >> 24;

  *cluster_thr = threshold & NPS_ECALCLUSTER_THRESHOLD_TRIGGER_MASK;
  *readout_thr = (threshold & NPS_ECALCLUSTER_THRESHOLD_READOUT_MASK) >> 16;

  return OK;
}

/**
 * @brief Set the NPS Crate ID
 * @param[in] crate_id ID specifying Y coordinate: nps-vtp1=1,nps-vtp2=2...
 * @return OK if successfull, otherwise ERROR
 */
int32_t
vtpNPSSetCrateID(uint32_t crate_id)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  if(crate_id > NPS_CRATE_ID_MAX)
    {
      printf("%s: ERROR: Invalid crate_id 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     crate_id, crate_id, NPS_CRATE_ID_MAX, NPS_CRATE_ID_MAX);
      return ERROR;
    }

  VLOCK;
  vtp->v7.npsEcalCluster.crate_id = crate_id;
  VUNLOCK;

  return OK;
}

/**
 * @brief Return the NPS Crate ID
 * @param[inout] crate_id ID specifying Y coordinate
 * @return OK if successful, otherwise ERROR
 */
int32_t
vtpNPSGetCrateID(uint32_t *crate_id)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  uint32_t regval = 0;

  VLOCK;
  regval = vtp->v7.npsEcalCluster.crate_id;
  VUNLOCK;

  *crate_id = regval & NPS_ECALCLUSTER_CRATE_ID_MASK;

  return OK;
}

/**
 * @brief Set NPS Cosmic parameters
 * @param[in] scint_dt scintillator hit timing coincidence
 * @param[in] column_veto_en enables trigger VETO logic (only hits in single column of crate accepted)
 * @param[in] column_dt crystal column hit timing coincidence
 * @param[in] column_multmin crystal column min multiplicity
 * @return OK if successful, otherwise ERROR:
 */
int32_t
vtpNPSSetCosmic(uint32_t scint_dt, uint32_t column_dt, uint32_t column_multmin, uint32_t column_veto_en)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  if(scint_dt > NPS_SCINT_DT_MAX)
    {
      printf("%s: ERROR: Invalid scint_dt 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     scint_dt, scint_dt, NPS_SCINT_DT_MAX, NPS_SCINT_DT_MAX);
      return ERROR;
    }

  if(column_dt > NPS_COLUMN_DT_MAX)
    {
      printf("%s: ERROR: Invalid column_dt 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     column_dt, column_dt, NPS_COLUMN_DT_MAX, NPS_COLUMN_DT_MAX);
      return ERROR;
    }

  if(column_multmin > NPS_COLUMN_MULTMIN_MAX)
    {
      printf("%s: ERROR: Invalid column_multmin 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     column_multmin, column_multmin, NPS_COLUMN_MULTMIN_MAX, NPS_COLUMN_MULTMIN_MAX);
      return ERROR;
    }

  if(column_veto_en)
    column_veto_en = 1;

  VLOCK;
  // note: 0x30000, bits 16,17 enable the top and bottom scintillators for coincidence (should always be enabled)
  vtp->v7.npsCosmic.ctrl = (scint_dt << 8) | (column_dt << 4) | column_multmin | 0x30000 | (column_veto_en<<7);
  VUNLOCK;

  return OK;
}

/**
 * @brief Return the NPS Cosmic parameters
 * @param[out] scint_dt scintillator hit timing coincidence
 * @param[out] column_veto_en enables trigger VETO logic (only hits in single column of crate accepted)
 * @param[out] column_dt crystal column hit timing coincidence
 * @param[out] column_multmin crystal column min multiplicity
 * @return OK if successful, otherwise ERROR
 */
int32_t
vtpNPSGetCosmic(uint32_t *scint_dt, uint32_t *column_dt, uint32_t *column_multmin, uint32_t *column_veto_en)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  uint32_t regval = 0;

  VLOCK;
  regval = vtp->v7.npsCosmic.ctrl;
  VUNLOCK;


  *scint_dt = (regval & NPS_COSMIC_CTRL_SCINT_DT_MASK) >> 8;
  *column_veto_en = (regval & NPS_COSMIC_CTRL_COLUMN_VETOEN_MASK) >> 7;
  *column_dt = (regval & NPS_COSMIC_CTRL_COLUMN_DT_MASK) >> 4;
  *column_multmin = regval & NPS_COSMIC_CTRL_COLUMN_MULTMIN_MASK;

  return OK;
}


/**
 * @brief Set NPS FADC readout mask parameters
 * @param[in] offset offset in VTP readout window to look for clusters to determine FADC readout mask
 * @param[in] width coincidence width clusters are extended in time in VTP window to look for FADC readout mask
 * @return OK if successful, otherwise ERROR:
 */
int32_t
vtpNPSSetFadcMask(uint32_t offset, uint32_t width)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  if(offset > NPS_FADCMASK_OFFSET_MAX)
    {
      printf("%s: ERROR: Invalid offset 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     offset, offset, NPS_FADCMASK_OFFSET_MAX, NPS_FADCMASK_OFFSET_MAX);
      return ERROR;
    }

  if(width > NPS_FADCMASK_WIDTH_MAX)
    {
      printf("%s: ERROR: Invalid column_dt 0x%x (%d).  Max = 0x%x (%d)\n",
	     __func__,
	     width, width, NPS_FADCMASK_WIDTH_MAX, NPS_FADCMASK_WIDTH_MAX);
      return ERROR;
    }

  VLOCK;
  vtp->v7.npsFadcMask.ctrl = (offset << 16) | width;
  VUNLOCK;

  return OK;
}

/**
 * @brief Return NPS FADC readout mask parameters
 * @param[out] offset offset in VTP readout window to look for clusters to determine FADC readout mask
 * @param[out] width coincidence width clusters are extended in time in VTP window to look for FADC readout mask
 * @return OK if successful, otherwise ERROR
 */
int32_t
vtpNPSGetFadcMask(uint32_t *offset, uint32_t *width)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  uint32_t regval = 0;

  VLOCK;
  regval = vtp->v7.npsFadcMask.ctrl;
  VUNLOCK;


  *offset = regval & NPS_FADCMASK_CTRL_MASKOFFSET_MASK;
  *width  = (regval & NPS_FADCMASK_CTRL_MASKWIDTH_MASK) >> 16;

  return OK;
}

