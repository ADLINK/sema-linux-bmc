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
#include <errorcodes.h>
#include <string.h>
#include <errno.h>
#include <eapi.h>
#include <common.h>

uint32_t EApiLibInitialize(void)
{
	char res[128];
	char sysfile[128];
	int ret;
	
	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/board_name");
	ret = read_sysfs_file(sysfile, res, sizeof(res));
	if(ret < 0) {
		return ret;
	}	

	if (strlen(res) <= 1)
		return EAPI_STATUS_NOT_INITIALIZED;

	return EAPI_STATUS_SUCCESS;
}

uint32_t EApiLibUnInitialize(void)
{
	return EAPI_STATUS_SUCCESS;
}