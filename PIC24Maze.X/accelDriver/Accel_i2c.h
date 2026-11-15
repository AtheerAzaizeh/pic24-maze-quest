#ifndef ACCEL_I2C_H
#define ACCEL_I2C_H
/*
 * File:   Accel_i2c.h
 * Author: Amit
 *
 * Created on April 21, 2022
 */

#include "../i2cDriver/i2c1_driver.h"

typedef enum {OK, NACK, ACK, BAD_ADDR, BAD_REG} I2Cerror;

void i2c1_open(void);
I2Cerror i2cReadSlaveRegister(unsigned char devAddW, unsigned char regAdd, unsigned char *reg);
I2Cerror i2cWriteSlave(unsigned char devAddW, unsigned char regAdd, unsigned char data);
I2Cerror i2cReadSlaveMultRegister(unsigned char devAddW, unsigned char regAdd, int len, unsigned char *buf);

#endif