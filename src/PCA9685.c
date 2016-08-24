#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "PCA9685.h"

//#define DEBUG



/////////////////////////////////////////////////////////////////////
// open the I2C bus device and assign the default slave address 
int PCA9685_openI2C(int adapterNum, unsigned char addr) {
  int fd;
  int ret;

  // build the I2C bus device filename 
  char filename[20];
  sprintf(filename, "/dev/i2c-%d", adapterNum);

  // open the I2C bus device 
  fd = open(filename, O_RDWR);
  if (fd < 0) {
    printf("PCA9685_openI2C(): open() returned %d for %s\n", fd, filename);
    return fd;
  } // if 

  #ifdef DEBUG
  printf("PCA9685_openI2C(): opened %s as fd %d\n", filename, fd);
  #endif

  // set the default slave address for read() and write() 
  ret = ioctl(fd, I2C_SLAVE, addr);
  if (ret < 0) {
    printf("PCA9685_openI2C(): ioctl() returned %d for addr %d\n", ret, addr);
    return ret;
  } // if 

  return fd;
} // PCA9685_openI2C 



/////////////////////////////////////////////////////////////////////
// initialize a PCA9685 device to defaults, turn off PWM's, and set the freq 
int PCA9685_initPWM(int fd, unsigned char addr, float freq) {
  int ret;

  // send a software reset to get defaults 
  unsigned char resetval = _PCA9685_RESETVAL;
  unsigned char mode1val = 0x00 | _PCA9685_AUTOINCBIT;

  ret = _PCA9685_writeI2CRaw(fd, _PCA9685_MODE1REG, 1, &resetval);
  if (ret != 0) {
    printf("PCA9685_initPWM(): _PCA9685_writeI2CRaw() returned %d\n", ret);
    return ret;
  } // if 

  // after the reset, all of the control registers default vals are ok 

  // turn all PWM's off 
  ret = PCA9685_setAllPWM(fd, addr, 0x00, 0x00);
  if (ret != 0) {
    printf("PCA9685_initPWM(): PCA9685_setAllPWM() returned %d\n", ret);
    return ret;
  } // if 

  // set the oscillator frequency 
  ret = _PCA9685_setPWMFreq(fd, addr, freq);
  if (ret != 0) {
    printf("PCA9685_initPWM(): _PCA9685_setPWMFreq() returned %d\n", ret);
    return ret;
  } // if 

  ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_MODE1REG, 1, &mode1val);
  if (ret != 0) {
    printf("PCA9685_initPWM(): _PCA9685_writeI2CReg() returned ");
    printf("%d on addr %02x\n", ret, addr);
    return ret;
  } // if 

  return 0;
} // PCA9685_initPWM



/////////////////////////////////////////////////////////////////////
// set all PWM vals in one transaction 
// FIXME
// remove len parameter and use _PCA9685_CHANS constant 
int PCA9685_setPWMVals(int fd, unsigned char addr, int* vals) {
  unsigned char regVals[_PCA9685_CHANS*4];
  int ret;

  #ifdef DEBUG
  { int i;
    // report the write 
    printf("PCA9685_setPWMVals(): vals[%d]: ", _PCA9685_CHANS);
    for (i=0; i<_PCA9685_CHANS; i++) {
      printf(" %03x", vals[i]);
    } // for 
    printf("\n");
  }
  #endif
  
  { int i;
    for (i=0; i<_PCA9685_CHANS; i++) {
      regVals[i*4] = 0x00;
      regVals[i*4+1] = 0x00;
      regVals[i*4+2] = vals[i] & 0xFF;
      regVals[i*4+3] = vals[i] >> 8;
    } // for 
    ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_BASEPWMREG,
                               _PCA9685_CHANS*4, regVals);
    if (ret != 0) {
      printf("PCA9685_setPWMVals(): _PCA9685_writeI2CReg() returned %d, ", ret);
      printf("addr %02x, reg %02x, len %d\n",
              addr, _PCA9685_BASEPWMREG, _PCA9685_CHANS*4);
      return ret;
    } // if 
  } // int context 
  return 0;
} // PCA9685_setPWMVals



/////////////////////////////////////////////////////////////////////
// set a PWM channel (4 bytes) with the low 12 bits from a 16-bit value 
int PCA9685_setPWMVal(int fd, unsigned char addr, char reg, int on, int off) {
  unsigned char val;
  int ret;

  #ifdef DEBUG
  printf("PCA9685_setPWMVal(): reg %02x, on %02x, off %02x\n", reg, on, off);
  #endif

  // ON_L 
  val = on & 0xFF; // mask all bits above 8 
  ret = _PCA9685_writeI2CReg(fd, addr, reg, 1, &val);
  if (ret != 0) {
    printf("PCA9685_setPWMVal(): _PCA9685_writeI2CReg() returned ");
    printf("%d on addr %02x reg %02x val %02x\n", ret, addr, reg, val);
    return ret;
  } // if 

  // ON_H 
  val = on >> 8; // fetch all bits above 8 
  ret = _PCA9685_writeI2CReg(fd, addr, reg+1, 1, &val);
  if (ret != 0) {
    printf("PCA9685_setPWMVal(): _PCA9685_writeI2CReg() returned ");
    printf("%d on addr %02x reg %02x val %02x\n", ret, addr, reg, val);
    return ret;
  } // if 

  // OFF_L 
  val = off & 0xFF; // mask all bits above 8 
  ret = _PCA9685_writeI2CReg(fd, addr, reg+2, 1, &val);
  if (ret != 0) {
    printf("PCA9685_setPWMVal(): _PCA9685_writeI2CReg() returned ");
    printf("%d on addr %02x reg %02x val %02x\n", ret, addr, reg, val);
    return ret;
  } // if 

  // OFF_H 
  val = off >> 8; // fetch all bits above 8 
  ret = _PCA9685_writeI2CReg(fd, addr, reg+3, 1, &val);
  if (ret != 0) {
    printf("PCA9685_setPWMVal(): _PCA9685_writeI2CReg() returned ");
    printf("%d on addr %02x reg %02x val %02x\n", ret, addr, reg, val);
    return ret;
  } // if 
  
  return 0;
} // PCA9685_setPWMVal 



/////////////////////////////////////////////////////////////////////
// set all PWM channels using the ALL_LED registers 
int PCA9685_setAllPWM(int fd, unsigned char addr, int on, int off) {
  int ret;

  // send the values to the ALL_LED registers 
  ret = PCA9685_setPWMVal(fd, addr, _PCA9685_ALLLEDREG, on, off);
  if (ret != 0) {
    printf("PCA9685_setAllPWM(): PCA9685_setPWMVal() returned %d\n", ret);
    return ret;
  } // if 

  return 0;
} // PCA9685_setAllPWM 



/////////////////////////////////////////////////////////////////////
// print out the values of all registers used in a PCA9685
int PCA9685_dumpAllRegs(int fd, unsigned char addr) {
  unsigned char loBuf[_PCA9685_LOREGS];
  unsigned char hiBuf[_PCA9685_HIREGS];
  int ret;

  // read all the low PCA9685 registers 
  ret = _PCA9685_readI2CReg(fd, addr, _PCA9685_FIRSTLOREG,
                            _PCA9685_LOREGS, loBuf);
  if (ret != 0) {
    printf("PCA9685_dumpAllRegs(): _PCA9685_readI2CReg() returned %d\n", ret);
    exit(ret);
  } // if 

  // display all of the low PCA9685 register values 
  _PCA9685_dumpLoRegs(loBuf);

  // read all the high PCA9685 registers 
  ret = _PCA9685_readI2CReg(fd, addr, _PCA9685_FIRSTHIREG,
                            _PCA9685_HIREGS, hiBuf);
  if (ret != 0) {
    printf("PCA9685_dumpAllRegs(): _PCA9685_readI2CReg() returned %d\n", ret);
    exit(ret);
  } // if 

  // display all of the high PCA9685 register values 
  _PCA9685_dumpHiRegs(hiBuf);

  return 0;
} // PCA9685_dumpAllRegs 
/////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////
// internal functions, may be used but usually not required:

/////////////////////////////////////////////////////////////////////
// set the PWM frequency 
int _PCA9685_setPWMFreq(int fd, unsigned char addr, float freq) {
  int ret;
  unsigned char mode1Val;
  int prescale;

  // get initial mode1Val 
  ret = _PCA9685_readI2CReg(fd, addr, _PCA9685_MODE1REG, 1, &mode1Val);
  if (ret != 0) {
    printf("_PCA9685_setPWMFreq(): _PCA9685_readI2CReg() returned %d\n", ret);
    return ret;
  } // if 

  // clear restart 
  mode1Val = mode1Val & ~_PCA9685_RESTARTBIT;
  // set sleep 
  mode1Val = mode1Val | _PCA9685_SLEEPBIT;

  ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_MODE1REG, 1, &mode1Val);
  if (ret != 0) {
    printf("_PCA9685_setPWMFreq(): _PCA9685_writeI2CReg() returned %d\n", ret);
    return ret;
  } // if 

  // freq must be 40Hz - 1000Hz 
  freq = (freq > _PCA9685_MAXFREQ
               ? _PCA9685_MAXFREQ
               : (freq < _PCA9685_MINFREQ
                       ? _PCA9685_MINFREQ
                       : freq));
  // calculate and set prescale 
  prescale = (int)(25000000.0f / (4096 * freq) - 0.5f);

  ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_PRESCALEREG, 1,
                            (unsigned char*)&prescale);
  if (ret != 0) {
    printf("_PCA9685_setPWMFreq(): _PCA9685_writeI2CReg() returned %d\n", ret);
    return ret;
  } // if 

  // wake 
  mode1Val = mode1Val & ~_PCA9685_SLEEPBIT;
  ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_MODE1REG, 1, &mode1Val);
  if (ret != 0) {
    printf("_PCA9685_setPWMFreq(): _PCA9685_writeI2CReg() returned %d\n", ret);
    return ret;
  } // if 

  // allow the oscillator to stabilize at least 500us 
  { struct timeval sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_usec = 1000;
    ret = select(0, NULL, NULL, NULL, &sleeptime);
    if (ret < 0) {
      printf("_PCA9685_setPWMFreq(): select() returned %d\n", ret);
      return ret;
    } // if 
  } // context 

  // restart 
  mode1Val = mode1Val | _PCA9685_RESTARTBIT;
  ret = _PCA9685_writeI2CReg(fd, addr, _PCA9685_MODE1REG, 1, &mode1Val);
  if (ret != 0) {
    printf("_PCA9685_setPWMFreq(): _PCA9685_writeI2CReg() returned %d\n", ret);
    return ret;
  } // if 

  return 0;
} // _PCA9685_setPWMFreq 



/////////////////////////////////////////////////////////////////////
// dump the contents of the first 70 registers (modes and PWMs) 
int _PCA9685_dumpLoRegs(unsigned char* buf) {
  int i;
  for (i=0; i<_PCA9685_LOREGS; i++) {
    if ((i-6)%16 == 0) { printf("\n"); }
    printf("%02x ", buf[i]);
  } // for 
  printf("\n");

  return 0;
} // _PCA9685_dumpLoRegs 



/////////////////////////////////////////////////////////////////////
// dump the contents of the last six registers 
int _PCA9685_dumpHiRegs(unsigned char* buf) {
  int i;
  for (i=0; i<_PCA9685_HIREGS; i++) {
    printf("%02x ", buf[i]);
  } // for 
  printf("\n");

  return 0;
} // _PCA9685_dumpHiRegs 



/////////////////////////////////////////////////////////////////////
// read characters from a register at an address 
int _PCA9685_readI2CReg(int fd, unsigned char addr, unsigned char startReg,
            int len, unsigned char* readBuf) {
  int ret;
  // will be using a two message transaction 
  struct i2c_rdwr_ioctl_data data;
  struct i2c_msg msgs[2];

  // first message writes the start register address 
  msgs[0].addr = addr;
  msgs[0].flags = 0x00;
  msgs[0].len = 1;
  msgs[0].buf = &startReg;

  // second message reads the register value(s) 
  msgs[1].addr = addr;
  msgs[1].flags = I2C_M_RD;
  msgs[1].len = len;
  msgs[1].buf = readBuf;

  data.msgs = msgs;
  data.nmsgs = 2;

  // send the combined transaction 
  ret = ioctl(fd, I2C_RDWR, &data);
  if (ret < 0) {
    printf("_PCA9685_readI2CReg(): ioctl() returned ");
    printf("%d on addr %02x start %02x\n", ret, addr, startReg);
    return ret;
  } // if 

  #ifdef DEBUG
  { int i;
    // report the read 
    printf("_PCA9685_readI2CReg(): %02x:%02x:%02x", addr, startReg, len);
    // report the result 
    for (i=0; i<len; i++) {
      printf(" %02x", readBuf[i]);
    } // for 
    printf("\n");
  }
  #endif
  
  return 0;
} // _PCA9685_readI2CReg 



/////////////////////////////////////////////////////////////////////
// write characters to a register at an address 
int _PCA9685_writeI2CReg(int fd, unsigned char addr, unsigned char startReg,
             int len, unsigned char* writeBuf) {
  int ret;

  // prepend the register address to the buffer 
  unsigned char* rawBuf;
  rawBuf = (unsigned char*)malloc(len+1);
  rawBuf[0] = startReg;
  memcpy(&rawBuf[1], writeBuf, len);

  // pass the new buffer to the raw writer 
  ret = _PCA9685_writeI2CRaw(fd, addr, len+1, rawBuf);
  if (ret != 0) {
    printf("_PCA9685_writeI2CReg(): _PCA9685_writeI2CRaw() returned ");
    printf("%d on addr %02x reg %02x\n", ret, addr, startReg);
    return ret;
  } // if 

  free(rawBuf);

  return 0;
} // _PCA9685_writeI2CReg 



/////////////////////////////////////////////////////////////////////
// write characters to an i2c address 
int _PCA9685_writeI2CRaw(int fd, unsigned char addr, int len,
                         unsigned char* writeBuf) {
  struct i2c_rdwr_ioctl_data data;
  struct i2c_msg msgs[1];
  int ret;

  #ifdef DEBUG
  { int i;
    // report the write 
    printf("_PCA9685_writeI2CRaw: %02x:", addr);
    for (i=0; i<len; i++) {
      printf(" %02x", writeBuf[i]);
    } // for 
    printf("\n");
  }
  #endif
  
  // one msg in the transaction 
  msgs[0].addr = addr;
  msgs[0].flags = 0x00;
  msgs[0].len = len;
  msgs[0].buf = writeBuf;

  data.msgs = msgs;
  data.nmsgs = 1;

  // send a combined transaction 
  ret = ioctl(fd, I2C_RDWR, &data);
  if (ret < 0) {
    int i;
    printf("_PCA9685_writeI2CRaw(): ioctl() returned ");
    printf("%d on addr %02x\n", ret, addr);
    printf("_PCA9685_writeI2CRaw(): len = %d, buf = ", len);
    for (i=0; i<len; i++) {
      printf("%02x ", writeBuf[i]);
    } // for 
    printf("\n");
    return ret;
  } // if 
  return 0;
} // _PCA9685_writeI2CRaw 