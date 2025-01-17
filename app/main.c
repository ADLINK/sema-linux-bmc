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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <errorcodes.h>
#include <eapi.h>
#include <conv.h>
#include <eapi.h>

#define Version	"ADLINK-SEMA-BMC-LINUX-V4_R3_0_25_01_17"

char*			ExeName;
uint8_t	SetWatchdog, TriggerWatchdog, StopWatchdog, WatchDogCap,IsPwrUpWDogStart, IsPwrUpWDogStop;
uint8_t	StorageCap, StorageAreaRead, StorageAreaWrite;
uint8_t	SmartFanTempSet, SmartFanTempGet, SmartFanTempSetSrc, SmartFanTempGetSrc, SmartFanPWMSet;
uint8_t	SmartFanModeGet, SmartFanModeSet, SmartFanPWMGet;
uint8_t	GetStringA, GetValue, GetVoltageMonitor, GetVoltageMonitorCap, GetSEMAVersion;
uint8_t	VgaGetBacklightEnable, VgaSetBacklightEnable, VgaGetBacklightBrightness, VgaSetBacklightBrightness;
uint8_t	GPIOGetDirectionCaps, GPIOGetDirection, GPIOSetDirection, GPIOGetLevel, GPIOSetLevel;
uint8_t GetErrorLog, GetErrorNumberDescription, GetCurrentPosErrorLog, GetExceptionDescription;

static void errno_exit(const char *s) 
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

void ShowHelp(int condition)
{
	if (condition == 0)
	{
		printf("Usage:\n");
		printf("- Display this screen:\n");
		printf("	semautil /h\n");
		printf("- Get SEMA Version:\n");
		printf("	semautil version\n\n");
	}
	if (condition == 1 || condition == 0)
	{
		printf("- Watch Dog:\n");
		printf("  1. semautil /w get_cap\n");
		printf("  2. semautil /w start <sec> \n");
		printf("       sec 0 - 65535\n");
		printf("  3. semautil /w trigger\n");
		printf("  4. semautil /w stop\n");
		printf("  5. semautil /w pwrup_enable <sec (60-65535)> \n");
		printf("  6. semautil /w pwrup_disable\n\n");
	}
	if (condition == 2 || condition == 0)
	{
		uint32_t BlockLength, StorageSize;
		printf("Storage:\n");
		printf("  1. semautil /s get_cap\n");
		printf("  2. semautil /s read  <Address> <Length> \n");
		printf("  3. semautil /s write <Address> <string/value> <Length> \n");
		if (EApiStorageCap(0, &StorageSize, &BlockLength) == 0)
		{
			if (StorageSize == 1024)
			{
				printf("       Note: Address <0 - 1020> and Length should be  4 Bytes aligned\n\n");
			}
			else
			{
				printf("       Note: Address <0 - 508> and Length should be  4 Bytes aligned\n\n");
			}
		}
		printf("			Example : semautil /s write 1020 Aaaa 4\n");
		printf("				It will be written to 1020, 1021, 1022, 1023\n\n");
		printf("       Note: Hexa decimal values are not valid\n\n");
}
	if (condition == 3 || condition == 0)
	{
		printf("- Smart FAN control:\n");
		printf("  1. semautil /f set_temp_points <FanID> <Level1> <Level2> <Level3> <level4> \n");
		printf("       FanID    index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("  2. semautil /f set_pwm_points  <FanID> <PWMLevel1> <PWMLevel2> <PWMLevel3> <PWMlevel4> \n");
		printf("       FanID    index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("  3. semautil /f get_temp_points <FanID> \n");
		printf("       FanID    index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("  4. semautil /f get_pwm_points  <FanID> \n");
		printf("       FanID    index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("  5. semautil /f set_temp_source <FanID> <TempSrc>\n");
		printf("       FanID    index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("       TempSrc   0 - CPU and 1 - Board\n");
		printf("  6. semautil /f get_temp_source <FanID> \n");
		printf("       FanID     index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("  7. semautil /f get_mode        <FanID> \n");
		printf("       FanID     index (0:CPU fan, 1-3: system fan #1-3)\n");
		printf("  8. semautil /f set_mode        <FanID> <Mode>\n");
		printf("       FanID     index (0:CPU fan, #1-3: system fan #1-3)\n");
		printf("       Mode	  mode  (0:Auto, 1: Off, 2: On, 3: Soft)\n\n");
	}
	if (condition == 4 || condition == 0)
	{
		printf("- System monitor-Board Info:\n");
		printf("  1. semautil /i get_bd_info <eapi ID> \n");
		printf("       1  : Board manufacturer name \n");
		printf("       2  : Board name \n");
		printf("       3  : Board serial number\n");
		printf("       4  : Board BIOS revision\n");
		printf("       5  : Board bootloader revision\n");
		printf("       6  : Board restart event\n");
		printf("       7  : HW revision \n");
		printf("       8  : Board application revision\n");
		printf("       9  : Board repair date\n");
		printf("       10 : Board manufacturer date\n");
		printf("       11 : Board MAC address\n");
		printf("       12 : Board 2nd HW revision number\n");
		printf("       13 : Board 2nd serial\n\n");
	}
	if (condition == 5 || condition == 0)
	{
		printf("- Voltage monitor:\n");
		printf("  1. semautil /v get_voltage_cap \n");
		printf("  2. semautil /v get_voltage <Channel> \n");
		printf("       Channel 0-15\n");
	}
	if (condition == 6 || condition == 0)
	{
		printf("Error log:\n");
		printf("  1. semautil /e get_error_log <Position (0-31)>\n");
		printf("  2. semautil /e get_cur_error_log\n");
		printf("  3. semautil /e get_bmc_error_code <Error Number>\n");
	}
	if (condition == 7 || condition == 0)
	{
		printf("Exception Description :\n");
		printf("  1. semautil /x get_excep_desc\n");
	}
	if (condition == 8 || condition == 0)
	{
		printf("GPIO:\n");
		printf("  1. semautil /g get_direction_cap [id]\n");
		printf("  2. semautil /g get_direction     <GPIO Bit>\n");
		printf("  3. semautil /g set_direction     <GPIO Bit> <0 - Output or 1 - Input>\n");
		printf("  4. semautil /g get_level         <GPIO Bit>\n");
		printf("  5. semautil /g set_level         <GPIO Bit> <0 - Low or 1 - High>\n");
		printf("       GPIO set/write parameters:\n");
		printf("       GPIO Bit   0-15   \n");
		printf("        Note: GPIO access may not be available on all platforms\n");
	}
	if (condition == 9 || condition == 0)
	{
		printf("- Board values:\n");
		printf("  1. semautil /d  get_value <EAPI ID> \n");
		printf("       1	:  EAPI Specification Version \n");		
		printf("       2	:  Boot Counter\n");	
		printf("       3	:  Running Time Meter\n");	
		printf("       4	:  Vendor Specific Library Version\n");	
		printf("       5	:  CPU Temperature\n");	
		printf("       6	:  System Temperature\n");	
		printf("       7	:  CPU Core Voltage\n");	
		printf("       8	:  2.5V Voltage\n");	
		printf("       9	:  3.3V Voltage\n");	
		printf("       10	:  Battery Voltage\n");	
		printf("       11	:  5V Voltage\n");	
		printf("       12	:  5V Standby Voltage\n");	
		printf("       13	:  12V Voltage\n");	
		printf("       14	:  CPU Fan\n");   /// not working	
		printf("       15	:  System Fan 1\n");	
		printf("       16	:  Get power uptime\n"); /// not working	
		printf("       17	:  Get restart event\n");	
		printf("       18	:  Get BMC capabilities\n");	
		printf("       19	:  Get extended BMC capabilities\n");	
		printf("       20	:  Board Min Temperature\n");	
		printf("       21	:  Board Max Temperature\n");	
		printf("       22	:  Board Startup Temperature\n");	
		printf("       23	:  CPU Min Temperature\n");	
		printf("       24	:  CPU Max Temperature\n");	
		printf("       25	:  CPU Startup Temperature\n");	
		printf("       26	:  Get main power current\n");	
		printf("       27	:  GFX Voltage\n");		
		printf("       28	:  1.05V Voltage\n");	
		printf("       29	:  1.5V Voltage\n");	
		printf("       30	:  Vin Voltage\n");	
		printf("       31	:  System Fan 2\n");	
		printf("       32	:  System Fan 3\n");	
		printf("       33	:  Board 2nd Current Temperature\n");	
		printf("       34	:  Board 2nd Min temperature\n");	
		printf("       35	:  Board 2nd Max Temperature\n");	
		printf("       36	:  Board 2nd Startup Temperature\n");	
		printf("       37	:  Get Power cycle counter\n");	
		printf("       38	:  Get Board BMC Flag\n");	
		printf("       39	:  Get Board BMC Status\n");	
		printf("       40	:  IO Current\n\n");
	}
	if (condition == 10 || condition == 0)
	{
		printf(" -LVDS Backlight control:\n");
		printf("  1. semautil /b  set_bkl_value   [Id] [Level (0-255)]\n");
		printf("  2. semautil /b  set_bkl_enable  [Id] [0 or 1]\n");
		printf("  3. semautil /b  get_bkl_value   [Id]\n");
		printf("  4. semautil /b  get_bkl_enable  [Id]\n");
		printf("       [ID]       LCD\n");
		printf("        0    EAPI_ID_BACKLIGHT_1\n");
		printf("        1    EAPI_ID_BACKLIGHT_2\n");
		printf("        2    EAPI_ID_BACKLIGHT_3\n");
	}
	if (condition == 11 || condition == 0)
	{
		printf("\n- Generic I2C Read/Write:\n");
		printf("  1. semautil /i2c  bus_cap\n");
		printf("  2. semautil /i2c  probe_device [bus id]\n");
		printf("  3. semautil /i2c  write_raw	 [bus id] [address] [length] byte0 byte1 byte2 ...\n");
		printf("  3. semautil /i2c  read_raw	 [bus id] [address] [length]\n");
		printf("  4. semautil /i2c  read_xfer	 [bus id] [address] [cmd type] [cmd] [length]\n");
		printf("  5. semautil /i2c  write_xfer	 [bus id] [address] [cmd type] [cmd] [length] byte0 byte1 byte2 ...\n");
		printf("  6. semautil /i2c  get_status\n");

		printf("  \n[Bus Id]:\n");
		printf("    ID\tSEMA EAPI ID\t\t\tDescription\n");
		printf("    1\tEAPI_ID_I2C_EXTERNAL\t\tBaseboard I2C Interface\n");
		printf("    2\tEAPI_ID_I2C_LVDS_1\t\tLVDS \\ EDP 1 Interface\n");
		printf("    3\tEAPI_ID_I2C_LVDS_2\t\tLVDS \\ EDP 2 Interface\n");
		printf("    4\tSEMA_EAPI_EAPI_ID_I2C_EXTERNAL\t2nd External I2C Interface\n");
		printf("    5\tSEMA_EAPI_EAPI_ID_I2C_EXTERNAL\t3rd External I2C Interface\n");

		printf("  [Command type]:\n");
		printf("    ID\tENCODED CMD ID\t\tDescription\n");
		printf("    1\tEAPI_I2C_NO_CMD\t\tSpecify no command/index is used\n");
		printf("    2\tEAPI_I2C_ENC_STD_CMD\tExtended standard 8 bits CMD\n");
		printf("    3\tEAPI_I2C_ENC_EXT_CMD\tExtended standard 16 bits CMD\n");
	}}


int DispatchCMDToSEMA(int argc,char *argv[])
{
	int ret  = 0;
	/* Board information*/
	uint32_t Id, Size, Value = 0;
	char BoardInfo[64];
	char ExcepDesc[1024];
	memset(BoardInfo, 0, sizeof(BoardInfo));			
	memset(ExcepDesc, 0, sizeof(ExcepDesc));
	uint8_t ExceptionCode;
	uint32_t Pos, ErrorNumber = 0;
	uint8_t  Flags[20], RestartEvent[20];
	uint32_t PwrCycles, Bootcount, Time;
	uint8_t Status[20];
	signed char CPUtemp[20], Boardtemp[20];
	/*Voltage Monitor*/
	uint32_t Vid, Voltage;
	char Vmbuf[32];
	memset(Vmbuf, 0, sizeof(Vmbuf));				
	/* Backlight */
	uint32_t bid, enable;
	uint32_t brightness;
	/* Watchdog */
	uint32_t MaxDelay, MaxEventTimeout, MaxResetTimeout, delay, EventTimeout, ResetTimeout;
	/* Fan */
	int fid = 0, Level1, Level2, Level3, Level4, Tempsrc, fan_mode;
	/* Storage */
	uint32_t Offset, BufLen, ByteCnt;
	char *Buffer, memcap[1024];
	uint32_t Storagesize = 0, BlockLength = 0;
	// Library Initializing
	ret = EApiLibInitialize();
	if (ret){
		if (ret == -1){
			printf("Initialization Failed\n");
			exit(EXIT_FAILURE);
		}
		else
			errno_exit("Initialization");
	}
	if (VgaGetBacklightEnable)
	{
		if (argc != 4) {
			printf("Wrong arguments VgaGetBacklightEnable\n");
			exit(-1);
		}					
		bid = atoi(argv[3]);
		ret = EApiVgaGetBacklightEnable(bid, &enable);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiVgaGetBackLightEnable");
		} 
		if(enable)
			printf("Backlight set OFF\n");
		else
			printf("Backlight set ON\n");
	}
	if (VgaSetBacklightEnable)
	{
		if (argc != 5) {
			printf("Wrong arguments VgaSetBacklightEnable\n");
			exit(-1);
		}
		bid = atoi(argv[3]);
		enable = atoi(argv[4]);
		ret = EApiVgaSetBacklightEnable(bid, enable);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiVgaSetBackLightEnable");
		}
		printf("Backlight Enable Set\n");
	}
	if (VgaGetBacklightBrightness)
	{
		if (argc != 4) {
			printf("Wrong arguments VgaGetBacklightBrightness\n");
			exit(-1);
		}
		bid = atoi(argv[3]);
		ret = EApiVgaGetBacklightBrightness(bid, &brightness);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiVgaGetBackLightBrightness");
		}
		printf("Backlight Brightness: %u\n", brightness);
	}
	if(VgaSetBacklightBrightness)
	{
		if (argc != 5) {
			printf("Wrong arguments VgaSetBacklightBrightness\n");
			exit(-1);
		}
		bid = atoi(argv[3]);
		brightness = atoi(argv[4]);
		ret = EApiVgaSetBacklightBrightness(bid, brightness);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiVgaSetBackLightBrightness");
		}
	}
	if (GetValue)
	{
		if (argc != 4) {
			printf("Wrong arguments GetValue\n");
			exit(-1);
		}
		Id = atoi(argv[3]);
		ret = EApiBoardGetValue(Id, &Value);
		if (ret){
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetValue");
		}
		
		if (Id == 5)
		{
			printf("Value: %u C\n", Value);	
		}
		else if (Id == 26)
		{
			printf("Value: %u mA\n", Value);	
		}
		else
		{
			printf("Value: %u\n", Value);
		}
	}
	if (GetStringA)
	{
		if (argc != 4) {
			printf("Wrong arguments GetStringA\n");
			exit(-1);
		}
		Id = atoi(argv[3]);
		Size = sizeof(BoardInfo);
		ret = EApiBoardGetStringA(Id, BoardInfo, &Size);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetStringA");
		}
		printf("%s\n", BoardInfo);
	}
	if (WatchDogCap)
	{
		ret = EApiWDogGetCap(&MaxDelay, &MaxEventTimeout, &MaxResetTimeout);
		if (ret)
			errno_exit("EApiWDogGetCap");
		printf("MaxDelay\t: %u\nMaxEventTimeout : %u\nMaxResetTimeout : %u\n", MaxDelay, MaxEventTimeout, MaxResetTimeout);
	}
	if (SetWatchdog)
	{
		if (argc != 4) {
			printf("Wrong arguments SetWatchdog\n");
			exit(-1);
		}
		delay = 0;
		EventTimeout = 0;
		ResetTimeout = atoi(argv[3]);
		ret = EApiWDogStart(delay, EventTimeout, ResetTimeout);
		if (ret == EAPI_STATUS_RUNNING)
		{
			errno = EALREADY;
			errno_exit("EApiWDogStart");
		}

		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiWDogStart");
		}
		printf("Watchdog timeout is set\n");
	}
	if (TriggerWatchdog)				
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		ret = EApiWDogTrigger();
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiWDogTrigger");
		}
		printf("Watchdog Triggered\n");
	}
	if (StopWatchdog)
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		ret = EApiWDogStop();
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiWDogStop");
		}
		printf("Watchdog Stop Success\n");
	}

	if (IsPwrUpWDogStart)				
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		ResetTimeout = atoi(argv[3]);
		ret = EApiPwrUpWDogStart(ResetTimeout);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiPwrUpWDogStart");
		}
		printf("Watchdog Triggered\n");
	}


	if (IsPwrUpWDogStop)				 
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		ret = EApiPwrUpWDogStop();
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiPwrUpWDogStop");
		}
		printf("PowerUp Watchdog Stop Success\n");
	}


	if (SmartFanTempSet)
	{
		if (argc != 8) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		Level1 = atoi(argv[4]);
		Level2 = atoi(argv[5]);
		Level3 = atoi(argv[6]);
		Level4 = atoi(argv[7]);

		ret = EApiSmartFanSetTempSetpoints(fid, Level1, Level2, Level3, Level4);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanSetTempSetpoints");
		}
		printf("Fan ID: %d, Temperature set points done!\n", fid);
	}

	if (SmartFanTempGet)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		ret = EApiSmartFanGetTempSetpoints(fid, &Level1, &Level2, &Level3, &Level4);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanGetTempSetpoints");
		}
		printf("Fan ID: %d\nLevel1: %d\nLevel2: %d\nLevel3: %d\nLevel4: %d\n", fid, Level1, Level2, Level3, Level4);

	}

	if (SmartFanPWMSet)
	{
		if (argc != 8) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		Level1 = atoi(argv[4]);
		Level2 = atoi(argv[5]);
		Level3 = atoi(argv[6]);
		Level4 = atoi(argv[7]);

		ret = EApiSmartFanSetPWMSetpoints(fid, Level1, Level2, Level3, Level4);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanSetPWMSetpoints");
		}
		printf("Fan ID: %d, PWM points are set!\n", fid);
	}

	if (SmartFanPWMGet)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		ret = EApiSmartFanGetPWMSetpoints(fid, &Level1, &Level2, &Level3, &Level4);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanGetPWMSetpoints");
		}
		printf("Fan ID: %d\nLevel1: %d\nLevel2: %d\nLevel3: %d\nLevel4: %d\n", fid, Level1, Level2, Level3, Level4);
	}

	if (SmartFanModeSet)
	{
		if (argc != 5) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		fan_mode = atoi(argv[4]);
		ret = EApiSmartFanSetMode(fid, fan_mode);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanSetMode");
		}
		printf("Fan ID: %d, set to Mode: %d\n", fid, fan_mode);
	}


	if (SmartFanModeGet)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		ret = EApiSmartFanGetMode(fid, &fan_mode);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanGetMode");
		}
		printf("Fan id: %d\nFan Mode: %d\n", fid, fan_mode);

	}


	if (SmartFanTempSetSrc)
	{
		if (argc != 5) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		Tempsrc = atoi(argv[4]);
		ret = EApiSmartFanSetTempSrc(fid, Tempsrc);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanSetTempSrc");
		}
		printf("Fan ID: %d, set to Temperature source: %d\n", fid, Tempsrc);
	}


	if (SmartFanTempGetSrc)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		fid = atoi(argv[3]);
		ret = EApiSmartFanGetTempSrc(fid, &Tempsrc);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiSmartFanGetTempSrc");
		}
		printf("Fan ID: %d\nTemperature source: %d\n", fid, Tempsrc);
	}


	// Storage Functions Start

	if (StorageCap)
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}

		Id = EAPI_ID_STORAGE_STD;
		ret = EApiStorageCap(Id, &Storagesize, &BlockLength);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiStorageCap");
		}
		printf("Sorage Capabilities:\nStorage Size: %u\nBlock Length: %u\n", Storagesize, BlockLength);
	}


	if (StorageAreaRead)
	{
		if (argc != 5) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Id = EAPI_ID_STORAGE_STD;
		memset(memcap, 0, sizeof(memcap));
		Offset = atoi(argv[3]);
		ByteCnt = atoi(argv[4]);
		BufLen = sizeof(memcap);
		ret = EApiStorageAreaRead(Id, Offset, memcap, BufLen, ByteCnt);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiStorageAreaRead");
		}
		printf("Read Buffer: %s\n", memcap);
	}


	if (StorageAreaWrite)
	{
		if (argc != 6) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Id = EAPI_ID_STORAGE_STD;
		Offset = atoi(argv[3]);
		Buffer = argv[4];
		ByteCnt = atoi(argv[5]);
		ret = EApiStorageAreaWrite(Id, Offset, Buffer, ByteCnt);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiStorageAreaWrite");
		}
		printf("Write buffer: %s\n", Buffer);
	}



	if (GPIOGetDirectionCaps)
	{
		if (argc != 4){
			printf("Wrong arguments\n");
			exit(-1);
		}
		uint32_t input , output;
		Id = atoi(argv[3]);
		ret = EApiGPIOGetDirectionCaps(Id, &input, &output);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiGPIOGetDirectionCaps");
		}
		printf("GPIO Capabilities:\nInput : %u\nOutput: %u\n", input, output);
	}


	if (GPIOGetDirection)
	{
		uint32_t bitmask=0, dir;
	//	if((strncmp(argv[2], "0x", 2) != 0) || !Conv_HexString2DWord(argv[2], &bitmask)) {
	//		printf("Invalid GPIO Bitmask input\n");
	//		exit(1);
	//	}

                 bitmask = atoi(argv[3]);
		ret = EApiGPIOGetDirection(0, bitmask, &dir);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiGPIOGetDirection");
		}
		printf("GPIO Direction : %u\n", dir);
	}


	if (GPIOSetDirection)
	{
		uint32_t bitmask=0, dir=0;

                bitmask = atoi(argv[3]);
                dir = atoi(argv[4]);
		ret = EApiGPIOSetDirection(0, bitmask, dir);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiGPIOSetDirection");
		}
	}

	if (GPIOGetLevel)
	{
		uint32_t bitmask=0, val=0;

                bitmask = atoi(argv[3]);

		ret = EApiGPIOGetLevel(0, bitmask, &val);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiGPIOGetLevel");
		}
		printf("Value: %u\n", val);
	}


	if (GPIOSetLevel)
	{
		uint32_t bitmask=0, val=0;

                bitmask = atoi(argv[3]);
                val = atoi(argv[4]);
		ret = EApiGPIOSetLevel(0, bitmask, val);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiGPIOSetLevel");
		}
	}


	if (GetErrorLog)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Size = sizeof(ExcepDesc);
		Pos = atoi(argv[3]);
		ret = EApiBoardGetErrorLog (Pos, &ErrorNumber, Flags, RestartEvent, &PwrCycles, &Bootcount, &Time, Status, CPUtemp, Boardtemp);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetErrororLog");
		}
		printf("ErrorNumber: %u\nFlags: %s\nRestartEvent: %s\nPwrCycles: %u\nBootcount: %u\nTime: %u\n", ErrorNumber, Flags, RestartEvent, PwrCycles, Bootcount, Time);
		printf("Status: %s\nCPUtemp: %s\nBoardtemp: %s\n", Status, CPUtemp, Boardtemp);
	}


	if (GetErrorNumberDescription)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Size = sizeof(ExcepDesc);
		Pos = atoi(argv[3]);
		ret = EApiBoardGetErrorNumDesc(Pos, ExcepDesc, Size);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetErrorNumDescLog");
		}
		printf("%s\n", ExcepDesc);
	}

	if (GetCurrentPosErrorLog)
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Size = sizeof(ExcepDesc);
		ret = EApiBoardGetCurPosErrorLog (&ErrorNumber, Flags, RestartEvent, &PwrCycles, &Bootcount, &Time, Status, CPUtemp, Boardtemp);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetCurPosErrorLog");
		}
		printf("ErrorNumber: %u\nFlags: %s\nRestartEvent: %s\nPwrCycles: %u\nBootcount: %u\nTime: %u\n", ErrorNumber, Flags, RestartEvent, PwrCycles, Bootcount, Time);
		printf("Status: %s\nCPUtemp: %s\nBoardtemp: %s\n", Status, CPUtemp, Boardtemp);
	}

	if (GetExceptionDescription)
	{
		if (argc != 3) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Size = sizeof(ExcepDesc);
		for (ExceptionCode = 0; ExceptionCode < 32; ExceptionCode++) 
		{
		ret = EApiBoardGetExcepDesc(ExceptionCode, ExcepDesc, Size);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetExcepDesc");
		}
	
		printf("%02u -> %s", ExceptionCode, ExcepDesc);
		}
	}

	if(GetVoltageMonitorCap)
	{
		uint32_t value=0;

		if (argc != 3){
			printf("Wrong arguments\n");
			exit(-1);
		}

		ret= EApiBoardGetVoltageCap(&value);
		if(ret){
			printf("Get EApi information failed\n");
       	                errno_exit("EApiBoardGetVoltageCap");
		}

		if(value ==1)
			printf("\nVoltage monitor is compatible for this platform\n\n");
		else
			printf("\nVoltage monitor is not compatible for this platform\n\n");

	}

	if (GetVoltageMonitor)
	{
		if (argc != 4) {
			printf("Wrong arguments\n");
			exit(-1);
		}
		Vid = atoi(argv[3]);
		if (Vid < 0 || Vid > 15) {
                       printf("Invalid Channel. Please enter channel in between 0 - 15.\n");
                       exit(-1);
                }
		Size = sizeof(Vmbuf);
		ret = EApiBoardGetVoltageMonitor(Vid, &Voltage, Vmbuf, Size);
		if (ret) {
			printf("get eapi information failed\n");
			errno_exit("EApiBoardGetVoltageMonitor");
		}
		
		if(Voltage == 0)
		{
			printf("Invalid Channel\n");
		}
		else
		{
			printf("Description: %s\nVoltage: %u\n", Vmbuf, Voltage);
		}
	}
	
	if(GetSEMAVersion)
	{
		printf("\nSEMA Version : %s\n\n",Version);
	}

	return 0;
}

signed int ParseArgs(int argc, char* argv[])
{
	int help_condition = 0;
	int eRet = 1;
	if (argc == 1)
	{
		ShowHelp(help_condition);
		return -1;
	}
	if (strcasecmp(argv[1], "/h") == 0)
	{
		ShowHelp(help_condition);
		return -1;
	}
	/* Watch Dog*/
	else if (strcasecmp(argv[1], "/w") == 0)
	{
		if (argc == 3 && (strcasecmp(argv[2], "get_cap") == 0))
		{
			WatchDogCap = TRUE;
		}
		else if (argc == 4 && strcasecmp(argv[2], "start") == 0)
		{
			SetWatchdog = TRUE;
		}
		else if (argc == 3 && (strcasecmp(argv[2], "trigger") == 0))
		{
			TriggerWatchdog = TRUE;
		}
		else if (argc == 3 && (strcasecmp(argv[2], "stop") == 0))
		{
			StopWatchdog = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "pwrup_enable") == 0))
		{
			IsPwrUpWDogStart = TRUE;
		}
		else if (argc == 3 && (strcasecmp(argv[2], "pwrup_disable") == 0))
		{
			IsPwrUpWDogStop = TRUE;
		}
		else
		{
			help_condition = 1;
		}
	}
	else if (strcasecmp(argv[1], "/s") == 0)
	{
		if (argc == 3 && (strcasecmp(argv[2], "get_cap") == 0))
		{
			StorageCap = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "read") == 0))
		{
			StorageAreaRead = TRUE;
		}
		else if (argc == 6 && (strcasecmp(argv[2], "write") == 0))
		{
			StorageAreaWrite = TRUE;
		}
		else
		{
			help_condition = 2;
		}
	}
	else if (strcasecmp(argv[1], "/f") == 0)
	{
		if (argc == 8 && (strcasecmp(argv[2], "set_temp_points") == 0))
		{
			SmartFanTempSet = TRUE;
		}
		else if (argc == 8 && (strcasecmp(argv[2], "set_pwm_points") == 0))
		{
			SmartFanPWMSet = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_temp_points") == 0))
		{
			SmartFanTempGet = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_pwm_points") == 0))
		{
			SmartFanPWMGet = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_temp_source") == 0))
		{
			SmartFanTempGetSrc = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_temp_source") == 0))
		{
			SmartFanTempSetSrc = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_mode") == 0))
		{
			SmartFanModeGet = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_mode") == 0))
		{
			SmartFanModeSet = TRUE;
		}
		else
		{
			help_condition = 3;
		}
	}
	else if (strcasecmp(argv[1], "/i") == 0)
	{
		if (argc == 4 && (strcasecmp(argv[2], "get_bd_info") == 0))
		{
			GetStringA = TRUE;
		}
		else
		{
			help_condition = 4;
		}
	}
	else if (strcasecmp(argv[1], "/v") == 0)
	{
		if (argc == 3 && (strcasecmp(argv[2], "get_voltage_cap") ==0))
		{
			GetVoltageMonitorCap = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_voltage") == 0))
		{
			GetVoltageMonitor = TRUE;
		}
		else
		{
			help_condition = 5;
		}
	}
	else if (strcasecmp(argv[1], "/e") == 0)
	{
		if (argc == 4 && (strcasecmp(argv[2], "get_error_log") == 0))
		{
			GetErrorLog = TRUE;
		}
		else if (argc == 3 && (strcasecmp(argv[2], "get_cur_error_log") == 0))
		{
			GetCurrentPosErrorLog = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_bmc_error_code") == 0))
		{
			GetErrorNumberDescription = TRUE;
		}
		else
		{
			help_condition = 6;
		}
	}
	else if (strcasecmp(argv[1], "/x") == 0)
	{
		if (argc == 3 && (strcasecmp(argv[2], "get_excep_desc") == 0))
		{
			GetExceptionDescription = TRUE;
		}
		else
		{
			help_condition = 7;
		}
	}
	else if (strcasecmp(argv[1], "/g") == 0)
	{
		if (argc == 4 && (strcasecmp(argv[2], "get_direction_cap") == 0)) /***GPIO****/
		{
			GPIOGetDirectionCaps = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_direction") == 0))
		{
			GPIOGetDirection = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_direction") == 0))
		{
			GPIOSetDirection = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_level") == 0))
		{
			GPIOGetLevel = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_level") == 0))
		{
			GPIOSetLevel = TRUE;
		}
		else
		{
			help_condition = 8;
		}
	}
	else if (strcasecmp(argv[1], "/d") == 0)
	{
		if (argc == 4 && (strcasecmp(argv[2], "get_value") == 0))
		{
			GetValue = TRUE;
		}
		else
		{
			help_condition = 9;
		}
	}
	else if (strcasecmp(argv[1], "/b") == 0)
	{
		if (argc == 4 && (strcasecmp(argv[2], "get_bkl_enable") == 0))
		{
			VgaGetBacklightEnable = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_bkl_enable") == 0))
		{
			VgaSetBacklightEnable = TRUE;
		}
		else if (argc == 4 && (strcasecmp(argv[2], "get_bkl_value") == 0))
		{
			VgaGetBacklightBrightness = TRUE;
		}
		else if (argc == 5 && (strcasecmp(argv[2], "set_bkl_value") == 0))
		{
			VgaSetBacklightBrightness = TRUE;
		}
		else
		{
			help_condition = 10;
		}
	}
	else if (strcasecmp(argv[1], "version") == 0)
	{
		GetSEMAVersion = TRUE;
	}
	else
	{
		ShowHelp(help_condition);
		eRet = -3;
	}
	if (help_condition != 0)
	{
		ShowHelp(help_condition);
		eRet = -3;
	}
	return eRet;
}

int main(int argc , char* argv[])
{

	if (ParseArgs(argc, argv) < 0)
	{
		//printf("Argument Failed \n");
		return -1;
	}

	if (DispatchCMDToSEMA(argc,argv) < 0)
	{
		printf("Failed\n");
		return -2;
	}

	EApiLibUnInitialize();
	return 0;
}


