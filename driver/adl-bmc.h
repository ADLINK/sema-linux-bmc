/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
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

/**
 * @file adl-bmc.h 
 * @author 
 * @brief File containing the SEMA Linux 4.0 driver Read/Write Function Definitions
 *
 *
 */

#ifndef _ADL_BMC_H
#define _ADL_BMC_H


#define DRIVER_VERSION "1.0"

#define	MAX_BUFFER_SIZE		32
/* BMC Capabilities */
#define ADL_BMC_CAP_UPTIME 		(1 << 0)
#define ADL_BMC_CAP_RESTSRTEVT 		(1 << 1)
#define ADL_BMC_CAP_MEM			(1 << 2)
#define ADL_BMC_CAP_WD 			(1 << 3)
#define ADL_BMC_CAP_TEMP	        (1 << 4)
#define ADL_BMC_CAP_VM			(1 << 5)
#define ADL_BMC_CAP_CURRENTS 		(1 << 10)
#define ADL_BMC_CAP_BOOT_COUNTER 	(1 << 11)
#define ADL_BMC_CAP_FAN_CPU             (1 << 18)
#define ADL_BMC_SYS_FAN1_CAP            (1 << 19)
#define ADL_BMC_SYS_FAN2_CAP            (1 << 26)
#define ADL_BMC_SYS_FAN3_CAP            (1 << 27)
#define ADL_BMC_CAP_GPIOS               (1 << 28)



#define ADL_BMC_CAP_TEMP_1                   (1 << 0)
#define ADL_BMC_ERR_LOG_CAP 		     (1 << 3)	
#define ADL_BMC_SOFT_FAN_PWM_INTERPOLATION_CAP          (1 << 7)

/* BMC Commands*/
#define ADL_BMC_CMD_GET_BOARDERRLOG       	0x1c            ///< Get Board Error Log
#define ADL_BMC_CMD_SET_WD                      0x20            ///< Set/Clear Watchdog-timer
#define ADL_BMC_CMD_SET_PWD                     0x22            ///< Set/Clear PowerUp Watchdog-timer
#define ADL_BMC_CMD_SYSCFG                      0x27  		///< Get/Set system config register
#define ADL_BMC_CMD_CAPABILITIES		0x2F		///< Get BMC Capabilities
#define ADL_BMC_CMD_RD_VERSION1			0x30		///< Read version string 1
#define ADL_BMC_CMD_RD_VERSION2			0x31		///< Read version string 2
#define ADL_BMC_CMD_RD_TOM			0x32		///< Read total uptime minutes
#define ADL_BMC_CMD_RD_PWRUP_SECS		0x33		///< Read total seconds since power up
#define ADL_BMC_CMD_RD_PWRCYCLES		0x34		///< Read number of power cycles
#define ADL_BMC_CMD_RD_BMC_FLAGS		0x35		///< Read BMC flags
#define ADL_BMC_CMD_RD_RESTARTEVT		0x36		///< Read last system restart event
#define ADL_BMC_CMD_RD_CPU_TEMP			0x37		///< Read CPU Temperature
#define ADL_BMC_CMD_RD_SYSTEM_TEMP		0x38		///< Read System/Board Temperature
#define ADL_BMC_CMD_RD_MINMAX_TEMP		0x39		///< Read CPU Minimum/Maximum CPU and Board temperatures
#define ADL_BMC_CMD_RD_STARTUP_TEMP		0x3A		///< Read CPU Start Up temperature of CPU and Board
#define ADL_BMC_CMD_RD_BMC_STATUS		0x3D		///< Read BMC status
#define ADL_BMC_CMD_RD_BOOT_COUNTER_VAL		0x3E		///< Read number of boots
#define ADL_BMC_CMD_RD_BLVERSION		0x3F		///< Read boot loader version
#define ADL_BMC_CMD_SET_ADDRESS        		0x40            ///< Set address and length for flash access
#define ADL_BMC_CMD_WRITE_DATA                  0x41            ///< Write data to user flash
#define ADL_BMC_CMD_READ_DATA                   0x42            ///< Read data from user flash
#define ADL_BMC_CMD_CLEAR_DATA                  0x43            ///< Clear data from user flash
#define ADL_BMC_CMD_RD_AIN0			0x60		///< Read analog input Ch0
#define ADL_BMC_CMD_RD_CPU_FAN			0x68		///< Read CPU fan speed
#define ADL_BMC_CMD_RD_SYSTEM_FAN_1		0x6A		///< Read system fan 1 speed
#define ADL_BMC_CMD_RD_SYSTEM_FAN_2		0x6B		///< Read system fan 2 speed
#define ADL_BMC_CMD_RD_SYSTEM_FAN_3		0x6C		///< Read system fan 3 speed
#define ADL_BMC_CMD_RD_MPCURRENT		0x69		///< Read main power current 
#define ADL_BMC_CMD_EXC_CODE_TABLE		0x6F		///< Read exception code table
#define ADL_BMC_CMD_RD_MF_DATA_HW_REV		0x70		///< Read hardware revision string
#define ADL_BMC_CMD_RD_MF_DATA_SR_NO		0x71		///< Read board serial number
#define ADL_BMC_CMD_RD_MF_DATA_LR_DATA		0x72		///< Read board last repair date
#define ADL_BMC_CMD_RD_MF_DATA_MF_DATE		0x73		///< Read board manufactured date
#define ADL_BMC_CMD_RD_MF_DATA_2HW_REV		0x74		///< Read board 2nd hardware revision string
#define ADL_BMC_CMD_RD_MF_DATA_2SR_NO		0x75		///< Read board 2nd serial number
#define ADL_BMC_CMD_RD_MF_DATA_MAC_ID		0x76		///< Read board MAC id
#define ADL_BMC_CMD_RD_MF_DATA_RES		0x77		///< Read for future data 
#define ADL_BMC_CMD_RD_MF_DATA_PLATFORM_ID	0x78		///< Read platform id string
#define ADL_BMC_CMD_RD_MF_DATA_PLATFORM_SPEC_VER	0x79	///< Read platform specification version
#define ADL_BMC_CMD_SET_BKLITE                  0x80            ///< Set Backlight Brightness
#define ADL_BMC_CMD_GET_BKLITE                  0x81            ///< Get Backlight Brightness

#define ADL_BMC_CPU_FAN_TEMP_THRE_REG           0xA0
#define ADL_BMC_CPU_FAN_PWM_THRE_REG            0xA1
#define ADL_BMC_SYS_FAN1_TEMP_THRE_REG          0xA2
#define ADL_BMC_SYS_FAN1_PWM_THRE_REG           0xA3
#define ADL_BMC_SYS_FAN2_TEMP_THRE_REG          0xA8
#define ADL_BMC_SYS_FAN3_TEMP_THRE_REG          0xAA
#define ADL_BMC_SYS_FAN2_PWM_THRE_REG           0xA9
#define ADL_BMC_SYS_FAN3_PWM_THRE_REG           0xAB

#define ADL_BMC_CMD_GET_ADC_SCALE		0xA4
#define ADL_BMC_CMD_GET_VOLT_DESC		0xA5
#define ADL_BMC_CMD_EXT_HW_DESC			0xA6

#define ADL_BMC_CMD_RD_AIN8			0xD0
#define ADL_BMC_CMD_RD_SYSTEM2_TEMP		0xE0		///< Read current board 2nd Temperature
#define ADL_BMC_CMD_RD_SYSTEM2_MINMAX_TEMP	0xE1		///< Read minimum/maximum 2nd cpu and 2nd board temperatures
#define ADL_BMC_CMD_RD_SYSTEM2_STARTUP_TEMP	0xE2		///< Read 2nd startup temperaures of cpu and board


typedef enum
{
	SRE_UNKNOWN  	  = 0x00,
	SRE_SW_RESET 	  = 0x20,
	SRE_HW_RESET  	  = 0x30,
	SRE_WATCHDOG      = 0x40,
	SRE_BIOS_FAULT    = 0x50,
	SRE_POWER_DOWN    = 0x60,
	SRE_POWER_LOSS    = 0x70,
	SRE_POWER_CYCLE   = 0x80,
	SRE_VIN_DROP      = 0x90,
	SRE_POWER_FAIL    = 0xA0,
	SRE_CRIT_TEMP     = 0xB0,
	SRE_WAKEUP        = 0xC0
}tSRE;



#define ADL_MAX_HW_MTR_INPUT			16 		//Max Number of supported voltages


#define SEMA_MAX_CAPABILITY_GROUP 8


#define DEBUG 0	

#if DEBUG
#define debug_printk(fmt...) printk(fmt)
#else
#define debug_printk(fmt...) 
#endif



struct adl_bmc_dev {
	struct device *dev;
	struct i2c_client *i2c_client;

	/* Parameters in the BMC */
	unsigned char *boardid;

        uint32_t Bmc_Capabilities[8];

};

// Collect capabilities
void CollectCapabilities(unsigned int *Capabilities, unsigned DataCount, unsigned char *SMBusDatas);

//Read 
int adl_bmc_i2c_read_device(struct adl_bmc_dev *adl_bmc, char reg, int bytes, void *dest);


//Write
int adl_bmc_i2c_write_device(struct adl_bmc_dev *adl_bmc, int reg, int bytes, void *src);


#endif
