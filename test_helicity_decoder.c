/***********************************************************
 *
 *  fadc_vxs_list.c - Library of routines for the user to write for 
 *                    readout and buffering of events from JLab FADC using 
 *                    a JLab pipeline TI module and Linux VME controller. 
 *
 *                In this example, clock, syncreset, and trigger are      
 *                output from the TI then distributed using a  
 *                switch slot SD module 
 *  Modified by Arindam Sen (asen@jlab.org).
 *  Included the helicity generator board on May 9, 2025.
 *                                                         
 ***********************************************************/

 /* Event Buffer definitions */
 #define MAX_EVENT_POOL     10
 #define MAX_EVENT_LENGTH   1024*500      /* Size in Bytes */

 /* Define Interrupt source and address */
 #define TI_MASTER
 #define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
 #define TI_ADDR    (21<<19)          /* GEO slot 21 */

 #define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

 #include "dmaBankTools.h"
 #include "tiprimary_list.c" /* source required for CODA */
 #include "sdLib.h"
 #include "hdLib.h"

 /* Define helicity decoder address and bank*/
 #define HELICITY_DECODER_ADDR 0x00800000
 #define HELICITY_DECODER_BANK 0xDEC

 #define BUFFERLEVEL 1
 #define BLOCKLEVEL 1

  /* SD variables */
  static unsigned int sdScanMask = 0;

  /* trigger counts to renew the tiSoftTrig */
  unsigned int ntrigger=0;

  /* function prototype */
  void rocTrigger(int arg);


/****************************************
 *  DOWNLOAD
 ****************************************/

void rocDownload()
{
  unsigned int iflag;
  int ifa, stat;

  /*****************
   *   TI SETUP
   *****************/
  /*      
   * Set Trigger source                                                 
   *    For the TI-Master, valid sources:
   *      TI_TRIGGER_FPTRG     2  Front Panel "TRG" Input   
   *      TI_TRIGGER_TSINPUTS  3  Front Panel "TS" Inputs  
   *      TI_TRIGGER_TSREV2    4  Ribbon cable from Legacy TS module     
   *      TI_TRIGGER_PULSER    5  TI Internal Pulser (Fixed rate and/or random)   
   */

  tiSetTriggerSource(TI_TRIGGER_TSINPUTS); /* TS Inputs enabled */

  /* Enable set specific TS input bits (1-6) */
  tiEnableTSInput( TI_TSINPUT_1 );

  tiLoadTriggerTable(3);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

  /* Set the SyncReset width to 4 microSeconds */
  tiSetSyncResetType(1);

  /* Set initial number of events per block */
  blockLevel = BLOCKLEVEL;
  tiSetBlockLevel(blockLevel);
  
  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(BUFFERLEVEL);
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


  tiStatus(0);
 // sdStatus(0);

  printf("rocDownload: User Download Executed\n");
  printf("Last compiled date/time: %s %s\n",__DATE__, __TIME__);

  /* Initialize the library and module with helicity decoder internal clock*/
  hdInit(HELICITY_DECODER_ADDR, HD_INIT_INTERNAL, 0, 0);
  hdStatus(1);
}

/****************************************
 *  PRESTART
 ****************************************/

void rocPrestart()
{
  int ifa;

// functions for helicity decoder

  /* Switch helicity decoder to VXS master clock, trigger, syncreset */
  hdSetSignalSources(HD_INIT_VXS, HD_INIT_VXS, HD_INIT_VXS);

  /* Enable the module decoder, well before triggers are enabled */
  /* Setting data input (0x100 = 2048 ns) and
     trigger latency (0x40 = 512 ns) processing delays */
  //hdSetProcDelay(0x100, 0x40);

 /* Enable the module decoder, well before triggers are enabled */
 hdEnableDecoder();

 /* Using internal helicity generation for testing */
 hdSetHelicitySource(1, 0, 1);

/************************************************************************************************
 Time calculation for t_settle and t_stable (Arindam Sen)                                       
 For 125 MHz system clock, time base (the duration of a single count) or clock cycle is 8ns     
 Convert Hex to Dec (0x80)_{16}=(8×16)+(0×16)=(128)_{10}                                        
 Total Time = 128×8 ns = 1024 ns                                                                
 If we want to convert time to Hex code, below is one example for 20 μs                         
 Target Time (ns)=20 μs×1000=20,000 ns                                                          
 Decimal Count=20000ns/8ns=2500                                                                 
 Now covert it to Hex 2500_{10} = 9C4_{16}                                                      
 So the machine code will be 0x9C4                                                              
 Pattern mode: 0 = pair, 1 = quartet, 2 = octet, 3 = toggle                                     
 For Window Delay = just write the number like 16,64,128.....                                   
************************************************************************************************/

// hdHelicityGeneratorConfig(2,     /* Pattern = 2 (OCTET) */
//                         16,     /* Window Delay = 0 (0 windows) */
//                         0x500,  /* SettleTime = 0x40 (512 ns) */
//                         0x12500,  /* StableTime = 0x80 (1024 ns) */
//                         0xABCDEF01); /* Seed */

 hdEnableHelicityGenerator();

// end of helicity decoder

  tiSetBlockLevel(blockLevel);
  printf("rocPrestart: Block Level set to %d\n",blockLevel);

  tiSetEvTypeScalers(1);
  tiStatus(0);
  hdStatus(0);
//  faGStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 * GO  
 ****************************************/

void rocGo()
{
  //int fadc_mode = 0, pl=0, ptw=0, nsb=0, nsa=0, np=0;
  /* Get the current block level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n", __FUNCTION__,blockLevel);

  hdEnable();
  hdStatus(0);

}

void rocEnd()
{

  hdDisable();

  hdStatus(0);
  tiStatus(0);

  tiPrintEvTypeScalers();
  printf("rocEnd: Ended after %d events\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/

void rocTrigger(int arg)
{
  int ifa = 0, stat, nwords, dCnt;
  unsigned int datascan, scanmask;
  int roType = 2, roCount = 0, blockError = 0, timeout=0;

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

 /* Check for SYNC Event */
  if(tiGetSyncEventFlag() == 1)
    {
      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
        {
          printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
                 __func__, davail);
    }
   }


  BANKOPEN(HELICITY_DECODER_BANK, BT_UI4, blockLevel);
  while((hdBReady(0)!=1) && (timeout<100))
    {
      timeout++;
    }
  //printf(" timeout = %d \n", timeout);
  if(timeout>=100)
    {
      printf("%s: ERROR: TIMEOUT waiting for Helicity Decoder Block Ready\n",
             __func__);
    }
  else
    {
      dCnt = hdReadBlock(dma_dabufp, 1024>>2,1);

//        printf("%s: ERROR or NO data from hdReadBlock(...) = %d\n",
//               __func__, dCnt);

      if(dCnt<=0)
        {
          printf("%s: ERROR or NO data from hdReadBlock(...) = %d\n",
                 __func__, dCnt);
        }
      else
        {
          dma_dabufp += dCnt;
        }
    }

  BANKCLOSE;


}

void rocCleanup()
{
  printf("%s: Reset all modules \n",__FUNCTION__);
}

