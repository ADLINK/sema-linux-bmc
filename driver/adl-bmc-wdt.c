#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>
#include "adl-bmc.h"


#define WDT_TIMEOUT   		10
#define WDT_MIN_TIMEOUT   	1
#define WDT_MAX_TIMEOUT   	65535


static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started "
        "(default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct adl_bmc_wdt {
	struct adl_bmc_dev *adl_dev;
	struct watchdog_device wdt;
	struct kobject *kobj_ref;
};

#define WDT_TO_ADL_BMC_WDT(_wdt) (container_of(_wdt, struct adl_bmc_wdt, wdt))

static int adl_bmc_wdt_write(struct watchdog_device *wdt, unsigned short val)
{
	int ret = 0;
	unsigned char buff[2];
	struct adl_bmc_wdt *awdt = WDT_TO_ADL_BMC_WDT(wdt);

	buff[0] = val >> 8;
	buff[1] = val & 0xff;

	ret = adl_bmc_i2c_write_device(awdt->adl_dev, ADL_BMC_CMD_SET_WD, 2,  &buff[0]);
	if (ret < 0) {
		debug_printk("i2c write error: %d\n", ret);
		return ret;
	}

	return ret;

}

static int adl_bmc_wdt_set_timeout(struct watchdog_device *wdt, unsigned int timeout)
{
	debug_printk("set timeout.................\n");
        wdt->timeout = timeout;
 
	return adl_bmc_wdt_write(wdt, timeout);
}

static unsigned int adl_bmc_wdt_get_timeleft(struct watchdog_device *wdt)
{
	int ret; 
	unsigned int count;
	unsigned char buff[32];
	struct adl_bmc_wdt *awdt = WDT_TO_ADL_BMC_WDT(wdt);
	debug_printk("get timeleft.................\n");
 
	ret = adl_bmc_i2c_read_device(awdt->adl_dev, ADL_BMC_CMD_SET_WD, 0,  (void *)buff);
	if (ret < 0) {
		debug_printk("i2c write error: %d\n", ret);
		return ret;
	}
	
	count = (unsigned int)buff[0] << 8 | buff[1];

	return count;
}

static int adl_bmc_wdt_start(struct watchdog_device *wdt)
{
	debug_printk("start.................\n");
	return adl_bmc_wdt_write(wdt, wdt->timeout);	
}

static int adl_bmc_wdt_ping(struct watchdog_device *wdt)
{
	debug_printk("ping.................\n");
	return adl_bmc_wdt_write(wdt, wdt->timeout);
}

static int adl_bmc_wdt_stop(struct watchdog_device *wdt)
{
	debug_printk("stop...................................\n");
        wdt->timeout = 0;
        return adl_bmc_wdt_write(wdt, wdt->timeout);
}

static const struct watchdog_info adl_bmc_wdt_info = {
        .options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING, 
        .identity = "ADL BMC Watchdog",
};

static const struct watchdog_ops adl_bmc_wdt_ops = {
        .owner           = THIS_MODULE,
        .start           = adl_bmc_wdt_start,
        .stop            = adl_bmc_wdt_stop,
        .ping            = adl_bmc_wdt_ping,
        .set_timeout     = adl_bmc_wdt_set_timeout,
        .get_timeleft    = adl_bmc_wdt_get_timeleft,
};


static ssize_t wdt_min_timeout_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	debug_printk("%s:%d called\n", __func__, __LINE__);
	return sprintf(buf, "%d\n", WDT_MIN_TIMEOUT);
}

static ssize_t wdt_max_timeout_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	debug_printk("%s:%d called\n", __func__, __LINE__);
	return sprintf(buf, "%d\n", WDT_MAX_TIMEOUT);
}

static ssize_t sysfs_show_PwrUpWDog(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int ret;
        unsigned short timeout;
	unsigned char buff[32];
	memset(buff, 0, 32);
	
        ret = adl_bmc_i2c_read_device(NULL, ADL_BMC_CMD_SET_PWD, 2, (void *)buff);	
	if (ret < 0)
		return ret;

	timeout = (buff[0] << 8) | (buff[1]);

        return sprintf(buf, "%hu\n", timeout);
}

static ssize_t sysfs_store_PwrUpWDog(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned char buff[32];
	int ret;
        int timeout;

	sscanf(buf, "%d", &timeout);

	if(timeout > 65535) {
		return -EINVAL;	
	}

	debug_printk("value: %u\n", timeout);
	buff[0] = timeout >> 8;
	buff[1] = timeout & 0xFF;

        ret = adl_bmc_i2c_write_device(NULL, ADL_BMC_CMD_SET_PWD, 2, (void *)buff);	
	if (ret < 0)
		return ret;
	
	return count;
}

struct kobj_attribute attr0 = __ATTR_RO(wdt_min_timeout);
struct kobj_attribute attr1 = __ATTR_RO(wdt_max_timeout);
struct kobj_attribute attr2 = __ATTR(PwrUpWDog, 0660, sysfs_show_PwrUpWDog, sysfs_store_PwrUpWDog);

static int adl_bmc_wdt_probe(struct platform_device *pdev)
{
        int ret = 0;
	struct adl_bmc_wdt *awdt;
	unsigned char buff[32];
	
	int def_timeout = 0;	
        awdt = kzalloc(sizeof(*awdt), GFP_KERNEL);
        if (!awdt)
                return -ENOMEM;

	awdt->adl_dev = dev_get_drvdata(pdev->dev.parent);

	memset(buff, 0, sizeof(buff));
	ret = adl_bmc_i2c_read_device(awdt->adl_dev, ADL_BMC_CMD_SET_WD, 2,  (void *)buff);
	if (ret < 0) {
		debug_printk("i2c write error: %d\n", ret);
		return ret;
	}

	def_timeout = (unsigned int)buff[0] << 8 | buff[1];

        awdt->wdt.info               = &adl_bmc_wdt_info;
        awdt->wdt.ops                = &adl_bmc_wdt_ops;
        awdt->wdt.status             = 0;
        awdt->wdt.timeout            = def_timeout;
        awdt->wdt.min_timeout        = WDT_MIN_TIMEOUT;
        awdt->wdt.max_timeout        = WDT_MAX_TIMEOUT;
        awdt->wdt.parent = &pdev->dev;

	if(def_timeout)
		awdt->wdt.status = 0x1;

        watchdog_set_nowayout(&awdt->wdt, false); //nowayout);
        platform_set_drvdata(pdev, awdt);

        ret = watchdog_register_device(&awdt->wdt);
        if (ret)
                return ret;


	awdt->kobj_ref = kobject_create_and_add("Capabilities", &pdev->dev.kobj);

	if((ret = sysfs_create_file(awdt->kobj_ref, &attr0.attr))) {
		debug_printk(KERN_ERR "Error sysfs attr0\n");
		return ret;
	}

	if((ret = sysfs_create_file(awdt->kobj_ref, &attr1.attr))) {
		debug_printk(KERN_ERR "Error sysfs attr1\n");
		return ret;
	}

	if((ret = sysfs_create_file(awdt->kobj_ref, &attr2.attr))) {
		debug_printk(KERN_ERR "Error sysfs attr1\n");
		return ret;
	}

        return 0;
}

static int adl_bmc_wdt_remove(struct platform_device *pdev)
{
	struct adl_bmc_wdt *awdt = platform_get_drvdata(pdev);

        sysfs_remove_file(kernel_kobj, &attr0.attr);
        sysfs_remove_file(kernel_kobj, &attr1.attr);
        sysfs_remove_file(kernel_kobj, &attr2.attr);
	kobject_put(awdt->kobj_ref);

	debug_printk(" %s called.......\n", __func__);
        watchdog_unregister_device(&awdt->wdt);

	kfree(awdt);
	return 0;
}


static struct platform_driver adl_bmc_wdt_driver = {
	.driver = {
		.name = "adl-bmc-wdt",
	},
	.probe = adl_bmc_wdt_probe,
	.remove = adl_bmc_wdt_remove,

};
	
module_platform_driver(adl_bmc_wdt_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("Watchdog Timer driver");
