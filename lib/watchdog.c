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

static char WATCHDOG_DEVICE[261];

static int initialize_watchdog()
{
	struct dirent *de;
	DIR *dr = opendir("/sys/bus/platform/devices/adl-bmc-wdt/watchdog"); 

	if (dr == NULL)  // opendir returns NULL if couldn't open directory 
		return -1; 

	memset(WATCHDOG_DEVICE, 0, sizeof(WATCHDOG_DEVICE));
	while ((de = readdir(dr)) != NULL) {
		if(strncmp(de->d_name, "watchdog", strlen("watchdog")) == 0) {
			sprintf(WATCHDOG_DEVICE, "/dev/%s", de->d_name);
			closedir(dr);
			return 0;
		}
	}
	closedir(dr);     

	return -1;
}

#define WDOG_INIT() if(initialize_watchdog() < 0) return -1;

uint32_t EApiWDogGetCap(uint32_t *pMaxDelay, uint32_t *pMaxEventTimeout, uint32_t *pMaxResetTimeout)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	char sysfile[256];	
	char value[256] = { 0 };
        int tout = 0, ret; 

	WDOG_INIT();

        if(pMaxDelay==NULL && pMaxEventTimeout==NULL && pMaxResetTimeout==NULL){
                return EAPI_STATUS_INVALID_PARAMETER;
        }


	if (pMaxDelay)
	{
		*pMaxDelay = 0;
	}

	if (pMaxEventTimeout)
	{
		*pMaxEventTimeout = 0;
	}

	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-wdt/Capabilities/wdt_max_timeout");

	ret = read_sysfs_file(sysfile, value, sizeof(value)); 
	if (ret)
		return EAPI_STATUS_READ_ERROR;

	sscanf(value, "%d", &tout);
        *pMaxResetTimeout = tout;

        return status;
}

uint32_t EApiWDogStart(uint32_t Delay, uint32_t EventTimeout, uint32_t ResetTimeout)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	int fd, ret, i, tout;
	unsigned long flags;


	if(Delay>0){
                return EAPI_STATUS_INVALID_PARAMETER;
        }

        if(EventTimeout>0){
                return EAPI_STATUS_INVALID_PARAMETER;
        }

        if(ResetTimeout>UINT16_MAX){
                return EAPI_STATUS_INVALID_PARAMETER;
        }




	WDOG_INIT();

	char sysfile[256];	
	char value[256] = { 0 };
	char buf[20] = { 0 };
	for (i = 0; i < strlen(WATCHDOG_DEVICE) - 4; i++) 
	{
	        buf[i] = WATCHDOG_DEVICE[i+5];
	}
	sprintf(sysfile, "/sys/class/watchdog/%s/timeout", buf);

	ret = read_sysfs_file(sysfile, value, sizeof(value)); 
	if (ret)
		return EAPI_STATUS_READ_ERROR;

	tout = atoi(value);	
	if (tout)
		return EAPI_STATUS_RUNNING;


	fd = open(WATCHDOG_DEVICE, O_WRONLY);
	if (fd < 0) {
		return fd;
	}

	flags = ResetTimeout;

	ret = ioctl(fd, WDIOC_SETTIMEOUT, &flags);
	if (!ret){
		close(fd);
	}
	else {
		close(fd);
		return ret;
	}
	
        return status;
}

uint32_t EApiWDogTrigger(void)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	int fd, ret, i, tout = 0;

	WDOG_INIT();

	char sysfile[256];	
	char value[256] = { 0 };
	char buf[20] = { 0 };
	for (i = 0; i < strlen(WATCHDOG_DEVICE) - 4; i++) 
	{
	        buf[i] = WATCHDOG_DEVICE[i+5];
	}
	sprintf(sysfile, "/sys/class/watchdog/%s/timeout", buf);

	ret = read_sysfs_file(sysfile, value, sizeof(value)); 
	if (ret)
		return EAPI_STATUS_READ_ERROR;

	tout = atoi(value);	
	if (tout == 0)
		return EAPI_STATUS_ERROR;

	fd = open(WATCHDOG_DEVICE, O_WRONLY);
	if (fd < 0) {
		return fd;
	}

	ret = ioctl(fd, WDIOC_SETTIMEOUT, &tout);
	if (!ret)
		close(fd);
	else {
	        close(fd);	
		return ret;
	}

        return status;
}

uint32_t EApiWDogStop(void)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	int fd, ret;
	const char v = 'V';

	WDOG_INIT();

	fd = open(WATCHDOG_DEVICE, O_WRONLY);
	if (fd < 0) {
		return fd;
	}
	
	ret = write(fd, &v, 1);
	if (ret < 0)
		printf("Stopping watchdog ticks failed (%d)...\n", errno);

	close(fd);
        return status;
}

uint32_t EApiPwrUpWDogStart(uint32_t timeout)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	FILE *fp;
	char sysfile[256];
	char value[256];
	int ret;

	if (timeout > 65535 || timeout < 60) {
		errno = EINVAL;
		return -1;
	}

	WDOG_INIT();

	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-wdt/Capabilities/PwrUpWDog");
        fp = fopen(sysfile, "r+");
        if(fp == NULL)
                return -1;
        sprintf(value, "%u", timeout);
        ret = fwrite(value, 256, sizeof(char), fp);

	if (ret)
		fclose(fp);
	else {
		fclose(fp);
		return -1;
        }

        return status;
}

uint32_t EApiPwrUpWDogStop(void)
{
	uint32_t status = EAPI_STATUS_SUCCESS;
	FILE *fp;
	char sysfile[256];
	char value[256];
	uint32_t timeout= 0;
	int ret;

	WDOG_INIT();

	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-wdt/Capabilities/PwrUpWDog");
        fp = fopen(sysfile, "r+");
        if(fp == NULL)
               return -1;

	sprintf(value, "%u", timeout);

        ret = fwrite(value, 256, sizeof(char), fp);
        if (ret)
		fclose(fp);
	else {
		fclose(fp);
		return -1;
        }

        return status;

}
