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
vtpNPSSetEcalCluster(uint16_t seed_thr, uint16_t hit_dt, uint16_t cluster_thr)
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

  VLOCK;
  vtp->v7.npsEcalCluster.ctrl = (seed_thr << 16) | (hit_dt << 13) | (cluster_thr);
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
vtpNPSGetEcalCluster(uint16_t *seed_thr, uint16_t *hit_dt, uint16_t *cluster_thr)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_NPS,0);

  uint32_t regval = 0;

  VLOCK;
  regval = vtp->v7.npsEcalCluster.ctrl;
  VUNLOCK;

  *seed_thr = (regval & NPS_ECALCLUSTER_CTRL_SEED_THR_MASK) >> 16;
  *hit_dt = (regval & NPS_ECALCLUSTER_CTRL_HIT_DT_MASK) >> 13;
  *cluster_thr = regval & NPS_ECALCLUSTER_CTRL_CLUSTER_THR_MASK;

  return OK;
}

/**
 * @brief Set the NPS Crate ID
 * @param[in] crate_id ID specifying Y coordinate
 * @return OK if successfull, otherwise ERROR
 */
int32_t
vtpNPSSetCrateID(uint8_t crate_id)
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
vtpNPSGetCrateID(uint8_t *crate_id)
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
