/*
 * File:
 *    shmTest.c
 *
 * Description:
 *    Test of shared memory resources in VTP library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtpLib.h"
#include "vtpConfig.h"

extern VTP_CONF *vtpShmGetVTP_CONF();

int
main(int argc, char *argv[])
{
  VTP_CONF *conf;
  int stat=0, randy = 0;

  vtpCreateLockShm();

  vtpCheckMutexHealth(1);

  conf = vtpShmGetVTP_CONF();

  vtpLock();

  /* Only do reads and writes within the lock */
  printf("conf->hps.hodoscope.hit_dt = 0x%x (%d)\n",
	 conf->hps.hodoscope.hit_dt, conf->hps.hodoscope.hit_dt);

  /* Set the seed to the address of something in the stack (should change after each exec) */
  srandom((unsigned int)&randy);
  randy = random();

  printf(" .....            setting to 0x%x (%d)\n", randy, randy);
  conf->hps.hodoscope.hit_dt = randy;
  vtpUnlock();

  vtpKillLockShm(0);

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k -B shmTest"
  End:
*/
