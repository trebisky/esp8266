#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

void i2c_master_gpio_init(void);
void i2c_master_init(void);

void i2c_master_stop(void);
void i2c_master_start(void);
void i2c_master_setAck(int level);
int i2c_master_getAck(void);
int i2c_master_readByte(void);
void i2c_master_writeByte(int wrdata);

#endif
