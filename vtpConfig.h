#ifndef VTPCONFIG_H
#define VTPCONFIG_H

#include "vtpLib.h"

/****************************************************************************
 *
 *  vtpConfig.h  -  configuration library header file for VTP board 
 *
 */


#define FNLEN     250       /* length of config. file name - careful, sscanf
                               format needs to be updated when this changes */
#define STRLEN    250       /* length of str_tmp */
#define ROCLEN     80       /* length of ROC_name */

typedef struct
{
  int ssp_strigger_bit_mask;
  int ssp_sector_mask;
  int sector_mult_min;
  int sector_coin_width;
  int central_en;
} trgbit;

/** VTP configuration parameters **/
typedef struct {
  char fw_filename[FNLEN];
  
  int v7_configured;
  
  int fw_rev;
  int fw_type;
  
  int window_width;
  int window_offset;

  int payload_en;
  int fiber_en;
  
  struct
  {
    unsigned int fadcsum_ch_en[8];
    struct
    {
      int hit_emin;
      int hit_dt;
      int dalitz_min;
      int dalitz_max;
      int cosmic_emin;
      int cosmic_multmax;
      int cosmic_hitwidth;
      int cosmic_evaldelay;
    } inner;
    struct
    {
      int hit_emin;
      int hit_dt;
      int dalitz_min;
      int dalitz_max;
      int cosmic_emin;
      int cosmic_multmax;
      int cosmic_hitwidth;
      int cosmic_evaldelay;
    } outer;
  } ec;
  
  struct
  {
    unsigned int fadcsum_ch_en[8];
    int cosmic_emin;
    int cosmic_multmax;
    int cosmic_hitwidth;
    int cosmic_evaldelay;
    int cosmic_pixelen;
  } pc;
  
  struct
  {
    int threshold[3];
    int nframes;
    int dipfactor;
    int dalitz_min;
    int dalitz_max;
    int nstrip_min;
    int nstrip_max;
  } pcs;

  struct
  {
    int threshold[3];
    int nframes;
    int dipfactor;
    int dalitz_min;
    int dalitz_max;
    int nstrip_min;
    int nstrip_max;
  } ecs;
  
  struct
  {
    int trig_latency;
    int trig_width;
    trgbit trgbits[16];
  } gt;
  
  struct
  {
    int dcsegfind_threshold[2];
  } dc;
  
  struct
  {
    int hit_dt;
    int cluster_emin;
  } hcal;

  struct
  {
    int seed_emin;
    int seed_dt;
  } ftcal; 
} VTP_CONF;

/* functions */
void vtpInitGlobals();
int vtpReadConfigFile(char *filename);
int vtpDownloadAll();
int vtpUploadAll(char *string, int length);
int vtpConfig(char *fname);
void vtpMon();

#endif
