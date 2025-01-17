#include <linux/err.h>
#include <linux/of.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/version.h>

#include "adl-bmc.h"

struct kobject *kobj_ref;

struct adl_bmc_vm_data *vm_data;

struct adl_bmc_vm_data {
	struct regulator_desc adl_bmc_vm_desc[16];
	struct adl_bmc_dev *adl_dev;
	char name_arr[16][256];
	char adlink_name[16][256];
	int cnt;

};

#define GET_VOLT_AND_DESC	_IOWR('a','1',struct data *)
#define GET_VOLT_MONITOR_CAP	_IOWR('a','2',uint8_t *)

int vm_dev,flag,vm_cap;
struct class *class_adl_vm;
struct cdev cdev;
dev_t devdrv;
char volt_desc[16][256];

struct data{
	int id;
	int volt;
	char volt_desc[256];
};

static int voltagenum = 0;
unsigned int voltagevalue[32] = {0};

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

static int adl_bmc_vm_get_voltage(struct data *vm)
{
	int ret = 0;
        static unsigned char voltnum=0;
	unsigned char buff_hm[32] = {0};
	unsigned char buff_gain[32] = {0};
	unsigned char cmd_hm;
	u64 vol_val, vol_val_fl;
	unsigned short gain;

	debug_printk("%s id: %d \n", __FUNCTION__, vm->id);
	
	if (vm->id < 8) {
		cmd_hm = ADL_BMC_CMD_RD_AIN0 + vm->id;
	}
	else if (vm->id >= 8 && vm->id < 16) {
		cmd_hm = ADL_BMC_CMD_RD_AIN8 + (vm->id - 8);
	}
	else
		return -EINVAL;

	debug_printk("I2C commmand: 0x%x\n", cmd_hm);

	
	/*read the hwmon register for voltage*/
        ret = adl_bmc_i2c_read_device(vm_data->adl_dev, cmd_hm, 2, buff_hm);
	if(ret != 2)
		return -EINVAL;

	msleep(30);
	/*read the gain value*/
        ret = adl_bmc_i2c_read_device(vm_data->adl_dev, ADL_BMC_CMD_GET_ADC_SCALE, 32, buff_gain);
	if (ret < 0)
		return ret;
	gain = (buff_gain[vm->id * 2] << 8) | buff_gain[(vm->id * 2) + 1];
	vol_val_fl = 3223 * gain;
	vol_val = vol_val_fl * (buff_hm[0] << 8 | buff_hm[1]);
	vol_val = vol_val / 1000000;
        voltagevalue[voltnum] = vol_val;
        voltnum++;

	vm->volt=vol_val;
	strcpy(vm->volt_desc,volt_desc[vm->id]);
	return 0;
}

static struct regulator_init_data adl_bmc_initdata = {
	.constraints = {
		.always_on = 1,
	},
};

int open(struct inode *inode, struct file *file)
{
	if(flag==0)
	{
		flag=1;
		return 0;
	}
	else
	{
		return -EBUSY;
	}
	return 0;
}

int release(struct inode *inode , struct file *file)
{
	flag=0;
	return 0;
}

static long int ioctl (struct file *file, unsigned cmd, unsigned long arg)
{
	struct data vm;
	int RetVal,Cap;

	switch(cmd)
	{
		case GET_VOLT_AND_DESC:
		{
			if((RetVal = copy_from_user(&vm,(struct data *)arg,sizeof(vm)))!=0)
			{
				return -EFAULT;
			}
			RetVal=adl_bmc_vm_get_voltage(&vm);
			if((RetVal = copy_to_user((struct data*) arg,&vm,sizeof(vm)))!=0)
			{
				return -EFAULT;
			}
		}
		break;
		case GET_VOLT_MONITOR_CAP:
		{
			if((RetVal = copy_from_user(&Cap,(uint8_t *)arg,sizeof(Cap)))!=0)
			{
				return -EFAULT;
			}

			if((RetVal = copy_to_user((uint8_t *)arg,&vm_cap,sizeof(vm_cap)))!=0)
			{
				return -EFAULT;
			}
		}
		break;
		default:
			return -1;
	}
	return 0;
}

struct file_operations fops={
	.owner = THIS_MODULE,
	.open = open,
	.unlocked_ioctl = ioctl,
	.release = release,
};

static ssize_t sysfs_show_voltage_log(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned char buff_hm[32] = {0};
	unsigned char buff_gain[32] = {0};
	unsigned char cmd_hm;
	u64 vol_val, vol_val_fl;
	unsigned short gain;

	debug_printk("%s id: %d \n", __FUNCTION__, voltagenum);
	
	if (voltagenum < 8) {
		cmd_hm = ADL_BMC_CMD_RD_AIN0 + voltagenum;
	}
	else if (voltagenum >= 8 && voltagenum < 16) {
		cmd_hm = ADL_BMC_CMD_RD_AIN8 + (voltagenum - 8);
	}
	else
		return -EINVAL;

	debug_printk("I2C commmand: 0x%x\n", cmd_hm);

	
	/*read the hwmon register for voltage*/
        ret = adl_bmc_i2c_read_device(vm_data->adl_dev, cmd_hm, 2, buff_hm);
	if(ret != 2)
		return -EINVAL;

	msleep(30);
	/*read the gain value*/
        ret = adl_bmc_i2c_read_device(vm_data->adl_dev, ADL_BMC_CMD_GET_ADC_SCALE, 32, buff_gain);
	if (ret < 0)
		return ret;
	gain = (buff_gain[voltagenum * 2] << 8) | buff_gain[(voltagenum * 2) + 1];
	vol_val_fl = 3223 * gain;
	vol_val = vol_val_fl * (buff_hm[0] << 8 | buff_hm[1]);
	vol_val = vol_val / 1000000;

	return sprintf(buf,"%llu\n", vol_val);
        
}

static ssize_t sysfs_store_voltage_log(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	voltagenum = converttoint((char *)buf);
        return count;
}

static ssize_t sysfs_show_voltage_info(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

	if (voltagenum >= vm_data->cnt) 
		return -EINVAL;

	return sprintf(buf,"%s\n",vm_data->adl_bmc_vm_desc[voltagenum].name);
}

static ssize_t sysfs_store_voltage_info(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,size_t count)
{

	voltagenum = converttoint((char *)buf);

        return count;
}


struct kobj_attribute attr0 = __ATTR(voltage_info,0660,sysfs_show_voltage_info,sysfs_store_voltage_info);
struct kobj_attribute attr1 = __ATTR(voltage_log, 0660, sysfs_show_voltage_log, sysfs_store_voltage_log);

unsigned char voltagedata[32];

static int adl_bmc_vm_probe(struct platform_device *pdev)
{
	int ret, i, cnt = 0;
	unsigned char data[32];
	struct regulator_config config = { };

	struct device *dev = &pdev->dev;

 	kobj_ref = kobject_create_and_add("Info", &pdev->dev.kobj);
	ret = sysfs_create_file(kobj_ref, &attr0.attr); 

	if (ret < 0) { printk("error \n");
		goto ret_err; }


	ret = sysfs_create_file(kobj_ref, &attr1.attr); 

	if (ret < 0) { printk("error \n");
		goto ret_err; }

      	vm_dev = alloc_chrdev_region(&devdrv, 0, 1, "adl_vm");

	if(vm_dev < 0)
        {
                return -1;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
	class_adl_vm = class_create("adl_vm");
#else
	class_adl_vm = class_create(THIS_MODULE, "adl_vm");
#endif

	if (IS_ERR(class_adl_vm)) {
                printk("Error in Class_create\n");
        }
	 
	cdev_init(&cdev,&fops);

	if(cdev_add(&cdev,devdrv,1))
		printk("Error in Cdev_add\n");
	
	if(IS_ERR(device_create(class_adl_vm, NULL, devdrv, NULL, "adl_vm")))
		printk("Error in create device file\n");

	vm_data = devm_kzalloc(dev, sizeof(struct adl_bmc_vm_data), GFP_KERNEL);
        if(!vm_data)
                return -ENOMEM;

	vm_data->adl_dev = dev_get_drvdata(pdev->dev.parent);

	/*check voltage monitor capability*/
	if (vm_data->adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_VM){
		vm_cap = 1;
		debug_printk("Voltage monitor is compatible for this platform\n");
	}
	else { 
		vm_cap = 0;
		debug_printk("Voltage monitor is not compatible for this platform\n");
		return -EINVAL;
	}

	config.dev = &pdev->dev;
	config.driver_data = vm_data;
	config.init_data = &adl_bmc_initdata;


	debug_printk("probing....................\n");

	for (i = 0; i < 16; i++)
	{
		memset(vm_data->name_arr[i], 0, 256);
		memset(vm_data->adlink_name[i], 0, 256);

		data[0] = (unsigned char)i;
		ret = adl_bmc_i2c_write_device(vm_data->adl_dev, ADL_BMC_CMD_EXT_HW_DESC, 1, data);
		if (ret < 0)
			debug_printk("return failed  in write i=%d\n", i);
		msleep(30);
		ret = adl_bmc_i2c_read_device(vm_data->adl_dev, ADL_BMC_CMD_EXT_HW_DESC, 16, data);
		if (ret < 0)
			debug_printk("return failed in read i=%d\n", i);
		//printk("Buffer is %s\n", data +4);


		data[15] = 0;
		strcat(vm_data->name_arr[i], data + 4);
		strcpy(volt_desc[i],vm_data->name_arr[i]);
		vm_data->adl_bmc_vm_desc[i].name = vm_data->name_arr[i];
		vm_data->adl_bmc_vm_desc[i].id = i;
		vm_data->adl_bmc_vm_desc[i].type = REGULATOR_VOLTAGE;

		/*Check the number of voltage regulators supported*/
		if (strstr(data, "VCORE") != NULL)
			cnt++;
	}	

	vm_data->cnt = ADL_MAX_HW_MTR_INPUT - (cnt);
	debug_printk("Final Count is %d\n", vm_data->cnt);

	platform_set_drvdata(pdev, vm_data);
	debug_printk("Returning probe\n");
	return 0;

ret_err:
	kobject_put(kobj_ref);
	return ret;

}

static int adl_bmc_vm_remove(struct platform_device *pdev)
{
	struct adl_bmc_vm_data *vm_data = platform_get_drvdata(pdev);
	debug_printk("Remove...............\n");
        sysfs_remove_file(kernel_kobj, &attr0.attr);
        sysfs_remove_file(kernel_kobj, &attr1.attr);

	kobject_put(kobj_ref);
	
	device_destroy(class_adl_vm,devdrv);
	class_destroy(class_adl_vm);
	cdev_del(&cdev);
	unregister_chrdev(devdrv,"adl_vm");

	devm_kfree(&pdev->dev, vm_data);

	return 0;
}

static struct platform_driver adl_bmc_vm_driver = {
	.driver		= {
		.name		= "adl-bmc-vm",
	},
	.probe		= adl_bmc_vm_probe,
	.remove 	= adl_bmc_vm_remove,

};

module_platform_driver(adl_bmc_vm_driver);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("Voltage Monitor driver");
