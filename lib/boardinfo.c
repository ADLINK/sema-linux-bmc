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
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h> 
#include <string.h>
#include <eapi.h>
#include <common.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef tempconvert
#define KELVINS_OfFSET 2731
#endif

#define GET_VOLT_AND_DESC	_IOWR('a','1',struct data *)
#define GET_VOLT_MONITOR_CAP	_IOWR('a','2',uint8_t *)
#define MAX_ID		 16

int dev_handle;

struct data{
	int id;
	int volt;
	char volt_desc[256];
};

#define PLATFORMS_NUMBER 2

char *Board[PLATFORMS_NUMBER] = {
	"LEC-AL",
	"Q7-AL"
};

#ifdef tempconvert
static int encode_celcius(uint32_t temp)
{
	uint32_t tmp;
	tmp = temp / 1000;
         tmp = tmp * 10 + KELVINS_OfFSET;
        return tmp;
}
#endif

uint32_t IsFileExist(char *sysf)
{
	int fd;
	fd = open(sysf, O_RDONLY);
	if (fd < 1)
	{
		close(fd);
		return -1;
	}

	close(fd);
	return 0;

}

uint32_t EApiBoardGetStringA(uint32_t Id, char *pBuffer, uint32_t *pBufLen)
{
	char res[128];
	memset(res, 0, 128);
	char sysfile[128];
	int ret;

	uint32_t status = EAPI_STATUS_SUCCESS;

	switch (Id)
	{
		case EAPI_ID_BOARD_MANUFACTURER_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/manufacturer_name");
			break;
		case EAPI_ID_BOARD_NAME_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/board_name");
			break;
		case EAPI_ID_BOARD_SERIAL_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/serial_number");
			break;
		case EAPI_ID_BOARD_BIOS_REVISION_STR:
			sprintf(sysfile, "/sys/class/dmi/id/bios_version");
			break;
		case EAPI_SEMA_ID_BOARD_BOOT_VERSION_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/bmc_boot_version");
			break;
		case EAPI_SEMA_ID_BOARD_RESTART_EVENT_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/restart_event_str");
			break;
		case EAPI_ID_BOARD_HW_REVISION_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/hw_rev");
			break;
		case EAPI_SEMA_ID_BOARD_APPLICATION_VERSION_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/bmc_application_version");
			break;
		case EAPI_SEMA_ID_BOARD_REPAIR_DATE_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/last_repair_date");
			break;
		case EAPI_SEMA_ID_BOARD_MANUFACTURE_DATE_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/manufactured_date");
			break;
		case EAPI_SEMA_ID_BOARD_MAC_STRING:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/mac_address");
			break;
		case EAPI_SEMA_ID_BOARD_2ND_HW_REVISION_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/second_hw_rev");
			break;
		case EAPI_SEMA_ID_BOARD_2ND_SERIAL_STR:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/second_ser_num");
			break;
		case EAPI_ID_BOARD_PLATFORM_TYPE_STR:
            		sprintf(sysfile,"/sys/bus/platform/devices/adl-bmc-boardinfo/information/platform_id");
			break;
			
		default:
			status = EAPI_STATUS_UNSUPPORTED;
	}

	ret = read_sysfs_file(sysfile, pBuffer, *pBufLen);
	
	if (ret == 0)
		status = EAPI_STATUS_SUCCESS;

	if (strlen(pBuffer) == 0 || ret == -1){
		return EAPI_STATUS_UNSUPPORTED;
	}

	return status;
}

uint32_t EApiBoardGetValue(uint32_t Id, uint32_t *pValue)
{
	char res[128];
	memset(res, 0, 128);
	char sysfile[128] = {0};
	int ret;
	uint32_t status = EAPI_STATUS_SUCCESS;

	int hwmon_number;
	hwmon_number = get_hwmon_num();

	switch (Id)
	{	
		case EAPI_ID_GET_EAPI_SPEC_VERSION:
			*pValue = (EAPI_VERSION);
			break;
		case EAPI_ID_BOARD_BOOT_COUNTER_VAL:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/boot_counter_val");
			break;
		case EAPI_ID_BOARD_RUNNING_TIME_METER_VAL:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/total_up_time");
			break;
		case EAPI_ID_BOARD_LIB_VERSION_VAL:
			*pValue = EAPI_VER_CREATE(4,3,1);
			break;
		case EAPI_ID_HWMON_CPU_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/cpu_cur_temp",hwmon_number);
			ret = IsFileExist(sysfile);
			if (ret){
				sprintf(sysfile, "/sys/class/thermal/thermal_zone1/temp");
				ret = IsFileExist(sysfile);
				if (ret == 0){
					ret = read_sysfs_file(sysfile, res, sizeof(res));
					if (ret == 0)
					{
						*pValue = EAPI_ENCODE_CELCIUS(atoi(res)/1000);
						return ret;
					}
				}
			}
		
			break;
		case EAPI_ID_HWMON_SYSTEM_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys1_cur_temp",hwmon_number);
			break;
		case EAPI_ID_HWMON_VOLTAGE_VCORE:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_vcore");
			break;
		case EAPI_ID_HWMON_VOLTAGE_2V5:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_2v5");
			break;
		case EAPI_ID_HWMON_VOLTAGE_3V3:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_3v3");
			break;
		case EAPI_ID_HWMON_VOLTAGE_VBAT:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_vbat");
			break;
		case EAPI_ID_HWMON_VOLTAGE_5V:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_5v");
			break;
		case EAPI_ID_HWMON_VOLTAGE_5VSB:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_5vsb");
			break;
		case EAPI_ID_HWMON_VOLTAGE_12V:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_12v");
			break;
		case EAPI_ID_HWMON_FAN_CPU:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/cpu_fan_speed",hwmon_number);
			break;
		case EAPI_ID_HWMON_FAN_SYSTEM:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys1_fan_speed",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_POWER_UP_TIME:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/power_up_time");
			break;
		case EAPI_SEMA_ID_BOARD_RESTART_EVENT:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/restart_event");
			break;
		case EAPI_SEMA_ID_BOARD_CAPABILITIES:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/capabilities");
			break;
		case EAPI_SEMA_ID_BOARD_CAPABILITIES_EX:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/capabilities_ext");
			break;
		case EAPI_SEMA_ID_BOARD_SYSTEM_MIN_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys1_min_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_SYSTEM_MAX_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys1_max_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_SYSTEM_STARTUP_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys1_startup_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_CPU_MIN_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/cpu_min_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_CPU_MAX_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/cpu_max_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_CPU_STARTUP_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/cpu_startup_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_MAIN_CURRENT:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/main_current");
			break;
		case EAPI_SEMA_ID_HWMON_VOLTAGE_GFX_VCORE:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_gfx_vcore");
			break;
		case EAPI_SEMA_ID_HWMON_VOLTAGE_1V05:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_1v05");
			break;
		case EAPI_SEMA_ID_HWMON_VOLTAGE_1V5:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_1v5");
			break;
		case EAPI_SEMA_ID_HWMON_VOLTAGE_VIN:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/voltage_vin");
			break;
		case EAPI_SEMA_ID_HWMON_FAN_SYSTEM_2:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys2_fan_speed", hwmon_number);
			break;
		case EAPI_SEMA_ID_HWMON_FAN_SYSTEM_3:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys3_fan_speed", hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_2ND_SYSTEM_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys2_cur_temp",hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_2ND_SYSTEM_MIN_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys2_min_temp", hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_2ND_SYSTEM_MAX_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys2_max_temp", hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_2ND_SYSTEM_STARTUP_TEMP:
			sprintf(sysfile, "/sys/class/hwmon/hwmon%d/device/sys2_startup_temp", hwmon_number);
			break;
		case EAPI_SEMA_ID_BOARD_POWER_CYCLE:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/power_cycles");
			break;
		case EAPI_SEMA_ID_BOARD_BMC_FLAG:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/bmc_flags");
			break;
		case EAPI_SEMA_ID_BOARD_BMC_STATUS:
			status = EAPI_STATUS_UNSUPPORTED;
			return status;
		case EAPI_SEMA_ID_IO_CURRENT:
			sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/main_current");
			break;
		default:
			status = EAPI_STATUS_UNSUPPORTED;
			return status;
	}

	if (strlen(sysfile) == 0)
	{
		return EAPI_STATUS_SUCCESS;	
	}

	ret = read_sysfs_file(sysfile, res, sizeof(res));

	if (ret == 0)
		status = EAPI_STATUS_SUCCESS;
	
	if (strlen(res) == 0 || ret == -1){
		return EAPI_STATUS_UNSUPPORTED;
	}

	*pValue = atoi(res);

	return status;

}
static int get_regulator_voltage(int id, uint32_t *mVolts, char *buf, uint32_t size)
{
    struct data vm;

	if(id >= MAX_ID)
	{
		return EAPI_STATUS_UNSUPPORTED;
	}

	dev_handle = open("/dev/adl_vm",O_RDONLY);

	if(dev_handle < 0)
	{
		return EAPI_STATUS_UNSUPPORTED;
	}

	vm.id = id;
	vm.volt=0;
	*mVolts = 0;
	memset(vm.volt_desc,0,256);
	ioctl(dev_handle , GET_VOLT_AND_DESC , &vm);
	*mVolts = vm.volt;
	strcpy(buf,vm.volt_desc);
	close(dev_handle);

	return EAPI_STATUS_SUCCESS;
}

uint32_t EApiBoardGetVoltageMonitor(uint32_t id, uint32_t *mVolts, char *buf, uint32_t size)
{
	int ret;

	ret = get_regulator_voltage(id, mVolts, buf, size);
	
	return ret;

}

uint32_t EApiBoardGetVoltageCap(uint32_t *value)
{
	uint32_t vm_cap;

    dev_handle = open("/dev/adl_vm",O_RDONLY);

    if(dev_handle < 0)
    {
        return EAPI_STATUS_UNSUPPORTED;
    }
	ioctl(dev_handle , GET_VOLT_MONITOR_CAP, &vm_cap);
	*value=vm_cap;
	close(dev_handle);
	return EAPI_STATUS_SUCCESS;
}

uint32_t EApiBoardGetErrorLog (uint32_t Pos, uint32_t *ErrorNumber, uint8_t  *Flags, uint8_t  *RestartEvent, uint32_t *PwrCycles, uint32_t *Bootcount, uint32_t *Time, uint8_t *Status, signed char *CPUtemp, signed char *Boardtemp)
{
    char sysfile[128];
	int ret, i, j;
        unsigned char res[32];
	char buf[32];

	uint32_t status = EAPI_STATUS_SUCCESS;
	char *data[] = {"ErrorNumber", "Flags", "RestartEvent", "PowerCycle", "BootCount", "Time", "Status", "CPUTemp", "BoardTemp", NULL};
	char *value[9];

	char pBuffer[1024] = {0};

	memset(res, 0, sizeof(res));
	memset(buf, 0, sizeof(buf));
	/*store exception number to buf*/
	sprintf(buf, "%u", Pos);
	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/error_log");

	ret = write_sysfs_file(sysfile, buf, sizeof(buf));
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	
	ret = read_sysfs_file(sysfile, pBuffer, sizeof(pBuffer));
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	

	char *test = pBuffer, *token;
	for(i = 0; (token = strtok(test, ": \n")) != NULL; i++)
	{
		for(j = 0; data[j] != NULL; j++)
		{
			if(strcmp(data[j], token) == 0)
			{
				token = strtok(NULL,": \n");
				value[j] = strdup(token);
			}
		}
		test = NULL;
	}

	*ErrorNumber = atoi(value[0]);
	strcpy((char *)Flags, value[1]);
	strcpy((char *)RestartEvent, value[2]);
	*PwrCycles = atoi(value[3]);
	*Bootcount = atoi(value[4]);
	*Time = atoi(value[5]);
	strcpy((char *)Status, value[6]);
	strcpy((char *)CPUtemp, value[7]);
	strcpy((char *)Boardtemp, value[8]);

	return status;
}

uint32_t EApiBoardGetCurPosErrorLog (uint32_t *ErrorNumber, uint8_t  *Flags, uint8_t  *RestartEvent, uint32_t *PwrCycles, uint32_t *Bootcount, uint32_t *Time, uint8_t *Status, signed char *CPUtemp, signed char *Boardtemp)
{
	char sysfile[128];
	int ret, i, j;
        unsigned char res[32];

	uint32_t status = EAPI_STATUS_SUCCESS;
	char *data[] = {"ErrorNumber", "Flags", "RestartEvent", "PowerCycle", "BootCount", "Time", "Status", "CPUTemp", "BoardTemp", NULL};
	char *value[9];

	char pBuffer[1024] = {0};

	memset(res, 0, sizeof(res));
	/*store exception number to buf*/
	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/cur_pos_error_log");

	ret = read_sysfs_file(sysfile, pBuffer, sizeof(pBuffer));
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	

	char *test = pBuffer, *token;
	for(i = 0; (token = strtok(test, ": \n")) != NULL; i++)
	{
		for(j = 0; data[j] != NULL; j++)
		{
			if(strcmp(data[j], token) == 0)
			{
				token = strtok(NULL,": \n");
				value[j] = strdup(token);
			}
		}
		test = NULL;
	}

	*ErrorNumber = atoi(value[0]);
	strcpy((char *)Flags, value[1]);
	strcpy((char *)RestartEvent, value[2]);
	*PwrCycles = atoi(value[3]);
	*Bootcount = atoi(value[4]);
	*Time = atoi(value[5]);
	strcpy((char *)Status, value[6]);
	strcpy((char *)CPUtemp, value[7]);
	strcpy((char *)Boardtemp, value[8]);

	return status;
}


uint32_t EApiBoardGetErrorNumDesc(uint32_t Pos, char *pBuf, uint32_t Size)
{
	char sysfile[128];
	int ret;
        unsigned char res[32];
	char buf[32];

	uint32_t status = EAPI_STATUS_SUCCESS;
	memset(res, 0, sizeof(res));
	memset(buf, 0, sizeof(buf));
	/*store exception number to buf*/
	sprintf(buf, "%u", Pos);
	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/err_num_des");

	ret = write_sysfs_file(sysfile, buf, sizeof(buf));
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	

	ret = read_sysfs_file(sysfile, pBuf, Size);
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	
	
	if (pBuf[0] != 'e')
		return EAPI_STATUS_UNSUPPORTED;

        return status;
}


uint32_t EApiBoardGetExcepDesc(uint32_t exc_code, char *exc_desc, uint32_t size)
{
	char sysfile[128];
	int ret;
        unsigned char res[32];
	char buf[32];

	uint32_t status = EAPI_STATUS_SUCCESS;
	memset(res, 0, sizeof(res));
	memset(buf, 0, sizeof(buf));
	/*store exception number to buf*/
	sprintf(buf, "%u", exc_code);
	sprintf(sysfile, "/sys/bus/platform/devices/adl-bmc-boardinfo/information/exc_des");

	ret = write_sysfs_file(sysfile, buf, sizeof(buf));
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	

	ret = read_sysfs_file(sysfile, exc_desc, size);
	if(ret < 0) {
		return EAPI_STATUS_UNSUPPORTED;
	}	

	return status;
}


