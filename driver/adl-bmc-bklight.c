/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "adl-bmc.h"

#define ADL_BMC_MIN_BRIGHT 0
#define ADL_BMC_MAX_BRIGHT 255

static int Backlight_count;

struct adl_bmc_bklight {
	int brightness;
};

static int adl_bmc_update_brightness(struct backlight_device *bl, int brightness)
{
	int ret;
	struct adl_bmc_bklight *bklite = bl_get_data(bl);
	unsigned char buff[2];
	debug_printk("func: %s line: %d\n", __func__, __LINE__);

	buff[0] = brightness;

	ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_SET_BKLITE, 1,  &buff[0]);
	if (ret < 0) {
		debug_printk("i2c write error: %d\n", ret);
		return ret;
	}

	bklite->brightness = brightness;
	bl->props.brightness = brightness;
	
	return 0;
}

static int adl_bmc_bklight_update_status(struct backlight_device *bl)
{

	unsigned char buff[32];
	int brightness = 0, ret;

	//Increment Count
	Backlight_count++;

	if (Backlight_count == 2) {
		memset(buff, 0, sizeof(buff));
		ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BKLITE, 0,  buff);
		if (ret < 0) {
			debug_printk("i2c read error: %d\n", ret);
			return ret;
		}


		brightness = buff[0];
	}
	else
		brightness = bl->props.brightness;

	//printk("brightness: %d\n", brightness);

	debug_printk("func: %s line: %d\n", __func__, __LINE__);
	if (brightness < ADL_BMC_MIN_BRIGHT ||
			brightness > bl->props.max_brightness) {
		dev_err(&bl->dev, "lcd brightness should be %d to %d.\n",
				ADL_BMC_MIN_BRIGHT, ADL_BMC_MAX_BRIGHT);
		return -EINVAL;
	}

	/* Disable backlight*/
	if (bl->props.power == 4){
		brightness = 0;
	}

	debug_printk("bl_power: %d\n", bl->props.power);


	ret = adl_bmc_update_brightness(bl, brightness);
	if (ret)
		return ret;


	return 0;
}


static int adl_bmc_bklight_get_brightness(struct backlight_device *bl)
{
	int ret;
	unsigned char brightness;
	unsigned char buff[2];
	
	memset(buff, 0, sizeof(buff));
	ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BKLITE, 0,  buff);
	if (ret < 0) {
		debug_printk("i2c read error: %d\n", ret);
		return ret;
	}

	brightness = buff[0];
	
	printk("brightness: %c\n", brightness); 
	
	return brightness;
}

static const struct backlight_ops adl_bmc_bklight_ops = {
	.get_brightness	= adl_bmc_bklight_get_brightness,
	.update_status	= adl_bmc_bklight_update_status,
};


static int adl_bmc_bklight_probe(struct platform_device *pdev)
{	
	struct adl_bmc_bklight *bklite;
	struct backlight_device *bl = 0;
	struct backlight_properties props;

	unsigned char brightness = 0;
	unsigned char buff[2];
	int ret;

	bklite = devm_kzalloc(&pdev->dev, sizeof(*bklite), GFP_KERNEL);
	if(!bklite)
		return -ENOMEM;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = ADL_BMC_MAX_BRIGHT;

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
		bl = devm_backlight_device_register(&pdev->dev, pdev->name,
				pdev->dev.parent, bklite, &adl_bmc_bklight_ops,
				&props);
	#else
		bl = backlight_device_register(pdev->name,
				pdev->dev.parent, bklite, &adl_bmc_bklight_ops,
				&props);
	#endif	

	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight device\n");
		return PTR_ERR(bl);
	}

	
	memset(buff, 0, sizeof(buff));
	ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_GET_BKLITE, 0,  buff);
	if (ret < 0) {
		debug_printk("i2c read error: %d\n", ret);
		return ret;
	}

	brightness = buff[0];

	bl->props.brightness = brightness;

	platform_set_drvdata(pdev, bl);
	
	adl_bmc_bklight_update_status(bl);

	return 0;
}

static int adl_bmc_bklight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
        	devm_backlight_device_unregister(&pdev->dev, bl);
	#else
		backlight_device_unregister(bl);
	#endif

	return 0;
}

static struct platform_driver adl_bmc_bklight_driver = {
	.driver = {
		.name	= "adl-bmc-bklight",
	},

	.probe		= adl_bmc_bklight_probe,
	.remove		= adl_bmc_bklight_remove,
};

module_platform_driver(adl_bmc_bklight_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("Backlight Driver");

