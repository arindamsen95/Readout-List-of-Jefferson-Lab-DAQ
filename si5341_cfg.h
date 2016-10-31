#ifndef __SI5341_CFG_H
#define __SI5341_CFG_H

void si5341_softReset();
void si5341_hardReset();
void si5341_sync();
void si5341_selectClockSource(int src);
int si5341_configure(int src);
int si5341_Setup();
int si5341_Init();
int si5341_Test();

#endif
