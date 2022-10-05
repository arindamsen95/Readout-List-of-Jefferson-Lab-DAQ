void vtp_mpd_setup();
extern int I2C_SendStop(int id);

#define daLogMsg(__x, __y) printf(__x);printf(": ");printf(__y);printf("\n");
#define DALMAGO
#define DALMASTOP
char *apvbuffer = NULL;
char *bufp;

/* Filenames obtained from the platform or other config files */
char APV_CONFIG_FILENAME[250];

/* default config filenames, if they are not defined in COOL */
#define DEFAULT_APV_CONFIG "/home/sbs-onl/cfg/vtp_config_TS.cfg"

void
vtp_mpd_setup(char *filename)
{
  int useConfigFile = 0;

  if(filename != NULL)
    useConfigFile = 1;

  /*****************
   *   VTP SETUP
   *****************/
  int iFlag = 0;
  uint64_t vtpFiberBit = 0;
  uint64_t vtpFiberMaskToInit;

  apvbuffer = (char *)malloc(10*1024*sizeof(char));

  if(filename != NULL)
    {
      if(vtpMpdConfigInit(filename) == ERROR)
	{
	  printf("%s: Error using filename %s.\n",
		 __func__, filename);

	  printf("  trying %s\n",
		 DEFAULT_APV_CONFIG);

	  if(vtpMpdConfigInit(DEFAULT_APV_CONFIG) == ERROR)
	    {
	      daLogMsg("ERROR","Error loading APV configuration file");

	      return;
	    }
	  else
	    {
	      strncpy(APV_CONFIG_FILENAME, DEFAULT_APV_CONFIG, 250);
	    }
	}
      else
	{
	  strncpy(APV_CONFIG_FILENAME, filename, 250);
	}
    }

  vtpMpdConfigLoad();

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffffffffffff);

  /* ... the VTP holds all the MPD event build stuff in reset... */
  vtpMpdDisable(0xffffffffffffffff);

  /* setups up the MPD (so they clear their buffers) ... */

  /*****************
   *   MPD SETUP
   *****************/
  int rval = OK;
  uint64_t errSlotMask = 0;
  /* Index is the mpd / fiber... value mask if ADCs with APV config errors */
  uint32_t apvConfigErrorMask[VTP_MPD_MAX];
  uint32_t apvErrorTypeMask[VTP_MPD_MAX]; /* 0 : mpd init, 1: apv not found , 2: config */

  memset(apvConfigErrorMask, 0 , sizeof(apvConfigErrorMask));
  memset(apvErrorTypeMask, 0 , sizeof(apvErrorTypeMask));

  mpdSetPrintDebug(0);

  // discover MPDs and initialize memory mapping

  // In VTP mode, par1(fiber mask) and par3(number of mpds) are not used in mpdInit(par1, par2, par3, par4)
  // Instead, they come from the configuration file
  int initFlag = MPD_INIT_FIBER_MODE;
  uint64_t chanmask;

  if(useConfigFile)
    {
      chanmask = mpdGetVTPFiberMask();
    }
  else
    {
      chanmask = vtpMpdGetChanUpMask();
      mpdSetVTPFiberMap_preInit(chanmask);
      initFlag |= MPD_INIT_NO_CONFIG_FILE_CHECK;
    }
  mpdInitVTP(chanmask, initFlag);
  int fnMPD = mpdGetNumberMPD();


  //fnMPD = 1;
  if (fnMPD<=0) { // test all possible vme slot ?
    printf("ERR: no MPD discovered, cannot continue\n");
    return;
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
	errSlotMask |= (1ull << (uint64_t)i);
	continue;
      }

    printf(" - APV Reset\n");
    fflush(stdout);
    if (mpdI2C_ApvReset(i) != OK)
      {
	printf(" * * FAILED\n");
	error_status = ERROR;
	errSlotMask |= (1ull << (uint64_t)i);
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
      errSlotMask |= (1ull << (uint64_t)i);

    // configure adc on MPD
    printf("Configure ADC on MPD slot %d\n",i);
    mpdADS5281_Config(i);

    // configure fir
    // not implemented yet

    // 101 reset on the APV
    printf("Do 101 Reset on MPD slot %d\n",i);
    mpdAPV_Reset101(i);

    // <- MPD+APV initialization ends here
    //sleep(1);
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
  int iapv;
  uint64_t ifiber;
  for (ifiber = 0; ifiber < VTP_MPD_MAX; ifiber++)
    {

      if( ((1ull << ifiber) & mpdGetVTPFiberMask()) == 0)
	continue;


      /* Build the ADCmask of those in the config file */
      uint32_t configAdcMask = 0;
      for (iapv = 0; iapv < mpdGetNumberAPV(ifiber); iapv++)
	{
	  if(mpdApvGetAdc(ifiber,iapv) > -1)
	    {
	      configAdcMask |= (1 << mpdApvGetAdc(ifiber,iapv));
	      apvErrorTypeMask[ifiber] |= (1 << 1);
	    }
	}

      if(mpdGetFpgaRevision(ifiber) == 0)
	apvErrorTypeMask[ifiber] = (1 << 0);

      /* if (mpdGetApvEnableMask(ifiber) != 0) */
	{
	  rval = sprintf(bufp, "  MPD %2d : ", (int)ifiber);
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
	      if (mpdGetApvEnableMask(ifiber) & (1 << ibit))
		{
		  if(apvConfigErrorMask[ifiber] & (1 << ibit))
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
		  errSlotMask |= (1ull << ifiber);
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
	  if(errSlotMask & (1ull << ifiber))
	    {
	      rval = sprintf(bufp, " %s  %s  %s\n",
			     (apvErrorTypeMask[ifiber] & 0x1) ? "*MPD NotFound*" :
			     "              ",
			     (apvErrorTypeMask[ifiber] & 0x2) ? "*APV NotFound*" :
			     "              ",
			     (apvErrorTypeMask[ifiber] & 0x4) ? "*APV Config*" :
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

  DALMAGO;
  printf("%s",apvbuffer);
  DALMASTOP;

  if ((errSlotMask != 0) || (error_status != OK))
    {
      daLogMsg("ERROR", "MPD initialization errors");
    }

  mpdGStatus(1);

  if(apvbuffer)
    free(apvbuffer);
}
