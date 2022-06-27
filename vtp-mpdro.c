/*----------------------------------------------------------------------------*
 *  Copyright (c) 2021        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     vtp-mpdro : VTP library for MPD readout
 *
 *----------------------------------------------------------------------------*/

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "vtpLib.h"


#ifndef FNLEN
#define FNLEN     250       /* length of config. file name - careful, sscanf
                               format needs to be updated when this changes */
#endif

extern volatile ZYNC_REGS *vtp;
extern int VTP_FW_Version;
extern int VTP_FW_Type[2];

char vtpMpdCommonModeFilename[FNLEN];
char vtpMpdPedestalFilename[FNLEN];
float vtpMpdPedestalFactor = 1.;

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

#define CHECKTYPE(v,c) {					    \
    if ((c!=0)&&(c!=1)) {					    \
       printf("%s: ERROR: VTP wrong Chip ID (%d)\n",__func__,c);    \
       return ERROR;                                                \
       }                                                            \
    if( (v != VTP_FW_TYPE_COMMON) && (v != VTP_FW_Type[c]) ) {         \
      printf("%s: ERROR: VTP wrong firmware type (%d)\n",__func__,v);  \
      return ERROR;						       \
    }								       \
  }

/* MPD Control/Status Routines */
#ifdef MPD_MON_NOT_SUPPORTED
int
vtpMpdMonEnable(int fiber)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  if((fiber<0) || (fiber>=VTP_MPD_MAX))
  {
    printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
    return ERROR;
  }

  VLOCK;
  vtp->v7.mpdFiber[fiber].mon_ctrl = 0x1; // reset
  vtp->v7.mpdFiber[fiber].mon_ctrl = 0x2; // enable write
  VUNLOCK;
  return OK;
}

int
vtpMpdMonDump(int fiber)
{
  int i;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  if((fiber<0) || (fiber>=VTP_MPD_MAX))
  {
    printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
    return ERROR;
  }

  printf("%s(%d)\n", __func__, fiber);
  VLOCK;
  vtp->v7.mpdFiber[fiber].mon_ctrl = 0; // disable write

  for(i=0;i<65536;i++)
  {
    int word, rx_d;
    int isk, iscomma, disperr, notintable, clkcorcnt, bufstatus, realign;
    word = vtp->v7.mpdFiber[fiber].mon_status;

    rx_d         = (word>>0)  & 0xFFFF;
    isk          = (word>>16) & 0x0003;
    iscomma      = (word>>18) & 0x0003;
    disperr      = (word>>20) & 0x0003;
    notintable   = (word>>22) & 0x0003;
    clkcorcnt    = (word>>24) & 0x0007;
    bufstatus    = (word>>27) & 0x0007;
    realign      = (word>>31) & 0x0001;

    printf(    "0x%08X: RE_AL %d, BF_ST %d, CC %d, NIT %d, DSP %d, ISC %d, ISK %d, D %04X\n", word, realign, bufstatus, clkcorcnt, notintable, disperr, iscomma, isk, rx_d);

  }

  VUNLOCK;
  return OK;
}
#endif // MPD_MON_NOT_SUPPORTED

int
vtpMpdSetAvg(int fiber, int apv, int min, int max)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  if((fiber<0) || (fiber>=VTP_MPD_MAX))
    {
      printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
      return ERROR;
    }
  if((apv<0) || (apv>15))
    {
      printf("%s: ERROR: invalid apv %d\n",__func__,apv);
      return ERROR;
    }
  if(min<-4095)
    {
      printf("%s: Warning: min(%d)<0, setting min to -4095\n",__func__,min);
      min = -4095;
    }
  if(min>4095)
    {
      printf("%s: Warning: min(%d)>4095, setting min to 4095\n",__func__,min);
      min = 4095;
    }
  if(max<-4095)
    {
      printf("%s: Warning: max(%d)<0, setting min to -4095\n",__func__,max);
      max = -4095;
    }
  if(max>4095)
    {
      printf("%s: Warning: max(%d)>4095, setting max to 4095\n",__func__,max);
      max = 4095;
    }

  VLOCK;
  //    [min threshold]       [apvid]     [0=min]   [use APV15 offset space]
  val = ((min & 0x1fff)<<0) | (apv<<17) | (0<<16) | (15<<23);
  vtp->v7.mpdFiber[fiber].apv_offset = val | 0x80000000;

  //    [max threshold]       [apvid]     [1=max]   [use APV15 offset space]
  val = ((max & 0x1fff)<<0) | (apv<<17) | (1<<16) | (15<<23);
  vtp->v7.mpdFiber[fiber].apv_offset = val | 0x80000000;

  VUNLOCK;

  return OK;
}

int
vtpMpdSetApvOffset(int fiber, int apv, int strip, int offset)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  if((fiber<0) || (fiber>=VTP_MPD_MAX))
    {
      printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
      return ERROR;
    }
  if((apv<0) || (apv>15))
    {
      printf("%s: ERROR: invalid apv %d\n",__func__,apv);
      return ERROR;
    }

  if((strip<0) || (strip>127))
    {
      printf("%s: ERROR: invalid strip %d\n",__func__,strip);
      return ERROR;
    }
  if(offset<-4095)
    {
      printf("%s: Warning: offset(%d)<0, setting offset to -4095\n",__func__,offset);
      offset = -4095;
    }
  if(offset>4095)
    {
      printf("%s: Warning: offset(%d)>4095, setting offset to 4095\n",__func__,offset);
      offset = 4095;
    }

  val = (offset & 0x1fff) | (strip<<16) | (apv<<23);

  VLOCK;
  vtp->v7.mpdFiber[fiber].apv_offset = val | 0x80000000;
  VUNLOCK;
  return OK;
}

int
vtpMpdSetApvThreshold(int fiber, int apv, int strip, int threshold)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);
  if((fiber<0) || (fiber>=VTP_MPD_MAX))
    {
      printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
      return ERROR;
    }
  if((apv<0) || (apv>15))
    {
      printf("%s: ERROR: invalid apv %d\n",__func__,apv);
      return ERROR;
    }
  if((strip<0) || (strip>127))
    {
      printf("%s: ERROR: invalid strip %d\n",__func__,strip);
      return ERROR;
    }
  if(threshold>511)
    {
      printf("%s: Warning: threshold(%d)>511, setting threshold to 511\n",__func__,threshold);
      threshold = 511;
    }
  if(threshold<0)
    {
      printf("%s: Warning: threshold(%d)<0, setting threshold to 0\n",__func__,threshold);
      threshold = 0;
    }

  val = (threshold & 0x1fff) | (strip<<16) | (apv<<23);

  VLOCK;
  vtp->v7.mpdFiber[fiber].apv_thr = val | 0x80000000;
  VUNLOCK;
  return OK;
}


int
vtpMpdFiberReset()
{
  int impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      vtp->v7.mpdFiber[impd].gtx_ctrl = MPD_GTX_CTRL_FIBER_GT_RESET;
    }
  usleep(60000);
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      vtp->v7.mpdFiber[impd].gtx_ctrl = 0;
    }
  VUNLOCK;
  return OK;
}

int
vtpMpdFiberLinkReset(uint64_t mpdmask)
{
  uint64_t impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if(mpdmask & (1ull<<impd))
	vtp->v7.mpdFiber[impd].gtx_ctrl = MPD_GTX_CTRL_FIBER_RESET;
    }
  usleep(60000);
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if(mpdmask & (1ull<<impd))
	vtp->v7.mpdFiber[impd].gtx_ctrl = 0;
    }
  VUNLOCK;
  return OK;
}

int
vtpMpdEbSetFlags(int build_all_samples, int build_debug_headers,
		 int enable_cm, int noprocessing_prescale,
                 int allow_peak_any_time, int min_avg_samples)
{
  int impd=0, val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  printf("%s(build_all_samples=%d,build_debug_headers=%d,enable_cm=%d,noprocessing_prescale=%d,allow_peak_any_time=%d,min_avg_samples=%d\n",
    __func__, build_all_samples, build_debug_headers, enable_cm, noprocessing_prescale, allow_peak_any_time, min_avg_samples);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      val = vtp->v7.mpdFiber[impd].eb_ctrl & 0xFFFFFFF1;

      if(build_all_samples)
	val |= MPD_EBCTRL_BUILD_ALL_SAMPLES;

      if(build_debug_headers)
	val |= MPD_EBCTRL_BUILD_DEBUG_HEADERS;

      if(enable_cm)
	val |= MPD_EBCTRL_ENABLE_CM;

      if(allow_peak_any_time)
        val |= MPD_EBCTRL_ALLOW_PEAK_ANY_TIME;

      val |= (noprocessing_prescale & 0xFFFF)<<16;

      val |= (min_avg_samples & 0xFF)<<8;

      vtp->v7.mpdFiber[impd].eb_ctrl = val;
    }
  VUNLOCK;

  return OK;
}

int
vtpMpdEbGetFlags(int *build_all_samples, int *build_debug_headers,
		 int *enable_cm, int *noprocessing_prescale,
                 int *allow_peak_any_time, int *min_avg_samples)
{
  int impd=0;
  unsigned int val = 0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  impd = 0;
  val = vtp->v7.mpdFiber[impd].eb_ctrl;
  VUNLOCK;


  *build_all_samples = (val & MPD_EBCTRL_BUILD_ALL_SAMPLES) ? 1 : 0;

  *build_debug_headers = (val & MPD_EBCTRL_BUILD_DEBUG_HEADERS) ? 1 : 0;

  *enable_cm = (val & MPD_EBCTRL_ENABLE_CM) ? 1 : 0;

  *allow_peak_any_time = (val & MPD_EBCTRL_ALLOW_PEAK_ANY_TIME) ? 1 : 0;

  *noprocessing_prescale = (val & MPD_EBCTRL_NOPROCESSING_PRESCALE_MASK) >> 16;

  *min_avg_samples = (val & MPD_EBCTRL_AVGNSTRIPS_MIN_MASK) >> 8;

  return OK;
}
int
vtpMpdEnable(uint64_t mpdmask)
{
  uint64_t impd=0;
  unsigned int rval = 0;
  int printFlag = 0;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if(mpdmask & (1ull<<impd))
	{
	  vtp->v7.mpdFiber[impd].eb_ctrl = MPD_EBCTRL_ENABLE;

#ifdef EB_BUSY_THR_SUPPORTED
	  vtp->v7.mpdFiber[impd].eb_busy_thr = 0x100;
	  rval = vtp->v7.mpdFiber[impd].eb_busy_thr;

	  if(printFlag)
	    printf("BusyTh set!!!0x%08X\n", rval);
#endif
	}
    }
  VUNLOCK;

  return OK;
}

int
vtpMpdDisable(uint64_t mpdmask)
{
  uint64_t impd=0;
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if(mpdmask & (1ull<<impd))
	{
	  val = vtp->v7.mpdFiber[impd].eb_ctrl & ~MPD_EBCTRL_ENABLE;
	  vtp->v7.mpdFiber[impd].eb_ctrl = val;
	}
    }
  VUNLOCK;

  return OK;
}

int
vtpMpdReadRegs(int impd)
{
  int ireg=0, res=0, errval=OK;;
  unsigned int rval=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  printf("%s(impd = %d): \n",
	 __func__, impd);
  VLOCK;

  for(ireg = (0x0>>2); ireg < (0x200>>2); ireg++)
    {
      vtp->v7.mpd.addr = (impd << 24) | (ireg << 2);
      rval = vtp->v7.mpd.data;

      if((ireg%4)==0)
	printf("\n%05x\t",ireg<<2);

      printf("  0x%08x  ",rval);
    }
  printf("\n\n");

  VUNLOCK;

  return errval;
}

unsigned int
vtpMpdReadReg(int impd, unsigned int reg)
{
  unsigned int rval=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  vtp->v7.mpd.addr = (impd << 24) | reg;
  rval = vtp->v7.mpd.data;
  VUNLOCK;

  return rval;
}

int
vtpMpdWriteReg(int impd, unsigned int reg, unsigned int value)
{
  unsigned int rval=OK;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  vtp->v7.mpd.addr = (impd << 24) | reg;
  vtp->v7.mpd.data = value;
  VUNLOCK;

  return rval;
}

int
vtpMpdGetSoftErrorCount(int fiber)
{
  int result;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  result = (vtp->v7.mpdFiber[fiber].gtx_status >> 8) & 0xFF;
  VUNLOCK;

  return result;
}

int
vtpMpdPrintStatus(uint64_t pmask, int upOnly)
{
  MPDFIBER_REGS mr[VTP_MPD_MAX];
  uint64_t upMask = 0;
  uint64_t impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  if(pmask == 0)
    pmask = (1ull<<VTP_MPD_MAX)-1;

  if(!upOnly)
    upMask = (1ull<<VTP_MPD_MAX)-1;

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      mr[impd].gtx_ctrl   = vtp->v7.mpdFiber[impd].gtx_ctrl;
      mr[impd].gtx_status = vtp->v7.mpdFiber[impd].gtx_status;
      mr[impd].eb_ctrl = vtp->v7.mpdFiber[impd].eb_ctrl;
      mr[impd].max_rx_len = vtp->v7.mpdFiber[impd].max_rx_len;

      if(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_CHANNEL_UP)
	upMask |= 1ull<<impd;
    }
  VUNLOCK;

  printf("\n");
  printf("                           MPD Settings and Status\n\n");
  printf("                  Channel  TX   ResetDone   -------ERRORS------     Event\n");
  printf("MPD  Ctrl Status    Up    Lock   TX  RX     HARD   FRAME    CNT     Builder\n");
  printf("--------------------------------------------------------------------------------\n");
  /*      31   0000   0038   DOWN     1     1   1      ---     ---    ---     ENABLED  */

  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if (( ((1ull << impd) & pmask) == 0)  ||
	  ( ((1ull << impd) & upMask) == 0))
	continue;
      printf("%2d   ",(int)impd);

      printf("%04x   ",mr[impd].gtx_ctrl);

      printf("%04x   ",mr[impd].gtx_status & 0xffff);

      printf("%s     ",(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_CHANNEL_UP)?" UP ":"DOWN");

      printf("%s     ",(mr[impd].gtx_status & MPD_GTX_STATUS_TX_LOCK)?"1":"0");

      printf("%s   ",(mr[impd].gtx_status & MPD_GTX_STATUS_TX_RESETDONE)?"1":"0");

      printf("%s      ",(mr[impd].gtx_status & MPD_GTX_STATUS_RX_RESETDONE)?"1":"0");

      printf("%s     ",(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_HARD_ERR)?"ERR":"---");

      printf("%s    ",(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_FRAME_ERR)?"ERR":"---");

      if(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_ERR_CNT)
	{
	  printf("%3d     ",(mr[impd].gtx_status & MPD_GTX_STATUS_FIBER_FRAME_ERR)>>8);
	}
      else
	printf("---     ");

      printf("%s",(mr[impd].eb_ctrl & MPD_EBCTRL_ENABLE)?"ENABLED ":"DISABLED");

      printf("\n");
    }
  printf("\n");


  printf("                     Decoder  Avgb     EvWriter Words         Input\n");
  printf("MPD  MAX_RX_LEN      State    State    State    Received      Busy\n");
  printf("--------------------------------------------------------------------------------\n");
  /*      31   0x12345678      0x7      0x7      0x7      0x123456      1 */

  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if (( ((1ull << impd) & pmask) == 0)  ||
	  ( ((1ull << impd) & upMask) == 0))
	continue;
      printf("%2d   ", (int)impd);

      printf("0x%08x      ", mr[impd].max_rx_len);

      printf("0x%x      ", (mr[impd].max_rx_len & MPD_MRL_DECODER_STATE_MASK) >> 29);

      printf("0x%x      ", (mr[impd].max_rx_len & MPD_MRL_AVGB_STATE_MASK) >> 26);

      printf("0x%x      ", (mr[impd].max_rx_len & MPD_MRL_EVENT_WRITER_STATE_MASK) >> 23);

      printf("0x%06x      ", (mr[impd].max_rx_len & MPD_MRL_WORDS_RECV_MASK) >> 1);

      printf("%d        ", (mr[impd].max_rx_len & MPD_MRL_VTP_INPUT_BUFFER_BUSY) ? 1 : 0);

      printf("\n");
    }

  printf("          \n");
  printf("           BuildAll  BuildDbg    Enable    NoProc AllowPeak    MinAvg\n");
  printf("MPD         Samples   Headers        CM  Prescale   Anytime   Samples\n");
  printf("--------------------------------------------------------------------------------\n");

  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      if (( ((1ull << impd) & pmask) == 0)  ||
	  ( ((1ull << impd) & upMask) == 0))
	continue;
      printf(" %-8d ",(int)impd);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_BUILD_ALL_SAMPLES) ? 1 : 0);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_BUILD_DEBUG_HEADERS) ? 1 : 0);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_ENABLE_CM) ? 1 : 0);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_NOPROCESSING_PRESCALE_MASK)>>16);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_ALLOW_PEAK_ANY_TIME) ? 1 : 0);

      printf("%9d ", (mr[impd].eb_ctrl & MPD_EBCTRL_AVGNSTRIPS_MIN_MASK)>>8);


      printf("\n");
    }

  printf("\n");
  printf("  Pedestal Factor: %f\n",
	 vtpMpdPedestalFactor);

  printf("  Common Mode Filename: %s\n",
	 vtpMpdCommonModeFilename);

  printf("  Pedestal Filename: %s\n",
	 vtpMpdPedestalFilename);

  printf("--------------------------------------------------------------------------------\n");
  printf("\n");
  printf("\n");

  return OK;
}

uint64_t
vtpMpdGetChanUpMask()
{
  unsigned int status = 0, rval = 0;
  uint64_t impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  for(impd=0; impd<VTP_MPD_MAX; impd++)
    {
      status = vtp->v7.mpdFiber[impd].gtx_status;
      status &= MPD_GTX_STATUS_FIBER_CHANNEL_UP;
      if(status)
	rval |= (1ull << impd);
    }
  VUNLOCK;

  return rval;
}

int
vtpGetMpdMaxRxLen(int impd)
{
  int result;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  result = vtp->v7.mpdFiber[impd].max_rx_len;
  VUNLOCK;
  return result;
}

#if MPD_EB_NOT_SUPPORTED
int
vtpGetEB_wordCount(int impd)
{
  int result;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);

  VLOCK;
  result = vtp->v7.mpdFiber[impd].eb_word_count;
  VUNLOCK;
  return result;
}


int
vtpGetEbStatus(unsigned int *blockcnt, unsigned int *wordcnt, unsigned int *eventcnt)
{

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO,0);


  VLOCK;
  *blockcnt = vtp->v7.mpdFiber[impd].eb_fifo_blk_count;
  *wordcnt = vtp->v7.mpdFiber[impd].eb_fifo_word_count;
  *eventcnt = vtp->v7.mpdFiber[impd].eb_fifo_event_count;
  VUNLOCK;
  return 0;
}

int
vtpPrintEbStatus(int id)
{
  unsigned int blockcnt, wordcnt, eventcnt;
  printf("MPD MAX RxLen 0: %d \n",sspGetMpdMaxRxLen(id, 0));
  printf("MPD MAX RxLen 1: %d \n",sspGetMpdMaxRxLen(id, 1));


  vtpGetEbStatus(id, &blockcnt, &wordcnt, &eventcnt);

  printf("%s: Block Count = %d, Word Count = %d, Event Count = %d\n", __func__,
    blockcnt, wordcnt, eventcnt);

  return(0);
}
#endif

int
vtpMpdSetCommonModeFilename(char *filename)
{
  strncpy(vtpMpdCommonModeFilename, filename, FNLEN);
  return 0;
}

int
vtpMpdGetCommonModeFilename(char *filename)
{
  strncpy(filename, vtpMpdCommonModeFilename, FNLEN);
  return 0;
}

int
vtpMpdSetPedestalFilename(char *filename)
{
  strncpy(vtpMpdPedestalFilename, filename, FNLEN);
  return 0;
}

int
vtpMpdGetPedestalFilename(char *filename)
{
  strncpy(filename, vtpMpdPedestalFilename, FNLEN);
  return 0;
}

int
vtpMpdSetPedestalFactor(float factor)
{
  vtpMpdPedestalFactor = factor;
  return 0;
}

int
vtpMpdGetPedestalFactor(float *factor)
{
  *factor = vtpMpdPedestalFactor;
  return 0;
}
