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
#include "vtpLib.h"

extern volatile ZYNC_REGS *vtp;
extern int VTP_FW_Version;
extern int VTP_FW_Type;

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

#define CHECKTYPE(v) {        \
    if( (v != VTP_FW_TYPE_COMMON) && (v != VTP_FW_Type) ) { \
      printf("%s: ERROR: VTP wrong firmware type (%d)\n",__func__,v);	\
      return ERROR;           \
    }               \
  }

/* MPD Control/Status Routines */
#if 0
int
vtpMpdMonEnable(int fiber)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if((fiber<0) || (fiber>31))
  {
    printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
    return ERROR;
  }

  VLOCK;
  vmeWrite32(&pSSP[id]->MPD[fiber].MonCtrl, 0x1); // reset
  vmeWrite32(&pSSP[id]->MPD[fiber].MonCtrl, 0x2); // enable write
  VUNLOCK;
  return OK;
}

int
vtpMpdMonDump(int fiber)
{
  int i;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if((fiber<0) || (fiber>31))
  {
    printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
    return ERROR;
  }

  printf("%s(%d,%d)\n", __func__, id, fiber);
  VLOCK;
  vmeWrite32(&pSSP[id]->MPD[fiber].MonCtrl, 0x0); // disable write

  for(i=0;i<65536;i++)
  {
    int word, rx_d;
    int isk, iscomma, disperr, notintable, clkcorcnt, bufstatus, realign;
    word = vmeRead32(&pSSP[id]->MPD[fiber].MonStatus);

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

int
vtpMpdSetAvg(int fiber, int apv, int min, int max)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if((fiber<0) || (fiber>31))
    {
      printf("%s: ERROR: invalid fiber %d\n",__func__,fiber);
      return ERROR;
    }
  if((apv<0) || (apv>15))
    {
      printf("%s: ERROR: invalid apv %d\n",__func__,apv);
      return ERROR;
    }

  val = ((min & 0x1fff)<<0) |
        ((max & 0x1fff)<<13) |
        (apv<<26);

  VLOCK;
  vmeWrite32(&pSSP[id]->MPD[fiber].Avg, val | 0x80000000);
  //vmeWrite32(&pSSP[id]->MPD[fiber].Avg, val);
  VUNLOCK;
  return OK;
}

int
vtpMpdSetApvOffset(int fiber, int apv, int strip, int offset)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if((fiber<0) || (fiber>31))
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

  val = (offset & 0x1fff) | (strip<<16) | (apv<<23);

  VLOCK;
      vmeWrite32(&pSSP[id]->MPD[fiber].ApvOffset, val | 0x80000000);
      //  vmeWrite32(&pSSP[id]->MPD[fiber].ApvOffset, val);
  VUNLOCK;
  return OK;
}

int
vtpMpdSetApvThreshold(int fiber, int apv, int strip, int threshold)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);
  if((fiber<0) || (fiber>31))
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

  val = (threshold & 0x1fff) | (strip<<16) | (apv<<23);
  // printf("%s(id=%d,fiber=%d,apv=%d,strip=%d,threshold=%d): 0x%08X\n",
  //     __func__, id, fiber, apv, strip, threshold, val);

  VLOCK;
      vmeWrite32(&pSSP[id]->MPD[fiber].ApvThreshold, val | 0x80000000);
      //vmeWrite32(&pSSP[id]->MPD[fiber].ApvThreshold, val);
  VUNLOCK;
  return OK;
}


int
vtpMpdFiberReset(int id)
{
  int impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      vmeWrite32(&pSSP[id]->MPD[impd].Ctrl, MPD_CTRL_GTX_RESET);
    }
  taskDelay(2);
  for(impd=0; impd<32; impd++)
    {
      vmeWrite32(&pSSP[id]->MPD[impd].Ctrl, 0);
    }
  VUNLOCK;
  return OK;
}

int
vtpMpdFiberLinkReset(unsigned int mpdmask)
{
  int impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      if(mpdmask & (1<<impd))
	vmeWrite32(&pSSP[id]->MPD[impd].Ctrl, MPD_CTRL_RESET);
    }
  taskDelay(2);
  for(impd=0; impd<32; impd++)
    {
      if(mpdmask & (1<<impd))
	vmeWrite32(&pSSP[id]->MPD[impd].Ctrl, 0);
    }
  VUNLOCK;
  return OK;
}

int
vtpMpdEbSetFlags(int build_all_samples, int build_debug_headers, int enable_cm)
{
  int impd=0, val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
    val = vmeRead32(&pSSP[id]->MPD[impd].EBCtrl) & 0xFFFFFFF1;
    if(build_all_samples) val |= 0x2;
    if(build_debug_headers) val |= 0x4;
    if(enable_cm) val |= 0x8;

    vmeWrite32(&pSSP[id]->MPD[impd].EBCtrl, val);
    }
  VUNLOCK;

  return OK;
}

int
vtpMpdEnable(unsigned int mpdmask)
{
  int impd=0;
  unsigned int rval = 0;
  int printFlag = 0;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      if(mpdmask & (1<<impd))
	{
	vmeWrite32(&pSSP[id]->MPD[impd].EBCtrl, MPD_EBCTRL_ENABLE);
	vmeWrite32(&pSSP[id]->MPD[impd].EBBusyThreshold, 0x100);
	rval = vmeRead32(&pSSP[id]->MPD[impd].EBBusyThreshold);
	if(printFlag)
	  printf("BusyTh set!!!0x%08X\n", rval);
	}
    }
  VUNLOCK;

  return OK;
}

int
vtpMpdDisable(unsigned int mpdmask)
{
  int impd=0, val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      if(mpdmask & (1<<impd))
	{
	val = vmeRead32(&pSSP[id]->MPD[impd].EBCtrl) & ~MPD_EBCTRL_ENABLE;
	vmeWrite32(&pSSP[id]->MPD[impd].EBCtrl, val);
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
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  printf("%s(id = %d, impd = %d): \n",
	 __func__, id, impd);
  VLOCK;
  vmeWrite32(&pSSP[id]->MPDSelector, impd);
  for(ireg = (0x0>>2); ireg < (0x200>>2); ireg++)
    {
      if((ireg%4)==0) printf("\n%05x\t",ireg<<2);
      res = vmeMemProbe((char *) &pSSP[id]->MPDregs[ireg],4,(char *)&rval);
      if(res<0)
	{
	  printf("  -BUSERROR-  ");
	  res=0;
	  errval=ERROR;
	}
      else
	printf("  0x%08x  ",rval);
    }
  printf("\n\n");

  VUNLOCK;

  return errval;
}

static int vtpMpdFiberSelected[MAX_VME_SLOTS+1];
static int vtpMpdFiberSelected_initd=0;

unsigned int
vtpMpdReadReg(int impd, unsigned int reg)
{
  unsigned int rval=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if(vtpMpdFiberSelected_initd==0)
    {
      memset(&vtpMpdFiberSelected,-1,sizeof(vtpMpdFiberSelected));
      vtpMpdFiberSelected_initd=1;
    }

  if(impd!=vtpMpdFiberSelected[id])
    {
      vmeWrite32(&pSSP[id]->MPDSelector, impd);
      vtpMpdFiberSelected[id]=impd;
    }

  rval = vmeRead32(&pSSP[id]->MPDregs[reg>>2]);

  return rval;
}

int
vtpMpdWriteReg(int impd, unsigned int reg, unsigned int value)
{
  unsigned int rval=OK;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  if(vtpMpdFiberSelected_initd==0)
    {
      memset(&vtpMpdFiberSelected,-1,sizeof(vtpMpdFiberSelected));
      vtpMpdFiberSelected_initd=1;
    }

  if(impd!=vtpMpdFiberSelected[id])
    {
      vmeWrite32(&pSSP[id]->MPDSelector, impd);
      vtpMpdFiberSelected[id]=impd;
    }

  vmeWrite32(&pSSP[id]->MPDregs[reg>>2],value);

  return rval;
}

int
vtpMpdGetSoftErrorCount(int fiber)
{
  int result;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  result = (vmeRead32(&pSSP[id]->MPD[fiber].Status) >> 8) & 0xFF;
  VUNLOCK;

  return result;
}

int
vtpMpdPrintStatus(int id)
{
  MPD_regs mr[32];
  int impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      mr[impd].Ctrl   = vmeRead32(&pSSP[id]->MPD[impd].Ctrl);
      mr[impd].Status = vmeRead32(&pSSP[id]->MPD[impd].Status);
      mr[impd].EBCtrl = vmeRead32(&pSSP[id]->MPD[impd].EBCtrl);
    }
  VUNLOCK;

  printf("\n");
  printf("                               SSP - Slot %2d\n",id);
  printf("                           MPD Settings and Status\n\n");
  printf("     Channel   -------ERRORS------     Event\n");
  printf("MPD    Up      HARD   FRAME   SOFT    Builder\n");
  printf("--------------------------------------------------------------------------------\n");
  for(impd=0; impd<32; impd++)
    {
      printf("%2d    ",impd);

      printf("%s      ",(mr[impd].Status & MPD_STATUS_CHANNELUP)?" UP ":"DOWN");

      printf("%s    ",(mr[impd].Status & MPD_STATUS_HARDERROR)?"ERR":"---");

      printf("%s     ",(mr[impd].Status & MPD_STATUS_FRAMEERROR)?"ERR":"---");

      if(mr[impd].Status & MPD_STATUS_SOFTERRORS)
	{
	  printf("%3d    ",(mr[impd].Status & MPD_STATUS_SOFTERRORS)>>8);
	}
      else
	printf("---    ");

      printf("%s",(mr[impd].EBCtrl & MPD_EBCTRL_ENABLE)?"ENABLED ":"DISABLED");

      printf("\n");
    }

  printf("--------------------------------------------------------------------------------\n");
  printf("\n");
  printf("\n");

  return OK;
}

unsigned int
vtpMpdGetChanUpMask()
{
  unsigned int status = 0, rval = 0;
  int impd=0;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  for(impd=0; impd<32; impd++)
    {
      status = vmeRead32(&pSSP[id]->MPD[impd].Status);
      status &= MPD_STATUS_CHANNELUP;
      if(status)
	rval |= (1 << impd);
    }
  VUNLOCK;

  return rval;
}

int
vtpGetMpdMaxRxLen(int impd)
{
  int result;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  result = vmeRead32(&pSSP[id]->MPD[impd].MaxRxLen);
  VUNLOCK;
  return result;
}

int
vtpGetEB_wordCount(int impd)
{
  int result;

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);

  VLOCK;
  result = vmeRead32(&pSSP[id]->MPD[impd].EB_wordCount);
  VUNLOCK;
  return result;
}


int
vtpGetEbStatus(unsigned int *blockcnt, unsigned int *wordcnt, unsigned int *eventcnt)
{

  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_MPDRO);


  VLOCK;
  *blockcnt = vmeRead32(&pSSP[id]->EB.FifoBlockCnt);
  *wordcnt = vmeRead32(&pSSP[id]->EB.FifoWordCnt);
  *eventcnt = vmeRead32(&pSSP[id]->EB.FifoEventCnt);
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
