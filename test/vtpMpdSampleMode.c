
/*
 * File:
 *    vtpMpdSampleMode.c
 *
 * Description:
 *    Sample mode data taking for fish / goalpost plots
 *
 *   if a filename is provided, only initialize those MPD defined
 *
 *   otherwise, only attempt to initialize MPDs found via the fiber+serial
 *    connection (channel must be up).
 *
 *
 * Usage:
 *      vtpMpdSampleMode
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "vtp.h"
#include "vtpLib.h"
#include "vtpConfig.h"
#include "vtpMpdConfig.h"
#define VTP
#include "mpdLib.h"
char *apvbuffer;
char *errorbuffer;
char *bufp;

int fnMPD=0;

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);


extern int I2C_SendStop(int id);
/* routine prototype */
void mpdSampleTest();

int main(int argc, char *argv[])
{
  int stat;
  apvbuffer = (char *)malloc(1024*50*sizeof(char));
  errorbuffer = (char *)malloc(1024*50*sizeof(char));
  int useConfigFile = 1;
  char filename[250];
  char rol_usrConfig[250] = "/home/sbs-onl/vtp/cfg/sbsvtp2.config";
  char shortHostname[HOST_NAME_MAX];

  if(argc > 1)
    {
      /* assume the only argument is the path to the config file */
      strncpy(filename, argv[1], sizeof(filename));
    }
  else
    {
      stat = getShortHostname(shortHostname);
      sprintf(filename, "/home/sbs-onl/cfg/%s_apv.cfg",shortHostname);
      sprintf(rol_usrConfig, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);
    }

  if(vtpMpdConfigInit(filename) == ERROR)
    {
      printf("ERROR: Error in configuration file\n\t%s", filename);
      return -1;
    }

  vtpMpdConfigLoad();

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpCheckMutexHealth(1);
  vtpLock();

  uint32_t vtpFiberMaskToInit;

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffff);

  vtpMpdDisable(0xffffffff);

  /* setups up the MPD (so they clear their buffers) ... */

  /*****************
   *   MPD SETUP
   *****************/
  int rval = OK;
  unsigned int errSlotMask = 0;
  /* Index is the mpd / fiber... value mask if ADCs with APV config errors */
  uint32_t apvConfigErrorMask[32];
  uint32_t apvErrorTypeMask[32]; /* 0 : mpd init, 1: apv not found , 2: config */

  memset(apvConfigErrorMask, 0 , sizeof(apvConfigErrorMask));
  memset(apvErrorTypeMask, 0 , sizeof(apvErrorTypeMask));

  mpdSetPrintDebug(0);

  // discover MPDs and initialize memory mapping

  // In VTP mode, par1(fiber mask) and par3(number of mpds) are not used in mpdInit(par1, par2, par3, par4)
  // Instead, they come from the configuration file
  unsigned int chanmask = vtpMpdGetChanUpMask();

  chanmask = mpdGetVTPFiberMask();
  mpdInitVTP(chanmask, MPD_INIT_FIBER_MODE | MPD_INIT_NO_CONFIG_FILE_CHECK);
  fnMPD = mpdGetNumberMPD();


  //fnMPD = 1;
  if (fnMPD<=0) { // test all possible vme slot ?
    printf("ERR: no MPD discovered, cannot continue\n");
    return -1;
  }

  printf(" MPD discovered = %d\n",fnMPD);

  // APV configuration on all active MPDs
  int error_status = OK;
  int k, i;
  for (k=0;k<fnMPD;k++) { // only active mpd set
    i = mpdSlot(k);

    int try_cnt = 0;

  retry:
    error_status = OK;

    printf(" Try initialize I2C mpd in slot %d\n",i);
    if (mpdI2C_Init(i) != OK) {
      printf("WRN: I2C fails on MPD %d\n",i);
    }

    printf("Try APV discovery and init on MPD slot %d\n",i);
    if (mpdAPV_Scan(i)<=0 && try_cnt < 3 ) { // no apd found, skip next
      try_cnt++;
      printf("failing retrying\n");
      goto retry;
    }



    if( try_cnt == 3 )
      {
	printf("APV blind scan failed for %d TIMES !!!!\n\n", try_cnt);
	errSlotMask |= (1 << i);
	continue;
      }

    printf(" - APV Reset\n");
    fflush(stdout);
    if (mpdI2C_ApvReset(i) != OK)
      {
	printf(" * * FAILED\n");
	error_status = ERROR;
	errSlotMask |= (1 << i);
      }

    usleep(10);
    I2C_SendStop(i);


    // board configuration (APV-ADC clocks phase)
    printf("Do DELAY setting on MPD slot %d\n",i);
    mpdDELAY25_Set(i, mpdGetAdcClockPhase(i,0), mpdGetAdcClockPhase(i,1));



    // apv configuration
    printf("Configure %d APVs on MPD slot %d\n",mpdGetNumberAPV(i),i);


    // apv configuration
    mpdSetPrintDebug(0);
    printf(" - Configure Individual APVs\n");
    printf(" - - ");
    fflush(stdout);
    int itry, badTry = 0, iapv, saveError = error_status;
    error_status = OK;
    for (itry = 0; itry < 3; itry++)
      {
	if(badTry)
	  {
	    printf(" ******** RETRY ********\n");
	    printf(" - - ");
	    fflush(stdout);
	    error_status = OK;
	  }
	badTry = 0;
	for (iapv = 0; iapv < mpdGetNumberAPV(i); iapv++)
	  {
	    printf("%2d ", iapv);
	    fflush(stdout);

	    if (mpdAPV_Config(i, iapv) != OK)
	      {
		printf(" * * FAILED for APV %2d\n", iapv);
		if(iapv < (mpdGetNumberAPV(i) - 1))
		  printf(" - - ");
		fflush(stdout);
		error_status = ERROR;
		apvConfigErrorMask[i] |= (1 << mpdApvGetAdc(i,iapv));
		apvErrorTypeMask[i] |= (1 << 2);
		badTry = 1;
	      }
	    else
	      {
		apvConfigErrorMask[i] &= ~(1 << mpdApvGetAdc(i,iapv));
	      }
	  }
	printf("\n");
	fflush(stdout);
	if(badTry)
	  {
	    printf(" ***** APV RESET *****\n");
	    fflush(stdout);
	    mpdI2C_ApvReset(i);
	  }
	else
	  {
	    if(itry > 0)
	      {
		printf(" ****** SUCCESS!!!! ******\n");
		error_status = OK;
		apvConfigErrorMask[i] = 0;
		apvErrorTypeMask[i] &= ~(1 << 2);
		fflush(stdout);
	      }
	    break;
	  }

      }

    error_status |= saveError;

    if(error_status == ERROR)
      errSlotMask |= (1 << i);

    // configure adc on MPD
    printf("Configure ADC on MPD slot %d\n",i);
    mpdADS5281_Config(i);

    // configure fir
    // not implemented yet

    // 101 reset on the APV
    printf("Do 101 Reset on MPD slot %d\n",i);
    mpdAPV_Reset101(i);

    // <- MPD+APV initialization ends here
    sleep(1);
  } // end loop on mpds
  //END of MPD configure

  // summary report
  bufp = (char *) &(apvbuffer[0]);

  rval = sprintf(bufp, "\n");
  if(rval > 0)
    bufp += rval;
  rval = sprintf(bufp, "Configured APVs (ADC 15 ... 0)            --------------------ERRORS------------------\n");
  if(rval > 0)
    bufp += rval;

  int ibit;
  int ifiber, id, iapv;
  for (ifiber = 0; ifiber < 32; ifiber++)
    {

      id = ifiber;

      if( ((1 << id) & mpdGetVTPFiberMask()) == 0)
	continue;


      /* Build the ADCmask of those in the config file */
      uint32_t configAdcMask = 0;
      for (iapv = 0; iapv < mpdGetNumberAPV(id); iapv++)
	{
	  if(mpdApvGetAdc(id,iapv) > -1)
	    {
	      configAdcMask |= (1 << mpdApvGetAdc(id,iapv));
	      apvErrorTypeMask[ifiber] |= (1 << 1);
	    }
	}

      if(mpdGetFpgaRevision(ifiber) == 0)
	apvErrorTypeMask[ifiber] = (1 << 0);

      /* if (mpdGetApvEnableMask(id) != 0) */
	{
	  rval = sprintf(bufp, "  MPD %2d : ", id);
	  if(rval > 0)
	    bufp += rval;
	  iapv = 0;
	  for (ibit = 15; ibit >= 0; ibit--)
	    {
	      if (((ibit + 1) % 4) == 0)
		{
		  rval = sprintf(bufp, " ");
		  if(rval > 0)
		    bufp += rval;
		}
	      if (mpdGetApvEnableMask(id) & (1 << ibit))
		{
		  if(apvConfigErrorMask[id] & (1 << ibit))
		    {
		      rval = sprintf(bufp, "C");
		    }
		  else
		    {
		      rval = sprintf(bufp, "1");
		    }
		  if(rval > 0)
		    bufp += rval;
		  iapv++;
		}
	      else if(configAdcMask & (1 << ibit))
		{
		  rval = sprintf(bufp, "E");
		  errSlotMask |= (1 << id);
		  if(rval > 0)
		    bufp += rval;
		}
	      else
		{
		  rval = sprintf(bufp, ".");
		  if(rval > 0)
		    bufp += rval;
		}
	    }
	  rval = sprintf(bufp, " (#APV %2d)", iapv);
	  if(rval > 0)
	    bufp += rval;
	  if(errSlotMask & (1 << id))
	    {
	      rval = sprintf(bufp, " %s  %s  %s\n",
			     (apvErrorTypeMask[id] & 0x1) ? "*MPD NotFound*" :
			     "              ",
			     (apvErrorTypeMask[id] & 0x2) ? "*APV NotFound*" :
			     "              ",
			     (apvErrorTypeMask[id] & 0x4) ? "*APV Config*" :
			     "");
	      if(rval > 0)
		bufp += rval;
	    }
	  else
	    {
	      rval = sprintf(bufp, "\n");
	      if(rval > 0)
		bufp += rval;
	    }
	}
    }
  rval = sprintf(bufp, "\n");
  if(rval > 0)
    bufp += rval;

  printf("%s",apvbuffer);

  if ((errSlotMask != 0) || (error_status != OK))
    {
      printf("ERROR: MPD initialization errors\n");
    }

  mpdSampleTest();


  vtpUnlock();


  return 0;
}

void
mpdSampleTest()
{
  /**********************************
   * SAMPLES TEST
   * sample APV output at 40 MHz
   * only for testing not for normal daq
   **********************************/
  int acq_mode = 0x2;
  FILE *fout;
  char *outfile = "outfile.txt";
  int clp_clock0 = 0;
  int clp_clock1 = 50;
  int clp_clockd = 2;
  int k,h,i,j,kk, error_count;
  int scount, sfreq;
  int sch0, sch1;
  int rdone, rtout;
  int fnMPD = mpdGetNumberMPD();
  uint16_t mfull, mempty;
#define MAX_HDATA 4096
#define MAX_SDATA 1024
  uint32_t hdata[MAX_HDATA]; // histogram data buffer
  uint32_t sdata[MAX_SDATA]; // samples data buffer

  if (acq_mode & 0x2) {

    fout = fopen(outfile,"w");
    if (fout == NULL) { fout = stdout; }

    fprintf(fout,"# SAMPLE MODE OUTPUT\n");
    fprintf(fout,"# CLOCK_RANGE: %d %d %d\n",clp_clock0, clp_clock1, clp_clockd);

    for (k=0;k<fnMPD;k++) { // only active mpd set
      i = mpdSlot(k);

      mpdSetAcqMode(i, "sample");
      mpdSetEventBuilding(i, 0);

      // load pedestal and thr default values
      mpdPEDTHR_Write(i);

      mpdSetApvEnableMask(i, 0x7fff);
      for (kk=clp_clock0;kk<clp_clock1;kk+=clp_clockd) { // loop on clock phases
	// set clock phase
	mpdDELAY25_Set(i, kk, kk);

	// enable acq
	mpdDAQ_Enable(i);

	// wait for FIFOs to get full
	mfull=0;
	mempty=1;
	printf("MPD / Clock : %d %d\n",i, kk);

	rtout = 2;
	do {
	  mpdFIFO_GetAllFlags(i,&mfull,&mempty);
	  printf("SAMPLE test wait fifo full: %x (%x) %x\n",
		 mfull,mpdGetApvEnableMask(i),mempty);
	  sleep(1);
	  rtout--;
	} while ((mfull!=mpdGetApvEnableMask(i)) && (rtout>0));

	// read data from FIFOs and estimate synch pulse period
	for (j=0;j<16;j++) { // 16 = number of ADC channel in one MPD
	  if (mpdGetApvEnableMask(i) & (1<<j)) { // apv is enabled
	    mpdFIFO_Samples(i,j, sdata, &scount, MAX_SDATA, &error_count);

	    printf("i: %d",i);
	    if (error_count != 0) {
	      printf("ERROR returned from FIFO_Samples %d\n",error_count);
	      continue;
	    }

	    fprintf(fout,"# MPD_ADC_COUNT_CLOCK_TOUT: %d %d %d %d %d\n",i, j, scount, kk, rtout);
	    printf("MPD/APV : %d / %d, peaks around: ",i,j);
	    sch0=-1;
	    sch1=-1;
	    sfreq=0;
	    for (h=0;h<scount;h++) { // detects synch peaks
	      //    printf("%04d ", sdata[h]); // output data
	      fprintf(fout,"%d ",sdata[h]);
	      if (sdata[h]>2500) { // threshold could be lower
	     	if (sch0<0) { sch0=h; sch1=sch0;} else {
		  if (h==(sch1+1)) { sch1=h; } else {
		    printf("%d-%d (v %d) ",sch0,sch1, sdata[h]);
		    sch0=h;
		    sch1=sch0;
		    sfreq++;
		  }
		}
	      }
	    }
	    fprintf(fout,"\n");
	    printf("\n Estimated synch period = %f (us) ,expected (20MHz:1.8, 40MHz:0.9)\n",((float) scount)/sfreq/40.0);
	  }
	}

      } // end loop on clock phases

    } // end loop on mpds

    if (fout != stdout)  fclose(fout);

  } // sample check mode
}

int
getShortHostname(char *shortHostname)
{
  char longHostname[HOST_NAME_MAX];
  char *tempShort;
  int rval;

  rval = gethostname(longHostname, HOST_NAME_MAX);
  if(rval < 0)
    {
      perror("gethostname");
      return rval;
    }

  printf("long Hostname : %s\n", longHostname);

  tempShort = strtok((char *)&longHostname,".");
  if(tempShort != NULL)
    {
      printf("short Hostname : >%s<\n", tempShort);
      strcpy(shortHostname,tempShort);
    }
  else
    printf("null\n");

  return rval;
}


/*
  Local Variables:
  compile-command: "make -k -B vtpMpdSampleMode"
  End:
 */
