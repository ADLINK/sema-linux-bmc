// Software License Agreement (BSD License)
//
// Copyright (c) 2022, ADLINK Technology, Inc
// All rights reserved.
//
// Redistribution and use of this software in source and binary forms,
// with or without modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Neither the name of ADLINK Technology nor the names of its contributors may be used
//   to endorse or promote products derived from this software without specific
//   prior written permission of ADLINK Technology, Inc.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <dirent.h>
#include <eapi.h>
#include <common.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int get_i2c_dev (char *i2c_dev)
{
	struct dirent *de;
	DIR *dr = opendir("/sys/class/i2c-adapter");

	if (dr == NULL)  // opendir returns NULL if couldn't open directory
		return -1;

	while ((de = readdir(dr)) != NULL)
	{
		if(strncmp(de->d_name, "i2c", strlen("i2c")) == 0)
		{
			int fd;
			char I2C_ADAPTER[512];
			sprintf(I2C_ADAPTER, "/sys/class/i2c-adapter/%s/name", de->d_name);

			if((fd = open(I2C_ADAPTER, O_RDONLY)) > 0)
			{
				if(read(fd, I2C_ADAPTER, sizeof(I2C_ADAPTER)) > 0)
				{
					if(strncmp(I2C_ADAPTER, "ADLINK BMC I2C adapter", strlen("ADLINK BMC I2C adapter")) == 0)
					{
						sprintf(i2c_dev, "/dev/%s", de->d_name);
						close(fd);
						closedir(dr);
						return 0;
					}
				}
				close(fd);
			}
		}
	}

	closedir(dr);
	return -ENODEV;
}

static inline int i2c_smbus_access(int file, char read_write, unsigned char command,
		int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

static inline int i2c_smbus_read_i2c_block_data(int file, unsigned char command,
		unsigned char length, unsigned char *values)
{
	union i2c_smbus_data data;
	int i;

	if (length > 32)
		length = 32;
	data.block[0] = length;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
				length == 32 ? I2C_SMBUS_I2C_BLOCK_BROKEN :
				I2C_SMBUS_I2C_BLOCK_DATA,&data)) {
		return -1;
	}
	else {
		for (i = 1; i <= data.block[0]; i++)
			values[i-1] = data.block[i];
		return data.block[0];
	}
}

static inline int i2c_smbus_write_i2c_block_data(int file, unsigned char command,
		unsigned char length, const unsigned char *values)
{
	union i2c_smbus_data data;
	int i;
	if (length > 32)
		length = 32;
	for (i = 1; i <= length; i++)
		data.block[i] = values[i-1];
	data.block[0] = length;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
			I2C_SMBUS_I2C_BLOCK_BROKEN, &data);
}

static int open_i2c_dev (uint8_t address)
{
	int file, ret;
	char i2c_dev[64];

	if((ret = get_i2c_dev(i2c_dev)) < 0)
		return ret;

	if((file = open(i2c_dev, O_RDWR)) < 0)
		return file;

	if((ret = ioctl(file, I2C_SLAVE, address>>1)) < 0)
		return ret;

	return file;
}

#define MAX_BLOCK 29

uint32_t EApiI2CWriteReadRaw(uint32_t Id, uint8_t Addr, void *pWBuffer, uint32_t WriteBCnt, void *pRBuffer, uint32_t RBufLen, uint32_t  ReadBCnt)
{
	static int file = 0;
	static uint32_t sema_capability = 0;
	uint32_t status = EAPI_STATUS_SUCCESS;

	WriteBCnt = WriteBCnt?WriteBCnt-1:0;	// need to conform to specification
	ReadBCnt = ReadBCnt?ReadBCnt-1:0;		// need to conform to specification

	if(WriteBCnt > 1 && pWBuffer == NULL)
		return EAPI_STATUS_INVALID_PARAMETER;

	if(ReadBCnt > 1 && pRBuffer == NULL)
		return EAPI_STATUS_INVALID_PARAMETER;

	if(ReadBCnt > 1 && RBufLen == 0)
		return EAPI_STATUS_INVALID_PARAMETER;

	if(ReadBCnt>RBufLen)
		return EAPI_STATUS_MORE_DATA;

	if(WriteBCnt==0 && ReadBCnt == 0)
		return EAPI_STATUS_INVALID_PARAMETER;

	if(Id!= EAPI_ID_I2C_EXTERNAL && Id != EAPI_ID_I2C_LVDS_1)
	{
		static uint32_t id = 18;
		if(sema_capability == 0)
		{
			if(EApiBoardGetValue(id, &sema_capability) < 0)
				return EAPI_STATUS_UNSUPPORTED;
		}

		if( Id == (SEMA_EAPI_ID_I2C_EXTERNAL_2))		// I2C Bus 3
		{
			if (!(sema_capability & SEMA_C_I2C3))
				return EAPI_STATUS_UNSUPPORTED;
		}
		else if(Id == (SEMA_EAPI_ID_I2C_EXTERNAL_3))	// I2C Bus 4
		{
			if (!(sema_capability & SEMA_C_I2C4))
				return EAPI_STATUS_UNSUPPORTED;
		}
		else
		{
			return EAPI_STATUS_UNSUPPORTED;
		}
	}

	if(WriteBCnt > MAX_BLOCK+1)
		return EAPI_STATUS_INVALID_BLOCK_LENGTH;

	if(ReadBCnt > MAX_BLOCK+1)
		return EAPI_STATUS_INVALID_BLOCK_LENGTH;

	if(file < 0)
		return EAPI_STATUS_UNSUPPORTED;

	if(file == 0)
		if((file = open_i2c_dev(Addr)) < 0)
			return EAPI_STATUS_UNSUPPORTED;

	{
		int ret;

		if(WriteBCnt > 0)
		{
			unsigned char *buf = pWBuffer;
			unsigned char addr = buf[0];
			ret = i2c_smbus_write_i2c_block_data(file, addr, WriteBCnt - 1, &buf[1]);
			if(ret < 0)
				return EAPI_STATUS_WRITE_ERROR;
		}

		if(ReadBCnt > 0)
		{
			unsigned char *buf = pWBuffer;
			if(WriteBCnt > 0)
			{
				unsigned char addr = buf[0];
				ret = i2c_smbus_read_i2c_block_data(file, addr, ReadBCnt, pRBuffer);
				if(ret < 0)
					return EAPI_STATUS_READ_ERROR;
			}
		}
	}

	return status;
}
