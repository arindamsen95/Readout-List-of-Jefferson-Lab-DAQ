/*************************************************************************
 *
 *  fadc_list.c - Library of routines for the user to write for
 *                readout and buffering of events from JLab FADC using
 *                a JLab pipeline TI module and Linux VME controller.
 *  
 *                In this example, clock, syncreset, and trigger are
 *                output from the TI then distributed using a Front Panel SDC.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*60      /* Size in Bytes */

/* Define Interrupt source and address */
#define TI_MASTER
#define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TI_ADDR    (21<<19)          /* GEO slot 21 */

#define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */
#include "fadcLib.h"        /* library of FADC250 routines */
#include "fadc250Config.h"
#include "sdLib.h"

//unsigned int blockLevel=1;
//blockLevel=1;
#define BUFFERLEVEL 1
#define BLOCKLEVEL 1

/* FADC Library Variables */
extern int fadcA32Base, nfadc;
#define NFADC     2
/* Address of first fADC250 */
#define FADC_ADDR (4<<19)
/* Increment address to find next fADC250 */
#define FADC_INCR (1<<19)
#define FADC_BANK 0x3
/*
#define FADC_WINDOW_LAT    375
#define FADC_WINDOW_WIDTH   24
#define FADC_MODE           10*/

#define FADC_READ_CONF_FILE {                   \
    fadc250Config("/home/mollerdaq/fadc250/testpid.cnf");                          \
    if(rol->usrConfig)                          \
      fadc250Config(rol->usrConfig);            \
  }

/* for the calculation of maximum data words in the block transfer */
unsigned int MAXFADCWORDS=0;

/* function prototype */
void rocTrigger(int arg);

/* SD variables */
static unsigned int sdScanMask = 0;

void
rocDownload()
{
  unsigned short iflag;
  int ifa, stat;

  /* Configure TI */
  blockLevel = BLOCKLEVEL;
  tiSetBlockBufferLevel(BUFFERLEVEL);
  tiSetBlockLevel(blockLevel);

  tiSetTriggerSource(TI_TRIGGER_TSINPUTS);

  /* Set needed TS input bits */
  //tiEnableTSInput( TI_TSINPUT_1 );
  tiEnableTSInput( TI_TSINPUT_2 );

  /* Load the trigger table that associates 
     All TS inputs as a physics trigger
  */
  tiLoadTriggerTable(3);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);
  tiSetTriggerHoldoff(3,10,0);
  tiSetTriggerHoldoff(4,10,0);

  /* Set the SyncReset width to 4 microSeconds */
  tiSetSyncResetType(1);

  /* Sync event every 1000 blocks */
  tiSetSyncEventInterval(0);

  /* Set L1A prescale ... rate/(x+1) */
  tiSetPrescale(0);

  /* Set TS input #1 prescale rate/(2^(x-1) + 1)*/
  tiSetInputPrescale(1, 0);

  /* Add trigger latch pattern to datastream */
  tiSetFPInputReadout(1);

  /* Init the SD library so we can get status info */
  sdScanMask = 0;
  stat = sdInit(0);
  if (stat != OK)
    {
      printf("%s: WARNING: sdInit() returned %d\n",__func__, stat);
      daLogMsg("ERROR","SD not found");
    }

  /* Program/Init FADC Modules Here */
  //iflag = 0; /* SDC Board address */
  //iflag |= FA_INIT_EXT_SYNCRESET;  /* Front panel sync-reset */
  //iflag |= FA_INIT_FP_TRIG;  /* Front Panel Input trigger source */
  //iflag |= FA_INIT_FP_CLKSRC;  /* Internal 250MHz Clock source */
  iflag = 0; /* NO SDC */
  iflag |= (1<<0);  /* VXS sync-reset */
  iflag |= FA_INIT_VXS_TRIG;  /* VXS trigger source */
  iflag |= FA_INIT_VXS_CLKSRC;  /* VXS 250MHz Clock source */
  iflag |= FA_INIT_SKIP_FIRMWARE_CHECK;  /* ignore firmware check */

  printf("fadc iflag: %x\n",iflag);
  fadcA32Base = 0x09000000;

  vmeSetQuietFlag(1);
  faInit(FADC_ADDR, FADC_INCR, NFADC, iflag);
  vmeSetQuietFlag(0);

  /* Just one FADC250 */
  if(nfadc == 1)
    faDisableMultiBlock();
  else
    faEnableMultiBlock(1);

  /* configure all modules based on config file */
  FADC_READ_CONF_FILE;

 for(ifa = 0; ifa < nfadc; ifa++)
    {
      /* Bus errors to terminate block transfers (preferred) */
      faEnableBusError(faSlot(ifa));

      /*trigger-related*/
      faResetMGT(faSlot(ifa),1);
      faSetTrigOut(faSlot(ifa), 7);

      /* Enable busy output when too many events are being processed */
      faSetTriggerBusyCondition(faSlot(ifa), 3);

    }


  sdSetActiveVmeSlots(faScanMask()); /* Tell the sd where to find the fadcs */

  tiStatus(0);
  faGStatus(0);
  sdStatus(0);

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  int ifa;

  /* Program/Init VME Modules Here */
  for(ifa=0; ifa < nfadc; ifa++)
    {
      faSoftReset(faSlot(ifa),0);
      faResetToken(faSlot(ifa));
      faResetTriggerCount(faSlot(ifa));
      faEnableSyncReset(faSlot(ifa));
    }

  tiSetBlockLevel(blockLevel);
  printf("rocPrestart: Block Level set to %d\n",blockLevel);
  tiStatus(0);
  faGStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  int fadc_mode = 0, pl=0, ptw=0, nsb=0, nsa=0, np=0;
  /* Get the current block level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
         __FUNCTION__,blockLevel);

  faGSetBlockLevel(blockLevel);

  /* Get the FADC mode and window size to determine max data size */
  faGetProcMode(faSlot(0), &fadc_mode, &pl, &ptw,
		&nsb, &nsa, &np);

  /* Set Max words from fadc (proc mode == 1 produces the most)
     nfadc * ( Block Header + Trailer + 2  # 2 possible filler words
               blockLevel * ( Event Header + Header2 + Timestamp1 + Timestamp2 +
	                      nchan * (Channel Header + (WindowSize / 2) )
             ) +
     scaler readout # 16 channels + header/trailer
   */
  MAXFADCWORDS = nfadc * (4 + blockLevel * (4 + 16 * (1 + (ptw / 2))) + 18);

  /*  Enable FADC */
  faGEnable(0, 0);
  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{

  /* FADC Disable */
  faGDisable(0);

  /* FADC Event status - Is all data read out */
  faGStatus(0);

  tiStatus(0);

  //faGReset(0);

  printf("rocEnd: Ended after %d events\n",tiGetIntCount());

}

void
rocTrigger(int arg)
{
  int ifa=0, stat, nwords, dCnt;
  unsigned int datascan, scanmask;
  int roType=2, roCount = 0, blockError = 0;

  roCount = tiGetIntCount();

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,3,0);

  dCnt = tiReadTriggerBlock(dma_dabufp);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      dma_dabufp += dCnt;
    }

  /* fADC250 Readout */
  BANKOPEN(FADC_BANK,BT_UI4,0);

  /* Mask of initialized modules */
  scanmask = faScanMask();
  /* Check scanmask for block ready up to 100 times */
  datascan = faGBlockReady(scanmask, 100);
  stat = (datascan == scanmask);

  if(stat)
    {
      if(nfadc == 1)
	roType = 1;   /* otherwise roType = 2   multiboard reaodut with token passing */
      nwords = faReadBlock(0, dma_dabufp, MAXFADCWORDS, roType);

      /* Check for ERROR in block read */
      blockError = faGetBlockError(1);

      if(blockError)
	{
	  printf("ERROR: Slot %d: in transfer (event = %d), nwords = 0x%x\n",
		 faSlot(ifa), roCount, nwords);

	  for(ifa = 0; ifa < nfadc; ifa++)
	    faResetToken(faSlot(ifa));

	  if(nwords > 0)
	    dma_dabufp += nwords;
	}
      else
	{
	  dma_dabufp += nwords;
	  faResetToken(faSlot(0));
	}
    }
  else
    {
      printf("ERROR: Event %d: Datascan != Scanmask  (0x%08x != 0x%08x)\n",
	     roCount, datascan, scanmask);
    }
  BANKCLOSE;

  /* Check for SYNC Event */
  if(tiGetSyncEventFlag() == 1)
    {
      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
        {
          printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
                 __func__, davail);

          while(tiBReady())
            {
              vmeDmaFlush(tiGetAdr32());
            }
        }

      for(ifa = 0; ifa < nfadc; ifa++)
        {
          davail = faBready(faSlot(ifa));
          if(davail > 0)
            {
              printf("%s: ERROR: fADC250 Data available after readout in SYNC event \n",
                     __func__, davail);

              while(faBready(faSlot(ifa)))
                {
                  vmeDmaFlush(faGetA32(faSlot(ifa)));
                }
            }
        }
    }

}

void
rocCleanup()
{

  printf("%s: Reset all FADCs\n",__FUNCTION__);
  faGReset(1);

}


