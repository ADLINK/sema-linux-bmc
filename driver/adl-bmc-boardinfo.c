#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/string.h>

#include "adl-bmc.h"

static unsigned short errnum_desc;

struct kobject *kobj_ref;

#pragma pack(1)
struct boarderrlog {
	unsigned short errnum;
	unsigned char flags;
	unsigned char restartevent;
	unsigned int pwrcycles;
	unsigned int bootcnt;
	unsigned int time;
	unsigned short status;
	char cputemp;
	char boardtemp;
};

int converttoint(char *buf)
{
        int i, result = 0;
        for(i = 0;buf[i] != 0; i++)
        {
                if(!(buf[i] <= '9' && buf[i] >= '0'))
                        break;
                result = (result * 10) + buf[i] - '0';
        }
        return result;
}

unsigned short get_voltage_id(unsigned char ch)
{
	int ret;
	unsigned char buff[32];
	unsigned short id = 0;

	if (ch >= 16)
		return id;

	memset(buff, 0, sizeof(buff));
	ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_VOLT_DESC, 0, (void *)buff);
	if (ret < 0)
		return ret;

	if (ret < 16)
		return id;
	if ((ch * 2) >= ret)
		return id;

	id = ((unsigned short)buff[2 * ch]) << 8 | buff[2 * ch + 1];

	return id;
}

unsigned char get_cur_channel(void)
{
	static unsigned char channel = 0;
	static unsigned char currentch = 0;

	if (!currentch)
	{
		int i;
		for (i = 0; i < 16; i++)
		{
			if (get_voltage_id(i) == 0x000F) {
				channel = i;
				currentch = 1;
				return channel;
			}
		}
	}

	return channel;
}

unsigned short get_scale_factor(unsigned char ch)
{
	static unsigned short scale[16];
	static unsigned char scaleavail = 0;

	if (ch >= 16)
		return 0;

	if (!scaleavail)
	{
		unsigned char ret, i, buff[32];
		memset(buff, 0, sizeof(buff));
		ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_ADC_SCALE, 0, (void *)buff);
		if ((ret < 16) && (buff[0] == 0xf0))
			return 0;

		if ((ch * 2) >= ret)
			return 0;

		for (i = 0; i < ret / 2; i++)
			scale[i] = ((unsigned short)buff[i*2]) << 8 | buff[i * 2 + 1];
		scaleavail = 1;
	}
	return scale[ch];
}

int get_voltage_value(unsigned char ch, int *pValue)
{
	u64 vol_val, vol_val_fl;
	unsigned char buff[32];
	unsigned char buff_gain[32];
	unsigned short gain;
	int ret;
	*pValue = 0;	

	if (ch >= 16)
		return -1;

	if (ch < 8) {
		if(2 != adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_AIN0 + ch, 0, (void *)buff))
			return -1;
	}
	else {
		if(2 != adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_AIN8 + (ch - 8), 0, (void *)buff))
			return -1;	
	}

	ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_ADC_SCALE, 32, buff_gain);
	if (ret < 0)
		return ret;
	gain = (buff_gain[ch * 2] << 8) | buff_gain[(ch * 2) + 1];
	vol_val_fl = 3223 * gain;
	vol_val = vol_val_fl * (buff[0] << 8 | buff[1]);
	vol_val = vol_val / 1000000;
	*pValue = vol_val;

	return 0;

}

int get_voltage_description(unsigned char Ch, char *Buffer)
{
	unsigned char	buf[32] = { 0 };
	unsigned short	ID;
	int length;
	if (Ch >= 16)
		return -1;	
	length = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_VOLT_DESC, 0, buf);
	if (length <= 0) return -1;
	if (length < 16)
		return -1;	
	if((Ch * 2)>= length){
		return -1;	
	}
	ID = ((unsigned short)buf[2*Ch])<<8|buf[2*Ch+1];

	if (((ID>0x000C) && (ID<=0x000E)) ||
		((ID>=0x0010) && (ID<=0x00FF))	)
	{
		sprintf(Buffer, "AIN%d", Ch);
		return 0;
	}
	if(ID>0xFF)
	{
	     	sprintf(Buffer,"%hu", ID);
		return 0;
	}

	// Pre-defindes voltage descriptions
	switch (ID)
	{
		case 0x0001:	sprintf(Buffer, "CPU-Vcore");	break;
		case 0x0002:	sprintf(Buffer, "GFX-Vcore");	break;
		case 0x0003:	sprintf(Buffer, "VTT");			break;
		case 0x0004:	sprintf(Buffer, "VSA");			break;
		case 0x0005:	sprintf(Buffer, "VIO1");		break;
		case 0x0006:	sprintf(Buffer, "VIO2");		break;
		case 0x0007:	sprintf(Buffer, "VIO3");		break;
		case 0x0008:	sprintf(Buffer, "VIO4");		break;
		case 0x0009:	sprintf(Buffer, "VIN");			break;
		case 0x000A:	sprintf(Buffer, "VMEM");		break;
		case 0x000B: 	sprintf(Buffer, "PCH-Vcore");	break;
		case 0x000C:	sprintf(Buffer, "VRTC");		break;
		case 0x000F:	sprintf(Buffer, "Main current");break;
		default:		return -1;
	}
	return 0;
}


int get_voltage_description_ext(unsigned char Ch , char *Buffer , bool truncate)
{
	static char desc[16][17] = { { 0 } };
	static bool descriptionavailable = false;
	
	if (Ch >= 16)
		return -1;	
	if (!descriptionavailable)
	{
		int i;
		unsigned char buf[32];	
		for (i = 0; i < 16; i++){
			unsigned char len;
			int ret;
			memset(buf, 0, sizeof(buf));
			ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_EXT_HW_DESC, 0, buf);
			if (ret <= 0 && i == 15)
				return -1;
			if (ret != 16){
			 return -1;
			}

			buf[ret] = 0;

			
			len = converttoint(buf + 1);
			strcpy(desc[len], (char *)buf);
		};

		descriptionavailable = true;
	}
	if (strlen(desc[Ch]) == 0) {
		return -1;
	}

	if (truncate){
		strcpy(Buffer, &(desc[Ch][4]));
	}

	else{
		strcpy(Buffer, desc[Ch]);
	}

	return 0;
}

int get_voltage(char ch,int stsize, const char **cmp, uint32_t* pValue)
{
	unsigned char k;
	int res;
	char buffer[33] , temp[33] = {0};
	memset(buffer, 0, sizeof(buffer));
	res = get_voltage_description_ext(ch, temp, true);
	if (res == 0)
	{
		unsigned char i, j;
		res = 0;
		for (i = 0, j = 0; i<strlen(temp); i++,j++)          
		{
			if (temp[i]!=' ')                           
				buffer[j] = temp[i];                     
			else
				j--;                                     
		}
		buffer[j] = 0;
			
	}
	else
	{
		get_voltage_description(ch, buffer);	
	}
	for (k=0 ;k < stsize; k++)
	{
		if (strcmp(buffer, cmp[k]) == 0)
		{
			int vol;
			get_voltage_value(ch, &vol);
			*pValue = (uint32_t)(vol);
			return 0;
		}
	}

	return -1;

}

static ssize_t board_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret, i,j;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_VERSION1, 4, (void *)buff);
	if (ret < 0)
		return ret;

	for (i = 0; i < 32; i++)
	{
		if(buff[i] == ' ')
			break;
	}

	for (j = 1; j < 32; j++)
	{
		if(buff[j + i] == ' '){
			buff[j + i] = 0;
			break;
		}
	}

        return sprintf(buf, "%s\n", buff + i + 1);
}
 

static ssize_t sysfs_show_error_log(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;

 	struct boarderrlog errlog={0};

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BOARDERRLOG, 20, (void *)&errlog);	
	if (ret < 0)
		return ret;


       	return sprintf(buf, "ErrorNumber: %hu\nFlags: 0x%x\nRestartEvent: 0x%x\nPowerCycle: %u\nBootCount: %u\nTime : %u\nStatus : 0x%x\nCPUTemp : %d\nBoardTemp : %d\n", errlog.errnum, errlog.flags, errlog.restartevent, errlog.pwrcycles, errlog.bootcnt, errlog.time, errlog.status, errlog.cputemp, errlog.boardtemp);
}

static ssize_t sysfs_store_error_log(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
        int ret, data;
	data = converttoint((char *)buf);
        debug_printk(KERN_INFO "Sysfs - Write!!\n");
        ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_GET_BOARDERRLOG, 1, (void *)&data);	
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t cur_pos_error_log_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
 	struct boarderrlog errlog={0};
 
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BOARDERRLOG, 20, (void *)&errlog);
	if (ret < 0)
		return ret;

       	return sprintf(buf, "ErrorNumber: %hu\nFlags: 0x%x\nRestartEvent: 0x%x\nPowerCycle: %u\nBootCount: %u\nTime : %u\nStatus: 0x%x\nCPUTemp : %d\nBoardTemp : %d\n", errlog.errnum, errlog.flags, errlog.restartevent, errlog.pwrcycles, errlog.bootcnt, errlog.time, errlog.status, errlog.cputemp, errlog.boardtemp);

}

static ssize_t sysfs_show_err_num_des(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int cnt, ret; 
	unsigned char buff[33];
	unsigned char buffer[33];
	unsigned char temp[6];
	char errcode;
	debug_printk(KERN_INFO "%s:Sysfs - Read!!!\n", __func__);

	memset(buff, 0, sizeof(buff));
	buff[0] = 0;
	ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_GET_BOARDERRLOG, 1, (void *)buff);	
	if (ret < 0)
		return ret;
	for (cnt = 0; cnt < 32; cnt ++)
	{
		unsigned short errnumcv;
		ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BOARDERRLOG, 20, (void *)buff);	
		if (ret < 0)
			return ret;

		errnumcv = (unsigned short)buff[0] | ((unsigned short)buff[1] << 8);

		if (errnum_desc == errnumcv)
		{
			errcode = (char)buff[2] & 0x0f;

			temp[0] = errcode;
	
			memset(buffer, 0, sizeof(buffer));
			ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_EXC_CODE_TABLE, 1, (void *)temp);	
			if (ret < 0)
				return ret;
	
			ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_EXC_CODE_TABLE, 18, (void *)buffer);	
			if (ret < 0)
				return ret;

		}
	}

	return sprintf(buf, "%s\n", buffer);
}

static ssize_t sysfs_store_err_num_des(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int data;
	debug_printk(KERN_INFO "Sysfs - Write!!!\n");
	data = converttoint((char *)buf);
	errnum_desc = data;

	return count;
}
static ssize_t sysfs_show_exc_des(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	
	unsigned char buff[32];
	memset(buff, 0, sizeof(buff));
        debug_printk(KERN_INFO "%s:Sysfs - Read!!!\n", __func__);
 
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_EXC_CODE_TABLE, 18, (void *)buff);	
	if (ret < 0)
		return ret;
   
        return sprintf(buf,"%s\n", buff);

}

static ssize_t sysfs_store_exc_des(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
        int ret, data;
	data = converttoint((char *)buf);
        ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_EXC_CODE_TABLE, 1, (void *)&data);	
	if (ret < 0)
		return ret;
	
	return count;
}


static ssize_t serial_number_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];

	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_SR_NO, 4, (void *)buff);
	if (ret < 0)
		return ret;

	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xff) || (buff[0] == ' '))
		return -EINVAL;

        return sprintf(buf, "%s\n", buff);
}
 
static ssize_t manufacturer_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
	sprintf(buff, "ADLINK Technology Inc.");

        return sprintf(buf, "%s\n", buff);
}

static ssize_t platform_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_PLATFORM_ID, 4, (void *)buff);
	if (ret < 0)
		return ret;

        return sprintf(buf, "%s\n", buff);
}

static ssize_t hw_rev_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_HW_REV, 4, (void *)buff);
	if (ret < 0)
		return ret;

	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xff) || (buff[0] == ' '))
		return -EINVAL;
        return sprintf(buf, "%s\n", buff);
}


static ssize_t bmc_application_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[64], *ptr;
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_VERSION1, 4, (void *)buff);
	if (ret < 0)
		return ret;
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_VERSION2, 4, (void *)(buff + strlen(buff)));
	if (ret < 0)
		return ret;
	if ((ptr = (char*)strchr((char*)buff, '(')))
	{
		*--ptr = 0;
	}
	else
		debug_printk("No Copyright string found (c), some thing wrong");
	
        return sprintf(buf, "%s\n", buff);
}

static ssize_t last_repair_date_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_LR_DATA, 4, (void *)buff);
	if (ret < 0)
		return ret;
	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xffffffff) || (buff[0] == ' '))
		return -EINVAL;

        return sprintf(buf, "%s\n", buff);
}

static ssize_t manufactured_date_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_MF_DATE, 4, (void *)buff);
	if (ret < 0)
		return ret;

	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xffffffff) || (buff[0] == ' '))
		return -EINVAL;

        return sprintf(buf, "%s\n", buff);
}

static ssize_t mac_address_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_MAC_ID, 4, (void *)buff);
	if (ret < 0)
		return ret;
	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xffffffff) || (buff[0] == ' '))
		return -EINVAL;
        
	return sprintf(buf, "%s\n", buff);
}

static ssize_t second_hw_rev_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_2HW_REV, 4, (void *)buff);
	if (ret < 0)
		return ret;

	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xffffffff) || (buff[0] == ' '))
		return -EINVAL;

        return sprintf(buf, "%s\n", buff);
}

static ssize_t second_ser_num_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	char buff[32];
	memset(buff, 0, sizeof(buff));
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MF_DATA_2SR_NO, 4, (void *)buff);
	if (ret < 0)
		return ret;

	if (((ret == 1) && (buff[0] == 0xf0)) || (buff[0] == 0xffffffff) || (buff[0] == ' '))
		return -EINVAL;

        return sprintf(buf, "%s\n", buff);
}

static ssize_t boot_counter_val_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret; 
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_BOOT_COUNTER_VAL, 4, (void *)buff);
	if (ret < 0)
		return ret;

	val = ((unsigned int)buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3]);

        return sprintf(buf, "%u\n", val);
}

static ssize_t total_up_time_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_TOM, 4, (void *)buff);
	if (ret < 0)
		return ret;

	val = ((unsigned int)buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3]);

        return sprintf(buf, "%u\n", val);
}

static ssize_t power_up_time_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_PWRUP_SECS, 4, (void *)buff);
	if (ret < 0)
		return ret;

	val = ((unsigned int)buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3]);

        return sprintf(buf, "%u\n", val);
}

static ssize_t restart_event_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned short val;
	memset(buff, 0, sizeof(buff));

	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_RESTARTEVT, 1, (void *)buff);
	if (ret < 0)
		return ret;

	val = (unsigned short)buff[0];

        return sprintf(buf, "%hu\n", val);
}

static ssize_t capabilities_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val[8];
	memset(buff, 0, sizeof(buff));
	memset(val, 0, sizeof(val));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_CAPABILITIES, 6, (void *)buff);
	if (ret < 0)
		return ret;

	CollectCapabilities(val, ret, buff);
		
        return sprintf(buf, "%u\n", *(val + 0));
}

static ssize_t capabilities_ext_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val[8];
	memset(buff, 0, sizeof(buff));
	memset(val, 0, sizeof(val));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_CAPABILITIES, 4, (void *)buff);
	if (ret < 0)
		return ret;

	CollectCapabilities(val, ret, buff);
		
        return sprintf(buf, "%u\n", *(val + 1));
}

static ssize_t main_current_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int power = 0;
	unsigned char buff[32];
	int ret;
	long mul = 0;

	memset(buff, 0, sizeof(buff));

	ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_MPCURRENT, 8, (void *)buff);
	if (ret < 0)
		return ret;

	mul = get_scale_factor(get_cur_channel());

	if (ret == 2){
		power = (unsigned short) ((mul * ((((unsigned short)buf[0]) << 8) | buf[1])) / 10 ) * 10;
	}

	if (ret == 8)
	{
		long Buf = 0, max = 0;
		unsigned int temp, i;
		for (i = 0; i < 4; i++)
		{	
			long tmp = 0;
			tmp = (((unsigned short)buff[i * 2]) << 8) | buff[i * 2 + 1];
			Buf += tmp;
			if (max < tmp)
				max = tmp;
		}

		Buf -= max;
		temp = (unsigned int)((330000 / 1024) * (mul));
		temp = temp / 1000;

		power = (unsigned int) ((((unsigned short)(Buf /3))) * temp);


		power = power / 100;
	}

	else
	{
		return sprintf(buf, "Error: Unexpecter number of bytes, Not supported\n");
	}		

	return sprintf(buf, "%u\n", power);
}

static ssize_t power_cycles_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_PWRCYCLES, 4, (void *)buff);
	if (ret < 0)
		return ret;

	val = ((unsigned int)buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3]);

        return sprintf(buf, "%u\n", val);
}

static ssize_t bmc_flags_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_BMC_FLAGS, 1, (void *)buff);
	if (ret < 0)
		return ret;

	val = (unsigned int)buff[0] ;

        return sprintf(buf, "%u\n", val);
}

static ssize_t bmc_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	
	unsigned int val;
	memset(buff, 0, sizeof(buff));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_BMC_STATUS, 4, (void *)buff);
	if (ret < 0)
		return ret;

	val = (unsigned int)buff[0];
        return sprintf(buf, "%u\n", val);
}

static ssize_t bmc_boot_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret, tmp;
	unsigned char buff[128];
	unsigned char buffer[32];
	
	memset(buff, 0, sizeof(buff));
	memset(buffer, 0, sizeof(buffer));

        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_BLVERSION, 0, (void *)buff);
	if (ret < 0)
		return ret;

	buff[ret] = 0;
 
        tmp = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_BLVERSION, 0, (void *)buffer);
	if (tmp < 0)
		return tmp;

	//buffer[tmp] = 0;

	if (strcmp ((unsigned char *)buff, (unsigned char *)buffer) != 0)
		strcat ((unsigned char *)buff, (unsigned char *)buffer);

	if (buff[0] == 0xf0)
	{
		buffer[0] = 0;
		return -1;
	}

        return sprintf(buf, "%s\n", buff);
}

static ssize_t restart_event_str_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
	unsigned char buff[32];
	unsigned char buffer[32];
	
	unsigned short val;
	memset(buff, 0, sizeof(buff));
	memset(buffer, 0, sizeof(buffer));

	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_RD_RESTARTEVT, 1, (void *)buff);
	if (ret < 0)
		return ret;

	val = (unsigned short)buff[0];

	switch (val)
	{
		case SRE_UNKNOWN:
			sprintf(buffer, "Unknown");
			break;
		case SRE_SW_RESET:
			sprintf(buffer, "Software Reset");
			break;
		case SRE_HW_RESET:
			sprintf(buffer, "Hardware Reset");
			break;
		case SRE_WATCHDOG:
			sprintf(buffer, "Watchdog");
			break;
		case SRE_BIOS_FAULT:
			sprintf(buffer, "BIOS fault");
			break;
		case SRE_POWER_DOWN:
			sprintf(buffer, "Power down");
			break;
		case SRE_POWER_LOSS:
			sprintf(buffer, "Power loss");
			break;
		case SRE_POWER_CYCLE:
			sprintf(buffer, "Power cycle");
			break;
		case SRE_VIN_DROP:
			sprintf(buffer, "Drop of main input voltage");
			break;
		case SRE_POWER_FAIL:
			sprintf(buffer, "Power fail");
			break;
		case SRE_CRIT_TEMP:
			sprintf(buffer, "Critical CPU shutdown");
			break;
		case SRE_WAKEUP:
			sprintf(buffer, "Wake up");
			break;
		default:
			sprintf(buffer, "Invalid value");
			break;
        }

	return sprintf(buf, "%s\n", buffer);
}

static ssize_t voltage_gfx_vcore_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"GFX-Vcore","VCC_GT","VGFX","VGG_S3","VNN_S","P_+VCCGT"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_1v05_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"1.050","V1P05S","V1P05_A","1.05","V1P05_S3"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_1v5_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"1.500","V1P5_S0","V1P5S","V1v5","P_+1V5_S"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;

       	return sprintf(buf, "%d\n", Value);
}
static ssize_t voltage_vin_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"VIN(12V)","VIN","V12","Q7","VSMARC","V12S","V5_ATX","P_+5V_ATX"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_vcore_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"CPU-Vcore", "VCC_CORE","VCORE","VCC_S","P_+VCORE"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_2v5_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"2.500", "2V5_VPP" };

	for(i = 0; i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_3v3_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"3.300","V3P3A","V3P3_A","3.3V","P_+3V3_A"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_vbat_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"VRTC","RTC"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_5v_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"5V","5.000","V5_DUAL","V5S","V5Vin","V5_S0","5V_DUAL","V5_S","P_+5V_ATX"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_5vsb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"V5SBY","V5_SBY","V5VSB","5VSB","P_+5V_ATX_SBY"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}

static ssize_t voltage_12v_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i, ret, Value = 0;
	const char *cmpstr[]={"VIN (12V)","12.000","V12","V12S","V12_V"};

	for(i = 0;i < ADL_MAX_HW_MTR_INPUT; ++i)
	{
		ret = get_voltage(i,sizeof(cmpstr)/sizeof(cmpstr[0]), cmpstr, &Value);
		if (ret == 0)
   			break;
	}
   
	if (ret)
		return -EINVAL;
        
	return sprintf(buf, "%d\n", Value);
}



struct kobj_attribute attr0 = __ATTR_RO(board_name);
struct kobj_attribute attr1 = __ATTR(error_log, 0660, sysfs_show_error_log, sysfs_store_error_log);
struct kobj_attribute attr2 = __ATTR_RO(cur_pos_error_log);
struct kobj_attribute attr3 = __ATTR(err_num_des, 0660, sysfs_show_err_num_des, sysfs_store_err_num_des);
struct kobj_attribute attr4 = __ATTR(exc_des, 0660, sysfs_show_exc_des, sysfs_store_exc_des);

struct kobj_attribute attr5 = __ATTR_RO(manufacturer_name);
struct kobj_attribute attr6 = __ATTR_RO(serial_number);
struct kobj_attribute attr7 = __ATTR_RO(platform_id);
struct kobj_attribute attr8 = __ATTR_RO(hw_rev);
struct kobj_attribute attr9 = __ATTR_RO(bmc_application_version);
struct kobj_attribute attr10 = __ATTR_RO(last_repair_date);
struct kobj_attribute attr11 = __ATTR_RO(manufactured_date);
struct kobj_attribute attr12 = __ATTR_RO(mac_address);
struct kobj_attribute attr13 = __ATTR_RO(second_hw_rev);
struct kobj_attribute attr14 = __ATTR_RO(second_ser_num);
struct kobj_attribute attr15 = __ATTR_RO(boot_counter_val);
struct kobj_attribute attr16 = __ATTR_RO(total_up_time);
struct kobj_attribute attr17 = __ATTR_RO(power_up_time);
struct kobj_attribute attr18 = __ATTR_RO(restart_event);
struct kobj_attribute attr19 = __ATTR_RO(capabilities);
struct kobj_attribute attr20 = __ATTR_RO(capabilities_ext);
struct kobj_attribute attr21 = __ATTR_RO(main_current);
struct kobj_attribute attr22 = __ATTR_RO(power_cycles);
struct kobj_attribute attr23 = __ATTR_RO(bmc_flags);
struct kobj_attribute attr24 = __ATTR_RO(bmc_status);

struct kobj_attribute attr25 = __ATTR_RO(bmc_boot_version);
struct kobj_attribute attr26 = __ATTR_RO(restart_event_str);

struct kobj_attribute attr27 = __ATTR_RO(voltage_gfx_vcore);
struct kobj_attribute attr28 = __ATTR_RO(voltage_1v05);
struct kobj_attribute attr29 = __ATTR_RO(voltage_1v5);
struct kobj_attribute attr30 = __ATTR_RO(voltage_vin);
struct kobj_attribute attr31 = __ATTR_RO(voltage_vcore);
struct kobj_attribute attr32 = __ATTR_RO(voltage_2v5);
struct kobj_attribute attr33 = __ATTR_RO(voltage_3v3);
struct kobj_attribute attr34 = __ATTR_RO(voltage_vbat);
struct kobj_attribute attr35 = __ATTR_RO(voltage_5v);
struct kobj_attribute attr36 = __ATTR_RO(voltage_5vsb);
struct kobj_attribute attr37 = __ATTR_RO(voltage_12v);



static int boardinfo_probe(struct platform_device *pdev)
{
	int ret;
	struct adl_bmc_dev *adl_dev;
        adl_dev = dev_get_drvdata(pdev->dev.parent);
	kobj_ref = kobject_create_and_add("information", &pdev->dev.kobj);
	ret = sysfs_create_file(kobj_ref, &attr0.attr); 

	if (ret < 0)
		goto ret_err;
        	
	/* check error log capability */
	if (adl_dev->Bmc_Capabilities[1] & ADL_BMC_ERR_LOG_CAP)
	{

		ret = sysfs_create_file(kobj_ref, &attr1.attr);
		if (ret < 0)
			goto ret_err;
		
		ret = sysfs_create_file(kobj_ref, &attr2.attr);
		if (ret < 0)
			goto ret_err;
		
		ret = sysfs_create_file(kobj_ref, &attr3.attr);
		if (ret < 0)
			goto ret_err;
        }
	
        ret = sysfs_create_file(kobj_ref, &attr4.attr); 
	if (ret < 0)
		goto ret_err;
      
	ret = sysfs_create_file(kobj_ref, &attr5.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr6.attr);
	if (ret < 0)
		goto ret_err;
        
        ret = sysfs_create_file(kobj_ref, &attr7.attr); 
	if (ret < 0)
		goto ret_err;

        ret = sysfs_create_file(kobj_ref, &attr8.attr); 
	if (ret < 0)
		goto ret_err;
   
	ret = sysfs_create_file(kobj_ref, &attr9.attr); 
	if (ret < 0)
		goto ret_err;

	ret = sysfs_create_file(kobj_ref, &attr10.attr); 
	if (ret < 0)
		goto ret_err;

	ret = sysfs_create_file(kobj_ref, &attr11.attr); 
	if (ret < 0)
		goto ret_err;

	ret = sysfs_create_file(kobj_ref, &attr12.attr); 
	if (ret < 0)
		goto ret_err;

	ret = sysfs_create_file(kobj_ref, &attr13.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr14.attr); 
	if (ret < 0)
		goto ret_err;

	/* check boot counter capability */
	if (adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_BOOT_COUNTER)
	{
		ret = sysfs_create_file(kobj_ref, &attr15.attr); 
		if (ret < 0)
			goto ret_err;
	}

	/* check up times and power cycle counter capability */
	if (adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_UPTIME)
	{
		ret = sysfs_create_file(kobj_ref, &attr16.attr); 
		if (ret < 0)
			goto ret_err;
		ret = sysfs_create_file(kobj_ref, &attr17.attr); 
		if (ret < 0)
			goto ret_err;
		ret = sysfs_create_file(kobj_ref, &attr22.attr); 
		if (ret < 0)
			goto ret_err;
	}

	/* check system restart event capability */
	if (adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_RESTSRTEVT)
	{
		ret = sysfs_create_file(kobj_ref, &attr18.attr); 
		if (ret < 0)
			goto ret_err;
		ret = sysfs_create_file(kobj_ref, &attr26.attr); 
		if (ret < 0)
			goto ret_err;
	}

	ret = sysfs_create_file(kobj_ref, &attr19.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr20.attr); 
	if (ret < 0)
		goto ret_err;

	/* check power monitor (current sense) capability */
	if (adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_CURRENTS)
	{
		ret = sysfs_create_file(kobj_ref, &attr21.attr); 
		if (ret < 0)
			goto ret_err;
	}

	ret = sysfs_create_file(kobj_ref, &attr23.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr24.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr25.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr27.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr28.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr29.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr30.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr31.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr32.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr33.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr34.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr35.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr36.attr); 
	if (ret < 0)
		goto ret_err;
	ret = sysfs_create_file(kobj_ref, &attr37.attr); 
	if (ret < 0)
		goto ret_err;
	debug_printk("%s:%d probe is done\n", __func__, __LINE__);

	return 0;

ret_err:
	kobject_put(kobj_ref);
	return ret;

}

static int boardinfo_remove(struct platform_device *pdev)
{
        sysfs_remove_file(kernel_kobj, &attr0.attr);
        sysfs_remove_file(kernel_kobj, &attr1.attr);
        sysfs_remove_file(kernel_kobj, &attr2.attr);
        sysfs_remove_file(kernel_kobj, &attr3.attr);
        sysfs_remove_file(kernel_kobj, &attr4.attr);
        sysfs_remove_file(kernel_kobj, &attr5.attr);
        sysfs_remove_file(kernel_kobj, &attr6.attr);
        sysfs_remove_file(kernel_kobj, &attr7.attr);
        sysfs_remove_file(kernel_kobj, &attr8.attr);
        sysfs_remove_file(kernel_kobj, &attr9.attr);
        sysfs_remove_file(kernel_kobj, &attr10.attr);
        sysfs_remove_file(kernel_kobj, &attr11.attr);
        sysfs_remove_file(kernel_kobj, &attr12.attr);
        sysfs_remove_file(kernel_kobj, &attr13.attr);
        sysfs_remove_file(kernel_kobj, &attr14.attr);
        sysfs_remove_file(kernel_kobj, &attr15.attr);
        sysfs_remove_file(kernel_kobj, &attr16.attr);
        sysfs_remove_file(kernel_kobj, &attr17.attr);
        sysfs_remove_file(kernel_kobj, &attr18.attr);
        sysfs_remove_file(kernel_kobj, &attr19.attr);
        sysfs_remove_file(kernel_kobj, &attr20.attr);
        sysfs_remove_file(kernel_kobj, &attr21.attr);
        sysfs_remove_file(kernel_kobj, &attr22.attr);
        sysfs_remove_file(kernel_kobj, &attr23.attr);
        sysfs_remove_file(kernel_kobj, &attr24.attr);
        sysfs_remove_file(kernel_kobj, &attr25.attr);
        sysfs_remove_file(kernel_kobj, &attr26.attr);
        sysfs_remove_file(kernel_kobj, &attr27.attr);
        sysfs_remove_file(kernel_kobj, &attr28.attr);
        sysfs_remove_file(kernel_kobj, &attr29.attr);
        sysfs_remove_file(kernel_kobj, &attr30.attr);
        sysfs_remove_file(kernel_kobj, &attr31.attr);
        sysfs_remove_file(kernel_kobj, &attr32.attr);
        sysfs_remove_file(kernel_kobj, &attr33.attr);
        sysfs_remove_file(kernel_kobj, &attr34.attr);
        sysfs_remove_file(kernel_kobj, &attr35.attr);
        sysfs_remove_file(kernel_kobj, &attr36.attr);
        sysfs_remove_file(kernel_kobj, &attr37.attr);
	kobject_put(kobj_ref);

	return 0;
}

static struct platform_driver adl_bmc_boardinfo_driver = {
	.driver = {
	    .name = "adl-bmc-boardinfo",
	},

	.probe = boardinfo_probe,
	.remove = boardinfo_remove,
	
};


module_platform_driver(adl_bmc_boardinfo_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("Board information driver");


