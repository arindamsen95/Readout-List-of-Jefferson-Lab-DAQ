int
vtpRocStatus(int flag)
{

  int  ii, status, fw_version, fw_type, timestamp;
  unsigned int ctrl, tcp_ctrl, tcp_state, tcp_status, ti[4], rocid, roc[6],
    tiTrigCnt, eb_ctrl, eb_status,  ebiotx[2], ebiorx[2], evioBank[3], slot[16], ppState[16];

  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;
  ctrl       = vtp->clk.Ctrl;
  status     = vtp->clk.Status;
  fw_version = vtp->clk.FW_Version;
  fw_type    = vtp->clk.FW_Type;
  timestamp  = vtp->clk.FW_Timestamp;

  tcp_ctrl   = vtp->tcpClient[0].Ctrl;
  tcp_state  = vtp->tcpClient[0].IP4_TCPStateStatus;
  tcp_status = vtp->tcpClient[0].IP4_TCPStatus;

  ti[0]      = vtp->tiLink.Ctrl;
  ti[1]      = vtp->tiLink.LinkStatus;
  ti[2]      = vtp->tiLink.Status;
  ti[3]      = vtp->tiLink.EBStatus;

  ebiotx[0] =  vtp->v7.ebioTx[0].Ctrl;
  ebiotx[1] =  vtp->v7.ebioTx[0].Status;
  ebiorx[0] =  vtp->ebiorx[0].Ctrl;
  ebiorx[1] =  vtp->ebiorx[0].Status;

  eb_ctrl      = vtp->v7.rocEB.Ctrl;
  eb_status    = vtp->v7.rocEB.Status;
  evioBank[0]  = vtp->v7.rocEB.evio_cfg[0];
  evioBank[1]  = vtp->v7.rocEB.evio_cfg[1];
  evioBank[2]  = vtp->v7.rocEB.evio_cfg[2];
  for(ii=0;ii<16;ii++) {
    slot[ii]    = (vtp->v7.rocEB.pp_cfg[ii])&0x3;
    ppState[ii] = vtp->v7.rocEB.pp_state[ii];
  }

  rocid      = vtp->roc.rocID;
  tiTrigCnt  = vtp->roc.TiTriggerCnt;
  roc[0]     = vtp->roc.Ctrl;
  roc[1]     = vtp->roc.State;
  roc[2]     = vtp->roc.CpuSyncEventStatus;
  roc[3]     = vtp->roc.CpuSyncEventLenStatus;
  roc[4]     = vtp->roc.CpuAsyncEventStatus;
  roc[5]     = vtp->roc.CpuAsyncEventLenStatus;

  VUNLOCK;

  printf("---------------------------------------\n");
  printf("--VTP (Z7) Statistics                --\n");
  printf("---------------------------------------\n");
  printf("Clock:\n");
  printf("  Global PLL locked: %d\n", (status>>0) & 0x1);
  printf("\n");
  printf("Control Reg: %08x\n", ctrl);
  printf("\n");
  printf("Firmware:\n");
  printf("  Type: %2d\n", fw_type);
  printf("  Version: %d.%d\n", (fw_version>>16) & 0xFFFF, (fw_version>>0) & 0xFFFF);
  printf("  Timestamp: 0x%08X : %d/%d/%d %d:%d:%d \n", timestamp,
	 ((timestamp>>17)&0x3f)+2000, ((timestamp>>23)&0xf), ((timestamp>>27)&0x1f),
	 ((timestamp>>12)&0x1f), ((timestamp>>6)&0x3f), ((timestamp>>0)&0x3f) );
  printf("\n\n");

  printf("TCP LINK Status:\n");
  printf("    Ctrl            = %08x\n",tcp_ctrl);
  printf("    State           = %08x\n",tcp_state);
  printf("    Status          = %08x\n",tcp_status);

  printf("\nEBIO (EB->ROC) Link:\n");
  printf("  TX Control : Status: 0x%08X | 0x%08X\n", ebiotx[0],ebiotx[1]);
  printf("  RX Control : Status: 0x%08X | 0x%08X\n", ebiorx[0],ebiorx[1]);

  printf("\n");
  printf("VTP ROC Status (ID = %d):\n",rocid);
  printf("    TI Trigger Cnt  = %d\n",tiTrigCnt);
  printf("    TI Link  Ctrl   = %08x\n",ti[0]);
  printf("    TI Link  Status = %08x\n",ti[1]);
  printf("    TI Status       = %08x\n",ti[2]);
  printf("    TI (EB Status)  = %08x\n",ti[3]);
  printf("\n");

  printf("    ROC_EB   Ctrl   = %08x\n",eb_ctrl);
  printf("    ROC_EB   Status = %08x\n",eb_status);
  printf("    ROC_EB   Bank0  = %08x\n",evioBank[0]);
  printf("    ROC_EB   Bank1  = %08x\n",evioBank[1]);
  printf("    ROC_EB   Bank2  = %08x\n",evioBank[2]);
  printf("    ROC_EB   P_PORT = ");
  for(ii=0;ii<16;ii++) {
    printf(" %d ",slot[ii]);
  }
  printf("\n    ROC_EB   STATE  = ");
  for(ii=0;ii<8;ii++) {
    printf(" 0x%08x ",ppState[ii]);
  }
  printf("\n                      ");
  for(ii=8;ii<16;ii++) {
    printf(" 0x%08x ",ppState[ii]);
  }
  printf("\n\n");

  printf("    ROC             Ctrl   = %08x\n",roc[0]);
  printf("    ROC             State  = %d  %02x %02x %02x %02x\n",
	 (roc[1]&0x10000)>>16,
	 (roc[1]&0xF000)>>12,
	 (roc[1]&0xF00)>>8,
	 (roc[1]&0xF0)>>4,
	 (roc[1]&0xF));
  printf("    ROC SyncEvt     Status = %08x\n",roc[2]);
  printf("    ROC SynEvtLen   Status = %08x\n",roc[3]);
  printf("    ROC AsyncEvt    Status = %08x\n",roc[4]);
  printf("    ROC AsyncEvtLen Status = %08x\n",roc[5]);

  printf("\n");

  return OK;
}



int
vtpRocReset(int en_mask)
{
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  vtp->roc.Ctrl = 1; /* Enable Reset */

  if(en_mask)
    vtp->roc.Ctrl = (en_mask&0x700);
  else
    vtp->roc.Ctrl = 0;


  return OK;
}

int
vtpRocTCPInit(int mask, int ip0, int ip1, int ip2, int ip3, int dst_port)
{

  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;
  vtp->clk.Ctrl = 0x7;
  vtp->clk.Ctrl = 0x6;
  vtp->clk.Ctrl = 0x0;
  usleep(10000);


  vtp->tcpClient[0].Ctrl = 0x03C5;
  vtp->tcpClient[0].IP4_StateRequest = 0;

  /* Program network parameters */
  vtp->tcpClient[0].IP4_Addr = (129<<24) | (57<<16) | (109<<8) | (28<<0);
  vtp->tcpClient[0].IP4_SubnetMask = 0xFFFFFF00;
  vtp->tcpClient[0].IP4_GatewayAddr = (129<<24) | (57<<16) | (109<<8) | (1<<0);
  vtp->tcpClient[0].MAC_ADDR[1] = 0x0000CEBA;
  vtp->tcpClient[0].MAC_ADDR[0] = 0xF00300DA;
  vtp->tcpClient[0].TCP_DEST_ADDR[1] = (ip0<<24) | (ip1<<16) | (ip2<<8) | (ip3<<0);
  vtp->tcpClient[0].TCP_PORT[0] = (10001<<16) | (dst_port<<0);

  vtp->tcpClient[0].Ctrl = 0x03C5;
  vtp->tcpClient[0].Ctrl = 0x03C0;
  usleep(10000);
  vtp->tcpClient[0].Ctrl = 0x13C0;


  sleep(1);

  /* Connect to Server */
  vtp->tcpClient[0].IP4_StateRequest = 2;

  VUNLOCK;

  return OK;
}

int
vtpRocEnd()
{
  CHECKINIT;
  //CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  VLOCK;
  vtp->tcpClient[0].IP4_StateRequest = 0;
  VUNLOCK;

  return OK;
}

int
vtpRocSetID(int roc_id)
{
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;

  vtp->roc.rocID = roc_id;

  VUNLOCK;


  return roc_id;
}

unsigned int
vtpRocGetTrigCnt()
{
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  return(vtp->roc.TiTriggerCnt);

}


int
vtpRocEnable(int en_mask)
{
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;

  vtp->roc.Ctrl = ((en_mask&7)<<8);

  VUNLOCK;


  return OK;
}



int
vtpRocGetCfg(int *roc_id, int *en_mask)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;

  val = vtp->roc.rocID;
  *roc_id    = (val & 0xFF);
  val = vtp->roc.Ctrl;
  *en_mask = (val>>8)&7;

  VUNLOCK;

  return OK;
}

int
vtpRocSetTcpCfg(
    unsigned char ipaddr[4],
    unsigned char subnet[4],
    unsigned char gateway[4],
    unsigned char mac[6],
    unsigned char destipaddr[4],
    unsigned short destipport
  )
{
  int inst=0, link=0;
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  VLOCK;
  vtp->tcpClient[inst].IP4_StateRequest = 0;
  vtp->tcpClient[inst].IP4_Addr         = (    ipaddr[0]<<24) | (    ipaddr[1]<<16) | (    ipaddr[2]<<8) | (    ipaddr[3]<<0);
  vtp->tcpClient[inst].IP4_SubnetMask   = (    subnet[0]<<24) | (    subnet[1]<<16) | (    subnet[2]<<8) | (    subnet[3]<<0);
  vtp->tcpClient[inst].IP4_GatewayAddr  = (   gateway[0]<<24) | (   gateway[1]<<16) | (   gateway[2]<<8) | (   gateway[3]<<0);
  vtp->tcpClient[inst].MAC_ADDR[1]      =                                             (       mac[0]<<8) | (       mac[1]<<0);
  vtp->tcpClient[inst].MAC_ADDR[0]      = (       mac[2]<<24) | (       mac[3]<<16) | (       mac[4]<<8) | (       mac[5]<<0);
  vtp->tcpClient[inst].TCP_DEST_ADDR[link] = (destipaddr[0]<<24) | (destipaddr[1]<<16) | (destipaddr[2]<<8) | (destipaddr[3]<<0);
  printf("%s: TCP_DEST_ADDR = 0x%08X (link=%d)\n", __func__, vtp->tcpClient[inst].TCP_DEST_ADDR[link], link);
  vtp->tcpClient[inst].TCP_PORT[link]      = (        10001<<16) | (destipport<<0);
  VUNLOCK;

  return OK;
}

int
vtpRocGetTcpCfg(
    unsigned char ipaddr[4],
    unsigned char subnet[4],
    unsigned char gateway[4],
    unsigned char mac[6],
    unsigned char destipaddr[4],
    unsigned short *destipport
  )
{
  unsigned int val, inst=0, link=0;
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);


  VLOCK;
  val = vtp->tcpClient[inst].IP4_Addr;
  ipaddr[0] = ((val>>24)&0xFF); ipaddr[1] = ((val>>16)&0xFF); ipaddr[2] = ((val>>8)&0xFF); ipaddr[3] = ((val>>0)&0xFF);

  val = vtp->tcpClient[inst].IP4_SubnetMask;
  subnet[0] = ((val>>24)&0xFF); subnet[1] = ((val>>16)&0xFF); subnet[2] = ((val>>8)&0xFF); subnet[3] = ((val>>0)&0xFF);

  val = vtp->tcpClient[inst].IP4_GatewayAddr;
  gateway[0] = ((val>>24)&0xFF); gateway[1] = ((val>>16)&0xFF); gateway[2] = ((val>>8)&0xFF); gateway[3] = ((val>>0)&0xFF);

  val = vtp->tcpClient[inst].MAC_ADDR[1];
  mac[0] = ((val>>8)&0xFF); mac[1] = ((val>>0)&0xFF);

  val = vtp->tcpClient[inst].MAC_ADDR[0];
  mac[2] = ((val>>24)&0xFF); mac[3] = ((val>>16)&0xFF); mac[4] = ((val>>8)&0xFF); mac[5] = ((val>>0)&0xFF);

  val = vtp->tcpClient[inst].TCP_DEST_ADDR[link];
  destipaddr[0] = ((val>>24)&0xFF); destipaddr[1] = ((val>>16)&0xFF); destipaddr[2] = ((val>>8)&0xFF); destipaddr[3] = ((val>>0)&0xFF);

  printf("%s: TCP_DEST_ADDR = 0x%08X (link=%d)\n", __func__, vtp->tcpClient[inst].TCP_DEST_ADDR[link],link);

  val = vtp->tcpClient[inst].TCP_PORT[link];
  *destipport = ((val>>0)&0xFFFF);
  VUNLOCK;

  return OK;
}


int
vtpRocEvioWriteControl(unsigned int type, unsigned int val0, unsigned int val1)
{

  unsigned int rocid=0;

  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);


  VLOCK;

  rocid = vtp->roc.rocID;

  /* cMsg Header */
  if(type == 0xffd4)
    vtp->roc.CpuAsyncEventData = 3;
  else
    vtp->roc.CpuAsyncEventData = 1;
  vtp->roc.CpuAsyncEventData = (13<<2);

  /* EVIO Block header */
  vtp->roc.CpuAsyncEventData = 13;
  vtp->roc.CpuAsyncEventData = 0xffffffff;
  vtp->roc.CpuAsyncEventData = 8;
  vtp->roc.CpuAsyncEventData = 1;
  vtp->roc.CpuAsyncEventData = rocid;
  vtp->roc.CpuAsyncEventData = (0x1400|0x200|4); /* Control Event, Last block, evio version */
  vtp->roc.CpuAsyncEventData = 0;
  vtp->roc.CpuAsyncEventData = 0xc0da0100;

  /* CODA Control Event */
  vtp->roc.CpuAsyncEventData = 4;
  vtp->roc.CpuAsyncEventData = ((type<<16)|(1<<8)|(0));
  vtp->roc.CpuAsyncEventData = 1200;
  vtp->roc.CpuAsyncEventData = val0;
  vtp->roc.CpuAsyncEventData = val1;

  vtp->roc.CpuAsyncEventLen = 15;
  VUNLOCK;

  return OK;
}

int
vtpRocEbReset()
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  VLOCK;
  vtp->v7.rocEB.Ctrl = 1;
  //  vtp->v7.rocEB.Ctrl = 0;
  VUNLOCK;

  return OK;
}

int
vtpRocEbStart()
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  VLOCK;
  vtp->v7.rocEB.Ctrl = 2;
  VUNLOCK;

  return OK;
}

int
vtpRocEbStop()
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  VLOCK;
  vtp->v7.rocEB.Ctrl = 1;
  VUNLOCK;

  return OK;
}

int
vtpRocEbConfig(unsigned int bank0, unsigned int bank1, unsigned int bank2, int slot_mask)
{
  int ii;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  VLOCK;
  vtp->v7.rocEB.evio_cfg[0] = bank0&0xffffff;
  vtp->v7.rocEB.evio_cfg[1] = bank1&0xffffff;
  vtp->v7.rocEB.evio_cfg[2] = bank2&0xffffff;

  /* Setup default Slot configuration - all available modules build to bank0) */
  for(ii=0;ii<16;ii++) {
    if(slot_mask&(1<<ii)) vtp->v7.rocEB.pp_cfg[ii] = 1;
  }


  VUNLOCK;

  return OK;
}


int
vtpRocMigReset()
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  printf("%s()\n", __func__);

  // V7 Mig
  VLOCK;
  vtp->v7.mig[0].Ctrl = 0x1;              // Assert SYS_RST
  usleep(10000);
  vtp->v7.mig[0].Ctrl = 0x2;                 // Assert FIFO_RST
  usleep(10000);
  vtp->v7.mig[0].Ctrl = 0x0;
  usleep(10000);
  VUNLOCK;

  return OK;
}


int
vtpRocQsfpReset(int inst, int reset)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  if(inst<0 || inst>1)
  {
    printf("%s: ERROR inst=%d invalid.\n", __func__, inst);
    return ERROR;
  }

  VLOCK;
  val = vtp->tcpClient[inst].Ctrl;
  if(reset) val &= 0xFFFFEFFF;
  else      val |= 0x00001000;
  VUNLOCK;

  return 0;
}


#define TCP_SKIP_EN   0x0

int
vtpRocSkipTcp(int inst, int skip)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  if(inst<0 || inst>1)
  {
    printf("%s: ERROR inst=%d invalid.\n", __func__, inst);
    return ERROR;
  }

  VLOCK;
  if(skip) vtp->ebiorx[inst].Ctrl |= 0x00000200;
  else     vtp->ebiorx[inst].Ctrl &= 0xFFFFFDFF;
  VUNLOCK;

  return OK;
}


int
vtpRocEbioReset()
{
  CHECKINIT;

  int j;

  vtp->v7.ebioTx[0].Ctrl = 0x7; // Assert: RESET, TRAINING, FIFO_RESET
  vtp->v7.ebioTx[0].Ctrl = 0x6; // Release RESET, ebioTx: TRAINING,FIFO_RESET

  vtp->ebiorx[0].Ctrl = 0xBA | TCP_SKIP_EN;
  usleep(10000);
  vtp->ebiorx[0].Ctrl = 0xB8 | TCP_SKIP_EN;
  usleep(10000);

  for(j=0;j<10;j++)
    {
      vtp->ebiorx[0].Ctrl = 0xBA | TCP_SKIP_EN;
      vtp->ebiorx[0].Ctrl = 0xB8 | TCP_SKIP_EN;
      usleep(1000);
      if(!(vtp->ebiorx[0].Status & 0xFFFF0000))
        break;
      vtp->ebiorx[0].Ctrl = 0xBE | TCP_SKIP_EN;
    }
  vtp->ebiorx[0].Ctrl = 0xB8 | TCP_SKIP_EN;
  usleep(1000);
  vtp->ebiorx[0].Ctrl = 0x38 | TCP_SKIP_EN;


  if(j != 10) printf("EBIORX(0)     sync'd\n");
  else        printf("EBIORX(0) NOT sync'd\n");

    // V7 evioTx reset release
  vtp->v7.ebioTx[0].Ctrl = 0x4;

    // Release resets downstream->upstream
#if 1
    vtp->ebiorx[0].Ctrl = 0x78 | TCP_SKIP_EN;
#endif
    vtp->v7.ebioTx[0].Ctrl = 0x0;
    vtp->v7.ebioTx[0].Ctrl = 0x8;

  return OK;
}




int
vtpRocTcpConnect(int connect, unsigned int *cdata, int dlen)
{
  int jj, inst=0;


  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_VCODAROC,0);

  printf("%s(%d,%d)\n", __func__, inst, connect);


  VLOCK;
  if(connect)
  {
    vtp->tcpClient[inst].IP4_StateRequest = 0;    // tcp: disconnect socket
    vtp->tcpClient[inst].Ctrl = 0x03C5;           // tcp: reset: phy, qsfp, tcp
    usleep(10000);


    // Z7 socket connect
    vtp->tcpClient[inst].Ctrl = 0x03C0;           // tcp: reset: qsfp
    usleep(10000);
    vtp->tcpClient[inst].Ctrl = 0x13C0;           // tcp: reset: none
    usleep(250000);
    vtp->tcpClient[inst].IP4_StateRequest = 1;    // tcp: connect socket
    usleep(250000);

    /* Make sure we are connected before we send any data
    {
      volatile unsigned int done=0;
      int wait=1000000;
      while(!done) {
	done = (vtp->tcpClient[inst].IP4_TCPStatus)&0xff;
	wait--;
	printf("done = %d\n",done);
	if(wait==0) break;
      }

      }*/


    /* Send optional Data required to complete connection to the CODA EMU (EB) */
    if((cdata!=0)&&(dlen!=0)) {
      printf("%s: Sending connection info (%d words)\n",__func__,dlen);
      for(jj=0; jj<dlen; jj++) {
	printf(" 0x%08x ",cdata[jj]);
	vtp->roc.CpuAsyncEventData = cdata[jj];
      }
      printf("\n");
      vtp->roc.CpuAsyncEventLen = dlen;
    }

    /*
    vtp->roc.CpuAsyncEventData = 0x634d736;   // cMsg
    vtp->roc.CpuAsyncEventData = 0x20697320;  //  is
    vtp->roc.CpuAsyncEventData = 0x636f6f63;  // cool
    vtp->roc.CpuAsyncEventData = cMsg_Vers;
    vtp->roc.CpuAsyncEventData = maxBufSize;
    vtp->roc.CpuAsyncEventData = 1;           // Total # of sockets
    vtp->roc.CpuAsyncEventData = 1;           // socket #

    vtp->roc.CpuAsyncEventLen = 7;
    */


  }
  else  /* disconnect the socket */
  {
    /*Before disconnecting the socket we should make sure all data has been sent by checking the
      TCP buffer full bit in the ROC State register
    int max=10000, tcpfull=VTP_ROC_STATE_TCPFULL;
    while(tcpfull) {
      max--;
      tcpfull = (vtp->roc.State)&VTP_ROC_STATE_TCPFULL;
      if(max==0) break;
    }
    */
    vtp->tcpClient[inst].IP4_StateRequest = 0;
    vtp->tcpClient[inst].Ctrl = 0x03C5;           // tcp: reset: phy, qsfp, tcp
  }
  VUNLOCK;


  return OK;
}

int
vtpRocTcpGo()
{
  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);


  /* Nothing to do here yet */


  return OK;
}

/* If status comes back non-zero then the TCP link is up */
int
vtpRocTcpConnected()
{
  int status=0;

  CHECKINIT;
  CHECKTYPE(ZYNC_FW_TYPE_ZCODAROC,1);

  status = (vtp->tcpClient[0].IP4_TCPStatus)&0xff;

  return status;

}
