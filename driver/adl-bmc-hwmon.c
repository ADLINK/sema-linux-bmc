#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/version.h>

#include "adl-bmc.h"

struct adl_bmc_hwmon_data {
	struct device *hwmon_dev;
	struct adl_bmc_dev *adl_dev;
	u8 soft_fan; 			/* Soft fan-pwm with interpolation supported*/ 
	struct mutex update_lock;
};

#define SHOW_SET_FAN_ENABLE		1
#define SHOW_SET_FAN_AUTO_TEMP_SRC 	2	
/*
FAN Modes:
	00 = AUTO (SMART FAN)
	01 = OFF
	10 = ON
	11 = Soft Fan (Smart FAN with interpolation

Temperature source:
	0 = CPU Temperature
	1 = Board Temperature
*/

#define SHOW_FAN_INPUT	 0

#define SHOW_TEMP_INPUT 0
#define SHOW_TEMP_MIN 1
#define SHOW_TEMP_MAX 2
#define SHOW_TEMP_STARTUP 3
#define SHOW_TEMP_LABEL 4


#define KELVINS_OfFSET 2731

static unsigned char DtsTemp;

static ssize_t show_fan_enable_temp_src(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	unsigned char conf_data[32];
	int i, size;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	unsigned int value = 0, bmc_conf = 0;
	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int fn = sensor_attr_2->nr;
	int fan_num = sensor_attr_2->index;
	debug_printk("%s\n", __func__);
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_SYSCFG, 32, conf_data);
	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, conf_data[i]);
	}

	if (size < 0)
		return size;


	bmc_conf = conf_data[0] | (conf_data[1] << 8) | (conf_data[2] << 16) | (conf_data[3] << 24);

	debug_printk("BMC Configuration value =%x\n", bmc_conf);

	switch (fn) {
		case SHOW_SET_FAN_ENABLE:
			debug_printk("SHOW_SET_FAN_ENABLE\n");

			if (fan_num == 0) //cpu fan
			{
				value = (bmc_conf >> 9) & 0x3;//get bits 10 and 9
			}

			if (fan_num == 1) //system fan1
			{
				value = (bmc_conf >> 11) & 0x3;//get bits 12 and 11
			}

		
			if (fan_num == 2) //system fan2
			{
				value = (bmc_conf >> 17) & 0x3;//get bits 18 and 17
			}

			if (fan_num == 3) //system fan3
			{
				value = (bmc_conf >> 20) & 0x3;//get bits 21 and 20
			}
			break;

		case SHOW_SET_FAN_AUTO_TEMP_SRC:
			debug_printk("SHOW_SET_FAN_AUTO_TEMP_SRC\n");

			if (fan_num == 0) //cpu fan
			{
				value = (bmc_conf >> 8) & 0x1;//get bit 8
			}

			if (fan_num == 1) //system fan1
			{
				value = (bmc_conf >> 13) & 0x1;//get bit 13
			}

		
			if (fan_num == 2) //system fan2
			{
				value = (bmc_conf >> 16) & 0x1;//get bit 16
			}

			if (fan_num == 3) //system fan3
			{
				value = (bmc_conf >> 20) & 0x1;//get bit 19
			}
			break;
		default:
			return -EINVAL;
	}
	


	return sprintf(buf, "%u\n", value);
	
}



static ssize_t set_fan_enable_temp_src(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char conf_data[32];
	int err;
	int i;
	int size;
	unsigned long val;
	unsigned int bmc_conf;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);

	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int fan_num = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;

	debug_printk("%s\n", __func__);
	debug_printk("fan_num %d \n", fan_num );

        err = kstrtoul(buf, 10, &val);
        if (err < 0)
                return err;

	mutex_lock(&hwmon_data->update_lock);
	/*read the config register*/
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_SYSCFG, 32, conf_data);
	if (size < 0)
	{
		goto Exit;
	}

	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, conf_data[i]);
	}

	bmc_conf = conf_data[0] | (conf_data[1] << 8) | (conf_data[2] << 16) | (conf_data[3] << 24);

	debug_printk("BMC Configuration value =%x\n", bmc_conf);
	switch (fn) {
		case SHOW_SET_FAN_ENABLE:
			debug_printk("Inside SHOW_SET_FAN_ENABLE\n");
			if (val > 3)
			{
				size = -EINVAL;
				goto Exit;
			}


			if (hwmon_data->soft_fan == 0 && val == 3)
			{
				size = -EINVAL;
				goto Exit;
			}
			if (fan_num == 0) //cpu fan
			{
				bmc_conf &= ~(0x3 << 9);//clear bits 10 and 9
				bmc_conf |= (val << 9);
			}

			if (fan_num == 1) //system fan1
			{
				bmc_conf &= ~(0x3 << 11);//clear bits 12 and 11
				bmc_conf |= (val << 11);
			}

		
			if (fan_num == 2) //system fan2
			{
				bmc_conf &= ~(0x3 << 17);//clear bits 18 and 17
				bmc_conf |= (val << 17);
			}

			if (fan_num == 3) //system fan3
			{
				bmc_conf &= ~(0x3 << 20);//clear bits 21 and 20
				bmc_conf |= (val << 20);
			}
			break;

		case SHOW_SET_FAN_AUTO_TEMP_SRC:
			debug_printk("Inside SHOW_SET_PWM_AUTO_TEMP_SRC\n");
			if (val > 1)
			{
				size = -EINVAL;
				goto Exit;
			}

			if (fan_num == 0) //cpu fan
			{
				bmc_conf &= ~(0x1 << 8);//clear bit 8
				bmc_conf |= (val << 8);
			}

			if (fan_num == 1) //system fan1
			{
				bmc_conf &= ~(0x1 << 13);//clear bit 13
				bmc_conf |= (val << 13);
			}

		
			if (fan_num == 2) //system fan2
			{
				bmc_conf &= ~(0x1 << 16);//clear bit 16
				bmc_conf |= (val << 16);
			}

			if (fan_num == 3) //system fan3
			{
				bmc_conf &= ~(0x1 << 20);//clear bit 19
				bmc_conf |= (val << 20);
			}
			break;
		default:
			size = -EINVAL;
			goto Exit;
	}
	
			
	conf_data[0] = bmc_conf & 0xff;
	conf_data[1] = (bmc_conf >> 8) & 0xff;
	conf_data[2] = (bmc_conf >> 16) & 0xff;
	conf_data[3] = (bmc_conf >> 24) & 0xff;
	size = adl_bmc_i2c_write_device(hwmon_data->adl_dev, ADL_BMC_CMD_SYSCFG, 4, conf_data);
	if (size < 0)
		goto Exit;
	
	size = count;
Exit:
	mutex_unlock(&hwmon_data->update_lock);
	return size;
}


/*
The BMC supports maximum 4 PWMs 
Each PWM can be set for 4 temperature trigger points for SMART fan/SMART fan with interpolation mode. 
And also, a corresponding PWM to be set. 

show_pwm_auto_point_temp - shows Temperature trigger points for a PWM
set_pwm_auto_point_temp  - sets Temperature trigger points for a PWM

show_pwm_auto_point_pwm	 - shows PWM trigger points for a PWM
set_pwm_auto_point_pwm   - sets PWM trigger points for a PWM
*/

static ssize_t show_fan_auto_point_temp(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u8 trig_temp[32];
	int i;
	int size;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);

	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int temp_index = sensor_attr_2->index;
	int fan_num = sensor_attr_2->nr;
	unsigned char cmd;

	debug_printk("%s fan_num: %d, temp_index: %d\n", __func__, fan_num, temp_index);
	
	switch (fan_num) {
		case 0: //cpu fan
		{
			cmd = ADL_BMC_CPU_FAN_TEMP_THRE_REG;
			break;
		}

		case 1: //system fan1
		{
			cmd = ADL_BMC_SYS_FAN1_TEMP_THRE_REG;
			break;
		}


		case 2: //system fan2
		{
			cmd = ADL_BMC_SYS_FAN2_TEMP_THRE_REG;
			break;
		}

		case 3: //system fan3
		{
			cmd = ADL_BMC_SYS_FAN3_TEMP_THRE_REG;
			break;
		}
		default:
			return -EINVAL;
	}


	/*read the default temperature values*/
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, cmd, 32, trig_temp);
	if (size < 0)
		return size;

	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, trig_temp[i]);
	}

	return sprintf(buf, "%d\n", trig_temp[temp_index]);

}

static ssize_t set_fan_auto_point_temp(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	u8 trig_temp[32];
	int err;
	int i;
	int size;
	long val;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);

	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int temp_index = sensor_attr_2->index;
	int fan_num = sensor_attr_2->nr;
	unsigned char cmd;

	debug_printk("%s fan_num: %d, temp_index: %d \n", __func__, fan_num, temp_index);
	debug_printk("fan_num %d,  user input is %s\n", fan_num , buf);

        err = kstrtol(buf, 10, &val);
        if (err < 0)
	{
		debug_printk("Error in kstrtoul %ld\n", val);
                return err;
	}

	debug_printk("User input value is %ld\n", val);
	if ((val > 128) || (val < -127)) //check temperature range
	{
		debug_printk("Error invalid value provided\n");
		return -EINVAL;
	}

	
	switch (fan_num) {
		case 0: //cpu fan
		{
			cmd = ADL_BMC_CPU_FAN_TEMP_THRE_REG;
			break;
		}

		case 1: //system fan1
		{
			cmd = ADL_BMC_SYS_FAN1_TEMP_THRE_REG;
			break;
		}


		case 2: //system fan2
		{
			cmd = ADL_BMC_SYS_FAN2_TEMP_THRE_REG;
			break;
		}

		case 3: //system fan3
		{
			cmd = ADL_BMC_SYS_FAN3_TEMP_THRE_REG;
			break;
		}
		default:
			return -EINVAL;
	}

	mutex_lock(&hwmon_data->update_lock);
	/*read the temparature values*/
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, cmd, 32, trig_temp);
	if (size < 0)
	{
		debug_printk("Invalid size\n");
		goto Exit;
	}

	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, trig_temp[i]);
	}


	trig_temp[temp_index] = (unsigned char)val;
	debug_printk("Updated trigger temp is %d\n", trig_temp[temp_index]);
	size = adl_bmc_i2c_write_device(hwmon_data->adl_dev, cmd, 4, trig_temp);
	if (size < 0)
	{
		goto Exit;
	}
	size = count;

Exit:
	mutex_unlock(&hwmon_data->update_lock);
	return size;
}


static ssize_t show_fan_auto_point_pwm(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	u8 trig_pwm[32];
	int i;
	int size;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);

	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int pwm_index = sensor_attr_2->index;
	int fan_num = sensor_attr_2->nr;
	unsigned char cmd;

	debug_printk("%s fan_num: %d, pwm_index: %d\n", __func__, fan_num, pwm_index);
	
	switch (fan_num) {
		case 0: //cpu fan
		{
			cmd = ADL_BMC_CPU_FAN_PWM_THRE_REG;
			break;
		}

		case 1: //system fan1
		{
			cmd = ADL_BMC_SYS_FAN1_PWM_THRE_REG;
			break;
		}


		case 2: //system fan2
		{
			cmd = ADL_BMC_SYS_FAN2_PWM_THRE_REG;
			break;
		}

		case 3: //system fan3
		{
			cmd = ADL_BMC_SYS_FAN3_PWM_THRE_REG;
			break;
		}
		default:
			return -EINVAL;
	}


	/*read the default temperature values*/
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, cmd, 32, trig_pwm);
	if (size < 0)
		return size;

	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, trig_pwm[i]);
	}

	return sprintf(buf, "%d\n", trig_pwm[pwm_index]);
}

static ssize_t set_fan_auto_point_pwm(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	u8 trig_pwm[32];
	int err;
	int i;
	int size;
	unsigned long val;
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);

	struct sensor_device_attribute_2 *sensor_attr_2 =
						to_sensor_dev_attr_2(attr);
	int pwm_index = sensor_attr_2->index;
	int fan_num = sensor_attr_2->nr;
	unsigned char cmd;

	debug_printk("%s fan_num: %d, pwm_index: %d\n", __func__, fan_num, pwm_index);

        err = kstrtoul(buf, 10, &val);
        if (err < 0)
                return err;

	if (val > 100) //check temperature range
		return -EINVAL;

	
	switch (fan_num) {
		case 0: //cpu fan
		{
			cmd = ADL_BMC_CPU_FAN_PWM_THRE_REG;
			break;
		}

		case 1: //system fan1
		{
			cmd = ADL_BMC_SYS_FAN1_PWM_THRE_REG;
			break;
		}


		case 2: //system fan2
		{
			cmd = ADL_BMC_SYS_FAN2_PWM_THRE_REG;
			break;
		}

		case 3: //system fan3
		{
			cmd = ADL_BMC_SYS_FAN3_PWM_THRE_REG;
			break;
		}
		default:
			return -EINVAL;
	}

	mutex_lock(&hwmon_data->update_lock);
	/*read the pwm values*/
	size = adl_bmc_i2c_read_device(hwmon_data->adl_dev, cmd, 32, trig_pwm);
	if (size < 0) {
		goto Exit;
	}

	for (i=0;i<32;i++)
	{
		debug_printk("read_data i=%d, val=%x\n", i, trig_pwm[i]);
	}


	trig_pwm[pwm_index] = (unsigned char)val;
	size = adl_bmc_i2c_write_device(hwmon_data->adl_dev, cmd, 4, trig_pwm);
	if (size < 0){
		goto Exit;
	}
	
	size = count;
Exit: 
	mutex_unlock(&hwmon_data->update_lock);
	return size;	
}


#define SENSOR_ATTR_FAN(fan_num) \
	SENSOR_ATTR_2(fan##fan_num##_enable, S_IRUGO | S_IWUSR, \
		show_fan_enable_temp_src, set_fan_enable_temp_src, SHOW_SET_FAN_ENABLE, fan_num-1), \
	SENSOR_ATTR_2(fan##fan_num##_auto_channels_temp, S_IRUGO | S_IWUSR, \
		show_fan_enable_temp_src, set_fan_enable_temp_src, SHOW_SET_FAN_AUTO_TEMP_SRC, fan_num-1)

#define SENSOR_ATTR_FAN_AUTO_POINT_TEMP(fan_num, temp_pt) \
	SENSOR_ATTR_2(fan##fan_num##_auto_point##temp_pt##_temp, S_IRUGO | S_IWUSR, \
		show_fan_auto_point_temp, set_fan_auto_point_temp, \
		fan_num-1, temp_pt-1)

#define SENSOR_ATTR_FAN_AUTO_POINT_PWM(fan_num, pwm_pt) \
	SENSOR_ATTR_2(fan##fan_num##_auto_point##pwm_pt##_pwm, S_IRUGO | S_IWUSR, \
		show_fan_auto_point_pwm, set_fan_auto_point_pwm, \
		fan_num-1, pwm_pt-1)


//CPU FAN 
static struct sensor_device_attribute_2 adl_bmc_sysfs_cpu_fan_pwm[] = {
        SENSOR_ATTR_FAN(1),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(1, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(1, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(1, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(1, 4),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(1, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(1, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(1, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(1, 4),
};

//System FAN 1
static struct sensor_device_attribute_2 adl_bmc_sysfs_sys_fan1_pwm[] = {
        SENSOR_ATTR_FAN(2),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(2, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(2, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(2, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(2, 4),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(2, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(2, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(2, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(2, 4),

};

//System FAN 2
static struct sensor_device_attribute_2 adl_bmc_sysfs_sys_fan2_pwm[] = {
        SENSOR_ATTR_FAN(3),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(3, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(3, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(3, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(3, 4),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(3, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(3, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(3, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(3, 4),
};

//System FAN 3
static struct sensor_device_attribute_2 adl_bmc_sysfs_sys_fan3_pwm[] = {
        SENSOR_ATTR_FAN(4),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(4, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(4, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(4, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_TEMP(4, 4),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(4, 1),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(4, 2),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(4, 3),
        SENSOR_ATTR_FAN_AUTO_POINT_PWM(4, 4),
};



static int encode_celcius(char temp)
{
	int tem;
	tem = temp * 10 + KELVINS_OfFSET;
	return tem;
}

static ssize_t show_fan_input(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	int ret;
	unsigned char buff[32];

	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr_2 = to_sensor_dev_attr_2(attr);
	
	unsigned short speed;
	int ix = sensor_attr_2->index;

	memset(buff, 0, sizeof(buff)); 
	switch(ix) 
	{
		case 0:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_CPU_FAN, 0, buff);
			if(ret < 0)
				return ret;
			msleep(40);
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_CPU_FAN, 0, buff);
			if(ret < 0)
				return ret;

			break;
		case 1:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_1, 0, buff);
			if(ret < 0)
				return ret;
			msleep(40);
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_1, 0, buff);
			if(ret < 0)
				return ret;
			break;
		case 2:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_2, 0, buff);
			if(ret < 0)
				return ret;
			msleep(40);
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_2, 0, buff);
			if(ret < 0)
				return ret;
			break;

		case 3:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_3, 0, buff);
			if(ret < 0)
				return ret;
			msleep(40);
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_FAN_3, 0, buff);
			if(ret < 0)
				return ret;
			break;
		default:
			debug_printk(KERN_INFO "Index is not Matcing\n");
			break;
	}

	debug_printk("bufer: %d %d\n", buff[0], buff[1]);	
	speed = (((unsigned short)buff[0]) << 8 | buff[1]);
	return sprintf(buf, "%hu\n", speed);
}
static ssize_t show_temp_input(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	int ret;
	char buff[32];

	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr_2 = to_sensor_dev_attr_2(attr);
	
	unsigned short temper = 0;
	int ix = sensor_attr_2->index;

	memset(buff, 0, sizeof(buff)); 
	switch(ix) 
	{
		case 0:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_CPU_TEMP, 2, buff);
			debug_printk("return value buf: %d ret: %d\n", buff[0], ret);
			if(ret < 0)
				return ret;
			
			temper = (unsigned short)buff[0];
			break;
		case 1:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[0]);
			break;
		case 2:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM2_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[0]);
			break;

		default:
			debug_printk(KERN_INFO "Index is not Matcing\n");
			break;
	}
	
		
	return sprintf(buf, "%hu\n", temper);
}

static ssize_t show_temp_min(struct device *dev, struct device_attribute *attr, char *buf)
{

	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int ret;
	unsigned short temper = 0;
	char buff[32];

	memset(buff, 0, sizeof(buff)); 
	switch(ix) 
	{
		case 0:
			if(DtsTemp)
				return -EINVAL;
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[1]);
			break;
		case 1:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[3]);
			break;
		case 2:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM2_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[3]);
			break;

		default:
			debug_printk(KERN_INFO "Index is not Matcing\n");
			break;
	}
	
		
	return sprintf(buf, "%hu\n", temper);
}


static ssize_t show_temp_max(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int ret;
	unsigned short temper = 0;
	char buff[32];

	memset(buff, 0, sizeof(buff)); 
	switch(ix) 
	{
		case 0:
			if(DtsTemp)
				return -EINVAL;
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[0]);
			break;
		case 1:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[2]);
			break;
		case 2:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM2_MINMAX_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[2]);
			break;

		default:
			debug_printk(KERN_INFO "Index is not Matcing\n");
			break;
	}
	
		
	return sprintf(buf, "%hu \n", temper);

	
}

static ssize_t show_temp_startup(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	struct adl_bmc_hwmon_data *hwmon_data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int ret;
	unsigned int temper = 0;
	char buff[32];
	memset(buff, 0, sizeof(buff)); 
	switch(ix) 
	{
		case 0:
			if(DtsTemp)
				return -EINVAL;
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_STARTUP_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[0]);
			break;
		case 1:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_STARTUP_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[1]);
			break;
		case 2:
			ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_SYSTEM2_STARTUP_TEMP, 1, buff);
			if(ret < 0)
				return ret;
			temper = encode_celcius(buff[1]);
			break;

		default:
			debug_printk(KERN_INFO "Index is not Matcing\n");
			break;
	}
	
	return sprintf(buf, "%u \n", temper);
}



#define SENSOR_ATTR_FAN_CPU(ix) \
       SENSOR_ATTR_2(cpu_fan_speed, S_IRUGO, \
                show_fan_input, NULL, SHOW_FAN_INPUT, ix-1)

#define SENSOR_ATTR_FAN_SYS1(ix) \
       SENSOR_ATTR_2(sys1_fan_speed, S_IRUGO, \
                show_fan_input, NULL, SHOW_FAN_INPUT, ix-1)

#define SENSOR_ATTR_FAN_SYS2(ix) \
       SENSOR_ATTR_2(sys2_fan_speed, S_IRUGO, \
                show_fan_input, NULL, SHOW_FAN_INPUT, ix-1)

#define SENSOR_ATTR_FAN_SYS3(ix) \
       SENSOR_ATTR_2(sys3_fan_speed, S_IRUGO, \
                show_fan_input, NULL, SHOW_FAN_INPUT, ix-1)

#define SENSOR_ATTR_TEMP_CPU(ix) \
       SENSOR_ATTR_2(cpu_cur_temp, S_IRUGO, \
                show_temp_input, NULL, SHOW_TEMP_INPUT, ix-1),\
       SENSOR_ATTR_2(cpu_min_temp, S_IRUGO, \
                show_temp_min, NULL, SHOW_TEMP_MIN, ix-1), \
       SENSOR_ATTR_2(cpu_max_temp, S_IRUGO, \
                show_temp_max, NULL, SHOW_TEMP_MAX, ix-1), \
       SENSOR_ATTR_2(cpu_startup_temp, S_IRUGO, \
                show_temp_startup, NULL, SHOW_TEMP_STARTUP, ix-1), \

#define SENSOR_ATTR_TEMP_SYSTEM1(ix) \
       SENSOR_ATTR_2(sys1_cur_temp, S_IRUGO, \
                show_temp_input, NULL, SHOW_TEMP_INPUT, ix-1),\
       SENSOR_ATTR_2(sys1_min_temp, S_IRUGO, \
                show_temp_min, NULL, SHOW_TEMP_MIN, ix-1), \
       SENSOR_ATTR_2(sys1_max_temp, S_IRUGO, \
                show_temp_max, NULL, SHOW_TEMP_MAX, ix-1), \
       SENSOR_ATTR_2(sys1_startup_temp, S_IRUGO, \
                show_temp_startup, NULL, SHOW_TEMP_STARTUP, ix-1), \


#define SENSOR_ATTR_TEMP_SYSTEM2(ix) \
       SENSOR_ATTR_2(sys2_cur_temp, S_IRUGO, \
                show_temp_input, NULL, SHOW_TEMP_INPUT, ix-1),\
       SENSOR_ATTR_2(sys2_min_temp, S_IRUGO, \
                show_temp_min, NULL, SHOW_TEMP_MIN, ix-1), \
       SENSOR_ATTR_2(sys2_max_temp, S_IRUGO, \
                show_temp_max, NULL, SHOW_TEMP_MAX, ix-1), \
       SENSOR_ATTR_2(sys2_startup_temp, S_IRUGO, \
                show_temp_startup, NULL, SHOW_TEMP_STARTUP, ix-1), \

//Temperatures 
static struct sensor_device_attribute_2 adl_bmc_sysfs_cpu_temp[] = {
        SENSOR_ATTR_TEMP_CPU(1)
};

static struct sensor_device_attribute_2 adl_bmc_sysfs_board_temp[] = {
        SENSOR_ATTR_TEMP_SYSTEM1(2)
};
static struct sensor_device_attribute_2 adl_bmc_sysfs_board_second_temp[] = {
        SENSOR_ATTR_TEMP_SYSTEM2(3)
};

//Fan 
static struct sensor_device_attribute_2 adl_bmc_sysfs_cpu_fan[] = {
        SENSOR_ATTR_FAN_CPU(1)
};

static struct sensor_device_attribute_2 adl_bmc_sysfs_system_fan1[] = {
        SENSOR_ATTR_FAN_SYS1(2)
};
static struct sensor_device_attribute_2 adl_bmc_sysfs_system_fan2[] = {
        SENSOR_ATTR_FAN_SYS2(3)
};

static struct sensor_device_attribute_2 adl_bmc_sysfs_system_fan3[] = {
        SENSOR_ATTR_FAN_SYS3(4)
};


static void adl_bmc_hwmon_remove_sysfs(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i;
	struct adl_bmc_hwmon_data *hwmon_data;
	hwmon_data = platform_get_drvdata(pdev);


        if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_FAN_CPU)
	{	
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_fan_pwm); i++) {
			device_remove_file(dev,
				&adl_bmc_sysfs_cpu_fan_pwm[i].dev_attr);
		}
	}

        if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN1_CAP)
	{	
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan1_pwm); i++) {
			device_remove_file(dev,
				&adl_bmc_sysfs_sys_fan1_pwm[i].dev_attr);
		}
	}

	
        if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN2_CAP)
	{	
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan2_pwm); i++) {
			device_remove_file(dev,
				&adl_bmc_sysfs_sys_fan2_pwm[i].dev_attr);
		}
	}


	
        if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN3_CAP)
	{	
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan3_pwm); i++) {
			device_remove_file(dev,
				&adl_bmc_sysfs_sys_fan3_pwm[i].dev_attr);
		}
	}

	/*Remove sysfs entry for tempearature*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_TEMP) 
	{
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_temp); i++)
		{
			device_remove_file(&pdev->dev, &adl_bmc_sysfs_cpu_temp[i].dev_attr);
		}
	}

	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_TEMP)
	{

		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_board_temp); i++)
		{
			device_remove_file(&pdev->dev, &adl_bmc_sysfs_board_temp[i].dev_attr);
		}
	}
 
	if (hwmon_data->adl_dev->Bmc_Capabilities[1] & ADL_BMC_CAP_TEMP_1)
	{

		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_board_second_temp); i++)
		{
			device_remove_file(&pdev->dev, &adl_bmc_sysfs_board_second_temp[i].dev_attr);
		}
	}

	/*Remove sysfs entry for fan*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_FAN_CPU) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_fan); i++)
		{
			 device_remove_file(&pdev->dev, &adl_bmc_sysfs_cpu_fan[i].dev_attr);
		}
	}

	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN1_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan1); i++)
		{
			device_remove_file(&pdev->dev, &adl_bmc_sysfs_system_fan1[i].dev_attr);
		}
	}

	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN2_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan2); i++)
		{
			device_remove_file(&pdev->dev, &adl_bmc_sysfs_system_fan2[i].dev_attr);
		}
	}

	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN3_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan3); i++)
		{
			 device_remove_file(&pdev->dev, &adl_bmc_sysfs_system_fan3[i].dev_attr);
		}
	}

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
static umode_t adl_is_visible(const void *data, enum hwmon_sensor_types type,
			       u32 attr, int channel)
{
	return 0;
}
static const struct hwmon_ops adl_hwmon_ops = {
	.is_visible = adl_is_visible,
};

static const struct hwmon_channel_info *adl_info[] = {
	HWMON_CHANNEL_INFO(chip,
			   HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_MAX_HYST),
	NULL
};

static struct hwmon_chip_info adl_chip_info = {
	.ops = &adl_hwmon_ops,
	.info = adl_info,
};
#endif

static int adl_bmc_hwmon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct adl_bmc_hwmon_data *hwmon_data;
	int i, err;

	hwmon_data = devm_kzalloc(dev, sizeof(struct adl_bmc_hwmon_data), GFP_KERNEL);
	if (!hwmon_data)
		return -ENOMEM;
	platform_set_drvdata(pdev, hwmon_data);

	hwmon_data->adl_dev = dev_get_drvdata(pdev->dev.parent);

	mutex_init(&hwmon_data->update_lock);
	/* Create sysfs interface based on the capability */

	/*check CPU fan capability*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_FAN_CPU) 
	{
		debug_printk("CPU fan present\n");
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_fan_pwm); i++) {
			err = device_create_file(dev,
				&adl_bmc_sysfs_cpu_fan_pwm[i].dev_attr);
			if (err)
				goto EXIT_DEV_REMOVE;
		}
	}
		

	/*check system fan1 capability*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN1_CAP) 
	{
		debug_printk("System fan1 present\n");
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan1_pwm); i++) {
			err = device_create_file(dev,
				&adl_bmc_sysfs_sys_fan1_pwm[i].dev_attr);
			if (err)
				goto EXIT_DEV_REMOVE;
		}
	}

	/*check system fan2 capability*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN2_CAP) 
	{
		debug_printk("System fan2 present\n");
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan2_pwm); i++) {
			err = device_create_file(dev,
				&adl_bmc_sysfs_sys_fan2_pwm[i].dev_attr);
			if (err)
				goto EXIT_DEV_REMOVE;
		}
	}

	/*check system fan3 capability*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN3_CAP) 
	{
		debug_printk("System fan3 present\n");
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_sys_fan3_pwm); i++) {
			err = device_create_file(dev,
				&adl_bmc_sysfs_sys_fan3_pwm[i].dev_attr);
			if (err)
				goto EXIT_DEV_REMOVE;
		}
	}

	/*check CPU temperature capability and create sysfs entry for CPU tempearature*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_TEMP) 
	{
		char buff[32];
		int ret;
		/* Test for CPU Temperature*/
		memset(buff, 0, sizeof(buff));
		ret = adl_bmc_i2c_read_device(hwmon_data->adl_dev, ADL_BMC_CMD_RD_CPU_TEMP, 1, buff);
		if(ret < 0)
			return ret;
		if (ret == 2) {
			DtsTemp = 0;

			for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_temp); i++)
			{
				err = device_create_file(&pdev->dev, &adl_bmc_sysfs_cpu_temp[i].dev_attr);
				if (err)
					dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
			}

		}

		else 
			DtsTemp = 1;


	}


	/*check board temperature capability and create sysfs entry for board tempearature*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_TEMP) 
	{
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_board_temp); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_board_temp[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}

	/*check board 2nd temperature capability and create sysfs entry for board 2nd tempearature*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[1] & ADL_BMC_CAP_TEMP_1) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_board_second_temp); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_board_second_temp[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}

	/*check CPU fan capability and create sysfs entry for CPU fan*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_FAN_CPU) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_cpu_fan); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_cpu_fan[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}

	/*check system fan 1 apability and create sysfs entry for sysyem fan 1*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN1_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan1); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_system_fan1[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}

	/*check system fan 2 apability and create sysfs entry for sysyem fan 2*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN2_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan2); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_system_fan2[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}

	/*check system fan 3 apability and create sysfs entry for sysyem fan 3*/
	if (hwmon_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_SYS_FAN3_CAP) 
	{ 
		for (i = 0; i < ARRAY_SIZE(adl_bmc_sysfs_system_fan3); i++)
		{
			err = device_create_file(&pdev->dev, &adl_bmc_sysfs_system_fan3[i].dev_attr);
			if (err)
				dev_err(&pdev->dev, "Creation of sysfs entry failed %d\n", err);
		}
	}


	/* Register device */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
		hwmon_data->hwmon_dev = hwmon_device_register_with_info(dev,"adl_bmc_hwmon",NULL,&adl_chip_info,NULL);
	#else
		hwmon_data->hwmon_dev = hwmon_device_register(dev);
	#endif

	if (IS_ERR(hwmon_data->hwmon_dev)) {
		err = PTR_ERR(hwmon_data->hwmon_dev);
		dev_err(dev, "Class registration failed (%d)\n", err);
		goto EXIT_DEV_REMOVE_SILENT;
	}

	/*Check PWM with interpolation capability is supported */
	if (hwmon_data->adl_dev->Bmc_Capabilities[1] & ADL_BMC_SOFT_FAN_PWM_INTERPOLATION_CAP)
	{
		debug_printk("PWM interpolation capability present\n");
		hwmon_data->soft_fan = 1;
	}
	else
	{
		debug_printk("PWM interpolation capability not present\n");
		hwmon_data->soft_fan = 0;
	}
		
	debug_printk("adl_cap in fan driver: 0x%x\n", hwmon_data->adl_dev->Bmc_Capabilities[0]);

	return 0;

EXIT_DEV_REMOVE:
	dev_err(dev, "ADL BMC FAN Sysfs interface creation failed (%d)\n", err);
EXIT_DEV_REMOVE_SILENT:
	adl_bmc_hwmon_remove_sysfs(pdev);
	return err;
}

static int adl_bmc_hwmon_remove(struct platform_device *pdev)
{
	struct adl_bmc_hwmon_data *hwmon_data = platform_get_drvdata(pdev);

	hwmon_device_unregister(hwmon_data->hwmon_dev);
	adl_bmc_hwmon_remove_sysfs(pdev);

	devm_kfree(&pdev->dev, hwmon_data);
	return 0;
}

static struct platform_driver adl_bmc_hwmon_driver = {
	.driver = {
		.name  = "adl-bmc-hwmon",
	},
	.probe  = adl_bmc_hwmon_probe,
	.remove = adl_bmc_hwmon_remove,
};


module_platform_driver(adl_bmc_hwmon_driver);

MODULE_AUTHOR("ADLINK");
MODULE_DESCRIPTION("ADLINK BMC FAN-PWM Driver");
MODULE_LICENSE("Dual BSD/GPL");


