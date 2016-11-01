#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "vtpLib.h"

#define LTM4676_CMD_PAGE		0x00
#define LTM4676_CMD_READ_VOUT_MODE	0x20
#define LTM4676_CMD_READ_VIN		0x88
#define LTM4676_CMD_READ_VOUT		0x8B
#define LTM4676_CMD_READ_IIN		0x89
#define LTM4676_CMD_READ_IIN_CH	        0xED
#define LTM4676_CMD_READ_IOUT		0x8C
#define LTM4676_CMD_READ_TEMP1	        0x8D
#define LTM4676_CMD_READ_TEMP2	        0x8E
#define LTM4676_CMD_READ_POUT		0x96
#define LTM4676_CMD_READ_MFG		0x99
#define LTM4676_CMD_READ_MODEL	        0x9A

#define LTM4676_1_I2C_SLAVEADDR	0x40
#define LTM4676_2_I2C_SLAVEADDR	0x4D
#define LTM4676_3_I2C_SLAVEADDR	0x4E
#define LTM4676_4_I2C_SLAVEADDR	0x4F

unsigned char LTM4676_ADDR[] = {
  LTM4676_1_I2C_SLAVEADDR,	// LTM4676A: Ch0:     +1.2V (MGT_AVTT), Ch1: +2.2V
  LTM4676_2_I2C_SLAVEADDR,	// LTM4676A: Ch0+Ch1: +1.0V (MGT_AVCC)
  LTM4676_3_I2C_SLAVEADDR,	// LTM4676A: Ch0+Ch1: +1.0V (VCCINT)
  LTM4676_4_I2C_SLAVEADDR		// LTM4676A: Ch0:     +3.3V,            Ch1: +1.5V (VDD_DDR)
};

const char *rail[] = {
  "+1.20V(MGT_AVTT)",
  "+2.20V          ",
  "+1.05V(MGT_AVCC)",
  "+1.05V(MGT_AVCC)",
  "+1.00V(VCCINT)  ",
  "+1.00V(VCCINT)  ",
  "+3.30V          ",
  "+1.50V(VDD_DDR) "
};

void exit_error(const char *func, int retval)
{
  printf("  from %s errno = %d\n",
	 func, retval);
}

void ltm4676_select_slave(int slaveAddr)
{
  if(vtpI2CSelectSlave((uint8_t)slaveAddr) < 0)
    exit_error(__func__, 1);
}

void ltm4676_set_page(char page)
{
  if(vtpI2CWrite8(LTM4676_CMD_PAGE, page) < 0)
    {
      exit_error(__func__, page);
    }
}

unsigned short ltm4676_read_word(unsigned char slaveAddr, int page, unsigned char cmd)
{
  int32_t rval;

  vtpI2CSelectSlave(slaveAddr);
  
  if(page >= 0)
    ltm4676_set_page(page);

  if((rval = vtpI2CRead16(cmd)) < 0)
    exit_error(__func__, 1);	

  return (rval & 0xFFFF);
}

void ltm4676_read_block(unsigned char slaveAddr, int page, unsigned char cmd, unsigned char *buf)
{
  int32_t rval;

  vtpI2CSelectSlave(slaveAddr);

  if(page >= 0)
    ltm4676_set_page(page);

  if((rval = vtpI2CReadBlock(cmd, buf)) < 0)
    exit_error(__func__, 1);	
}

float get_L11(unsigned short v)
{
  int N, Y;
  float result;

  if(v & 0x8000)	N = ((v>>11) & 0x1F) | 0xFFFFFFE0;
  else			N = ((v>>11) & 0x1F);

  if(v & 0x0400)	Y = ((v>>0) & 0x7FF) | 0xFFFFF800;
  else			Y = ((v>>0) & 0x7FF);

  result = (float)Y * powf(2.0, (float)N);

  return result;
}

float get_L16(unsigned short v)
{
  float result;

  result = (float)v / 4096.0;

  return result;
}

float get_vin(int chip)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], -1, LTM4676_CMD_READ_VIN);
  return get_L11(v);
}

float get_vout_ch(int chip, int channel)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], channel, LTM4676_CMD_READ_VOUT);
  return get_L16(v);
}

float get_iin(int chip)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], -1, LTM4676_CMD_READ_IIN);
  return get_L11(v);
}

float get_iin_ch(int chip, int channel)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], channel, LTM4676_CMD_READ_IIN_CH);
  return get_L11(v);
}

float get_iout_ch(int chip, int channel)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], channel, LTM4676_CMD_READ_IOUT);
  return get_L11(v);
}

float get_temp(int chip)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], -1, LTM4676_CMD_READ_TEMP2);
  return get_L11(v);
}

float get_temp_ch(int chip, int channel)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], channel, LTM4676_CMD_READ_TEMP1);
  return get_L11(v);
}

float get_power_ch(int chip, int channel)
{
  unsigned short v = ltm4676_read_word(LTM4676_ADDR[chip], channel, LTM4676_CMD_READ_POUT);
  return get_L11(v);
}

char *get_mfg_id(int chip)
{
  unsigned char buf[32];
  static char id[11];

  memset(buf, 0, sizeof(buf));
  memset(id, 0, sizeof(id));
  ltm4676_read_block(LTM4676_ADDR[chip], -1, LTM4676_CMD_READ_MFG, buf);
  /* if(buf[0] < sizeof(id)-1) */
    {
      memcpy(id, buf, sizeof(id));
      return &id[0];
    }

  return NULL;
}

char *get_part_id(int chip)
{
  unsigned char buf[32];
  static char id[11];

  memset(buf, 0, sizeof(buf));
  memset(id, 0, sizeof(id));
  ltm4676_read_block(LTM4676_ADDR[chip], -1, LTM4676_CMD_READ_MODEL, buf);
  /* if(buf[0] < sizeof(id)-1) */
    {
      memcpy(id, buf, sizeof(id));
      return &id[0];
    }
  return NULL;
}

void ltm4676_print_status()
{
  int i;

  for(i = 0; i < 4; i++)
    {
      printf("LTM4676A %d, SLAVE_ADDR %02X\n", i, LTM4676_ADDR[i]);
      printf("   Mfg Id = %s, Part Id = %s\n", get_mfg_id(i), get_part_id(i));
      printf("   VIN = %5.3fV, IIN = %5.3fA, CTRL TEMP = %5.3fC\n", get_vin(i), get_iin(i), get_temp(i));
      printf("      %s: VOUT = %5.3fV, IOUT = %5.3fA, TEMP = %5.3fC, IIN = %5.3fA, POWER = %5.3fW\n", rail[2*i+0],
	     get_vout_ch(i,0), get_iout_ch(i,0), get_temp_ch(i,0), get_iin_ch(i,0), get_power_ch(i,0));
      printf("      %s: VOUT = %5.3fV, IOUT = %5.3fA, TEMP = %5.3fC, IIN = %5.3fA, POWER = %5.3fW\n",rail[2*i+1],
	     get_vout_ch(i,1), get_iout_ch(i,1), get_temp_ch(i,1), get_iin_ch(i,1), get_power_ch(i,1));
    }
}

int main()
{
  if(vtpCheckAddresses() == ERROR)
    exit(-1);

  if(vtpOpen(VTP_I2C_OPEN) == ERROR)
    {
      printf("vtpOpen not OK\n");
      goto CLOSE;
    }

  ltm4676_print_status();

 CLOSE:
  vtpClose(VTP_I2C_OPEN);


  exit(0);
}
