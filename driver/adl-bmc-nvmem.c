#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/nvmem-provider.h>
#include <linux/delay.h>

#include "adl-bmc.h"

struct kobject *kobj_ref;
unsigned int storagesize;

static ssize_t nvmemcap_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int blocklen = 4;
	return sprintf(buf, "StorageSize %u\nBlockLength %u\n", storagesize, blocklen);
}

static int adl_bmc_nvmem_read(void *context, unsigned int offset, void *val, size_t bytes)
{

	unsigned char buf[32];
	unsigned short addr;

	int cnt = bytes, i;
	debug_printk("Func :%s offset: %d bytes: %lu\n", __func__, offset, bytes);
	
	addr = (unsigned short)offset;
	for(addr = offset, i = 0; addr < (offset + bytes); addr += 32, i ++) {
		int ret;
		buf[0] = addr >> 8;
		buf[1] = addr & 0x00ff;
		
		if (bytes <= MAX_BUFFER_SIZE) {
		       buf[2] = bytes;
		       cnt = bytes;
		      }

		else if (bytes > MAX_BUFFER_SIZE && i == 0) {
			buf[2] = MAX_BUFFER_SIZE;
			cnt = MAX_BUFFER_SIZE;
		}

		else if (bytes > MAX_BUFFER_SIZE && bytes%MAX_BUFFER_SIZE == 0) {
			buf[2] = MAX_BUFFER_SIZE;
			cnt = MAX_BUFFER_SIZE;
		}

		else {
			cnt = offset + bytes - addr ;
			if (cnt > 32) {
				cnt = 32;
				buf[2] = 32;
			}
		}

		debug_printk("buf[0]: 0x%02x 0x%02x 0x%02x offset: %d no of bytes %lu\n", buf[0], buf[1], buf[2], offset, bytes);

		ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_SET_ADDRESS, 3, buf);
		if (ret < 0)
			return ret;

		//sleep for 30 ms
		msleep(30);
	
			
		if (bytes > 32) {
		ret  = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_READ_DATA, cnt, &((unsigned char *)val)[addr - offset]);
		}
		
		else {
			printk("count2: %d\n", cnt);
			ret  = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_READ_DATA, cnt, &((unsigned char *)val)[0]);
		}


		if (ret < 0)
			return ret;
	}

	return bytes;

}


static int adl_bmc_nvmem_write(void *context, unsigned int offset, void *val, size_t bytes)
{

	unsigned char buf[32];
	unsigned short addr;
	unsigned short size;

	debug_printk("Func :%s offset: %d bytes: %lu\n", __func__, offset, bytes);
	debug_printk("Written value: %s\n", (char *)val);

	if(offset+bytes > 1024) {
		return -EINVAL;	
	}

	size = bytes;
	for(addr = offset; addr < (offset + bytes); addr += 32) {
		int ret;
		buf[0] = addr >> 8;
		buf[1] = addr & 0x00ff;
		if(size > 32){
			buf[2] = 32;
			size = size - 32;
		}
		else
			buf[2] = (unsigned char) size;


		debug_printk("buf[0]: 0x%02x 0x%02x 0x%02x offset: %d no of bytes %lu\n", buf[0], buf[1], buf[2], offset, bytes);

		msleep(30);
		ret  = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_SET_ADDRESS, 3, buf);
		if (ret < 0)
			return ret;

		//sleep for 30 ms
		msleep(30);

		ret  = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_WRITE_DATA, buf[2], &((unsigned char *)val)[0]);
		if (ret < 0)
			return ret;
	}


	return 0;
}

struct kobj_attribute attr0 = __ATTR_RO(nvmemcap);

static struct nvmem_config adl_bmc_nvmem_config = {
	.name = "nvmem_adl",
	.read_only = false,
	.word_size = 4,
	.stride = 4,
	.reg_read = adl_bmc_nvmem_read,
	.reg_write = adl_bmc_nvmem_write,
};


static int adl_bmc_nvmem_probe(struct platform_device *pdev)
{
	int ret;
	struct nvmem_device *nvdev;
	struct adl_bmc_dev *adl_dev;
	struct module owner;

	adl_dev = dev_get_drvdata(pdev->dev.parent);

	/* check storage area capability */
	if (adl_dev->Bmc_Capabilities[0] & ADL_BMC_CAP_MEM)
		storagesize = 1024;
	else
		storagesize = 512;

	adl_bmc_nvmem_config.dev = &pdev->dev;
	adl_bmc_nvmem_config.size = storagesize;
	adl_bmc_nvmem_config.owner = &owner;

	debug_printk("probe ..............\n");

	nvdev = nvmem_register(&adl_bmc_nvmem_config);
	if (IS_ERR(nvdev))
	{
		printk(KERN_DEBUG "Test\n");
		debug_printk("Failed to register\n");
		return PTR_ERR(nvdev);
	}
	
	platform_set_drvdata(pdev, nvdev);

	kobj_ref = kobject_create_and_add("capabilities", &pdev->dev.kobj);
	if (!kobj_ref)
		return -ENOMEM;

	ret = sysfs_create_file(kobj_ref, &attr0.attr);
	if (ret) {
		kobject_put(kobj_ref);
		return ret;
	}

	return 0;
}


static int adl_bmc_nvmem_remove(struct platform_device *pdev)
{

	struct nvmem_device *nvdev;
	nvdev = platform_get_drvdata(pdev);
	debug_printk("Remove ..............\n");
	sysfs_remove_file(kernel_kobj, &attr0.attr);
	kobject_put(kobj_ref);

	nvmem_unregister(nvdev);
	return 0;

}


static struct platform_driver adl_bmc_nvmem_driver = {
	.driver = {
		.name = "adl-bmc-nvmem",
	},
	.probe = adl_bmc_nvmem_probe,
	.remove = adl_bmc_nvmem_remove,

};


module_platform_driver(adl_bmc_nvmem_driver);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("driver for storage");
