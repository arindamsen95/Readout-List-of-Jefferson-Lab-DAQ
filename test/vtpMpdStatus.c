/*
 * File:
 *    vtpMpdStatus.c
 *
 * Description:
 *    Show status of VTP and their attached MPDs
 *
 *
 *   This first looks at the connections between VTP <---> MPD.
 *
 *   If a channel is up (good fiber+serial connections), it will be
 *    added to the list to be initialized.
 *
 *   Final status shown is for the MPDs discovered just from the
 *    connection up status
 *
 * Usage:
 *      vtpMpdStatus
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtp.h"
#include "vtpLib.h"
#include "vtpConfig.h"
#include "mpdLib.h"

int main(int argc, char *argv[])
{
  int stat;
  char *rol_usrConfig = "/home/sbs-onl/vtp/cfg/sbsvtp3.config";

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpStatus(1);

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffff);

  vtpMpdDisable(0xffffffff);
  vtpMpdEnable(0xffffffff);

  vtpStatus(0);
  vtpMpdPrintStatus(0,0);
  vtpMpdPrintStatus(0,1);

  unsigned int chanmask = vtpMpdGetChanUpMask();
  mpdInit(chanmask, 0, 32, MPD_INIT_FIBER_MODE | MPD_INIT_NO_CONFIG_FILE_CHECK);

  mpdGStatus(1);

 CLOSE:
  vtpClose(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);

  return 0;

}


/*
  Local Variables:
  compile-command: "make -k -B vtpMpdStatus"
  End:
 */
