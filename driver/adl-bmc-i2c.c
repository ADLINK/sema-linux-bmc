#define pr_fmt(fmt) "adlink-i2c: " fmt

#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "adl-bmc.h"

#define SLAVE_ADDR(x)						(((x)<<1) & 0xFE)
#define BMC_DELAY(x)  						udelay(delay * (x))
#define BMC_DELAY_PER_BYTE					100

#define BMC_I2C_WRITE_LEN_MAX				29
#define BMC_I2C_READ_LEN_MAX				32

#define BMC_I2C_RETRY_STATUS_DELAY			100
#define BMC_I2C_RETRY_STATUS_MAX			100

#define BMC_I2C_ERR_CLK_TIMEOUT				(1 << 7)
#define BMC_I2C_ERR_TRANS_TIMEOUT			(1 << 5)
#define BMC_I2C_ERR_ARB_LOST				(1 << 4)
#define BMC_I2C_ERR_ADDR_ACK				(1 << 2)
#define BMC_I2C_BUS_AVAILABLE				(1 << 0)
#define BMC_I2C_STATUS_TIMEOUT(x)			((x) & (BMC_I2C_ERR_CLK_TIMEOUT | BMC_I2C_ERR_TRANS_TIMEOUT))
#define BMC_I2C_STATUS_ARB_LOST(x)			((x) & BMC_I2C_ERR_ARB_LOST)
#define BMC_I2C_STATUS_ADDR_NAK(x)			((x) & BMC_I2C_ERR_ADDR_ACK)
#define BMC_I2C_STATUS_TRANSFER_DONE(x)		((x) & BMC_I2C_BUS_AVAILABLE)
#define BMC_I2C_STATUS_TRANSFER_FAILED(x)	(BMC_I2C_STATUS_TIMEOUT(x) || \
												((x) & \
												(BMC_I2C_ERR_ARB_LOST | \
												BMC_I2C_ERR_ADDR_ACK)))
#define BMC_I2C_STATUS_TRANSFER_OK(x)		((BMC_I2C_STATUS_TRANSFER_FAILED(x) == 0) \
												&& (BMC_I2C_STATUS_TRANSFER_DONE(x)))

struct adlink_i2c_dev {
	struct device 		*dev;
	struct mutex 		i2c_lock;
	struct i2c_adapter 	adapter;
};

static int delay;

static int i2c_status (void)
{
	int i, ret;
	unsigned char xfer_status;

	for(i=0; i<BMC_I2C_RETRY_STATUS_MAX; i++) {
		xfer_status = 0;
		ret = adl_bmc_i2c_read_device(NULL, 0xC4, 0, &xfer_status);
		if(ret < 0) {
			printk("i2c read status error: %d\n", ret);
			return -ENODEV;
		}
		else {
			debug_printk("i2c status = %x\n", xfer_status);

			if(BMC_I2C_STATUS_TRANSFER_OK(xfer_status))
				return 0;
			else if(BMC_I2C_STATUS_ADDR_NAK(xfer_status))
				return -ENODEV;
			else if(BMC_I2C_STATUS_TIMEOUT(xfer_status))
				return -ETIMEDOUT;
			else if(BMC_I2C_STATUS_ARB_LOST(xfer_status))
				return -EBUSY;
			else
				udelay(BMC_I2C_RETRY_STATUS_DELAY);
		}
	}

	return -ETIMEDOUT;
}

static int i2c_write (struct i2c_msg *msg)
{
	int i, ret;
	unsigned char buf[32];

	/*Added for write length validation*/
	if(msg->len > BMC_I2C_WRITE_LEN_MAX)  {
		printk("i2c write error: write length cannot exceed %d bytes\n",
			BMC_I2C_WRITE_LEN_MAX);
		return -EINVAL;
	}	
	else {
		buf[0] = SLAVE_ADDR(msg->addr);
		buf[1] = msg->len;
		buf[2] = 0x00;

		for(i=0; i<msg->len; i++)
			buf[i+3] = msg->buf[i];

		ret = adl_bmc_i2c_write_device(NULL, 0xC2, msg->len + 3, buf);
		if (ret < 0) {
			printk("i2c write error: %d\n", ret);
			return -ENODEV;
		}

		BMC_DELAY(msg->len + 1);

		return i2c_status();
	}
}

static int i2c_read (struct i2c_msg *msg)
{
	int ret;
	unsigned char buf[32];
	buf[0] = SLAVE_ADDR(msg->addr);

	if(msg->len > BMC_I2C_READ_LEN_MAX) {
		printk("i2c read error: read length cannot exceed %d bytes\n",
			BMC_I2C_READ_LEN_MAX);
		return -EINVAL;
	}
	else {
		int i;

		buf[1] = 0x00;
		buf[2] = msg->len;

		ret = adl_bmc_i2c_write_device(NULL, 0xC2, msg->len + 3, buf);
		if (ret < 0) {
			printk("i2c write error: %d\n", ret);
			return -ENODEV;
		}

		BMC_DELAY(msg->len + 1);

		ret = i2c_status();
		if (ret < 0)
			return ret;

		if (msg->len > 0) {
			ret = adl_bmc_i2c_read_device(NULL, 0xBF, 0, buf);
			if (ret < 0) {
				printk("i2c read error: %d\n", ret);
				return -ENODEV;
			}
		}

		debug_printk("read request %x %d %x %x\n", msg->addr, msg->len, msg->flags, msg->buf[0]);
		for(i=0; (i<ret) && (i<msg->len); i++)
			msg->buf[i] = buf[i];

		return 0;
	}
}

static int i2c_write_read (struct i2c_msg *msg_wr, struct i2c_msg *msg_rd)
{
	int i, ret;
	unsigned char buf[32];


	if(SLAVE_ADDR(msg_wr->addr) != SLAVE_ADDR(msg_rd->addr)) {
		printk("i2c error: repeated start cannot be done with a different slave addr\n");
		return -EINVAL;
	}

	if(msg_wr->len == 0) {
		printk("i2c write read error: write length cannot be 0 bytes\n");
		return -EINVAL;
	}

	if(msg_rd->len == 0) {
		printk("i2c write read error: read length cannot be 0 bytes\n");
		return -EINVAL;
	}

	if(msg_wr->len > BMC_I2C_WRITE_LEN_MAX)  {
		printk("i2c write read error: write length cannot exceed %d bytes\n",
			BMC_I2C_WRITE_LEN_MAX);
		return -EINVAL;
	}	

	if(msg_rd->len > BMC_I2C_READ_LEN_MAX) {
		printk("i2c write read error: read length cannot exceed %d bytes\n",
			BMC_I2C_READ_LEN_MAX);
		return -EINVAL;
	}

	buf[0] = SLAVE_ADDR(msg_wr->addr);
	buf[1] = msg_wr->len;
	buf[2] = msg_rd->len;

	for(i=0; i<msg_wr->len; i++)
		buf[i+3] = msg_wr->buf[i];

	ret = adl_bmc_i2c_write_device(NULL, 0xC2, msg_wr->len + 3, buf);
	if (ret < 0) {
		printk("i2c write read error: %d\n", ret);
		return -ENODEV;
	}

	BMC_DELAY(msg_wr->len + msg_rd->len + 2);

	ret = i2c_status();
	if (ret < 0)
		return ret;

	ret = adl_bmc_i2c_read_device(NULL, 0xBF, 0, buf);
	if (ret < 0) {
		printk("i2c read error: %d\n", ret);
		return -ENODEV;
	}

	debug_printk("read %x %d %x %x\n", msg_rd->addr, msg_rd->len, msg_rd->flags, msg_rd->buf[0]);
	for(i=0; (i<ret) && (i<msg_rd->len); i++)
		msg_rd->buf[i] = buf[i];

	return 0;
}

static int adlink_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int i, ret = -ENODEV;
	struct adlink_i2c_dev *adlink = i2c_get_adapdata(adap);

	mutex_lock(&adlink->i2c_lock);

	for (i=0; i<num; i++) {
		if(msgs[i].flags & I2C_M_RD) {
			ret = i2c_read(&msgs[i]);
		}
		else if((msgs[i].flags == 0) || (msgs[i].flags == 0x200)) {
			if (i < (num - 1)) {
				/* check whether next message is a read with repeated start */
				if (msgs[i+1].flags & I2C_M_RD) {
					ret = i2c_write_read(&msgs[i], &msgs[i+1]);
					++i;
				}
				else {
					ret = i2c_write(&msgs[i]);
				}
			}
			else {
				ret = i2c_write(&msgs[i]);
			}
		}
		else {
			printk("Unsupported xfer %x %x\n", msgs[i].flags, msgs[i].len);
			ret = -EOPNOTSUPP;
		}

		if (ret < 0)
			break;
	}

	mutex_unlock(&adlink->i2c_lock);

	if(ret == 0)
		ret = num;

	return ret;
}

static u32 adlink_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm adlink_i2c_algo = {
	.functionality	= adlink_i2c_func,
	.master_xfer	= adlink_i2c_xfer,
};

static int adl_bmc_i2c_probe(struct platform_device *pdev)
{
	struct i2c_adapter *adap;
	struct adlink_i2c_dev *adlink;

	char buf[100] = { 0};
	int ret;


	adlink = devm_kzalloc(&pdev->dev, sizeof(struct adlink_i2c_dev), GFP_KERNEL);
	if (!adlink)
		return -ENOMEM;

	adlink->dev = &pdev->dev;
	mutex_init(&adlink->i2c_lock);
	platform_set_drvdata(pdev, adlink);

	adap = &adlink->adapter;
	i2c_set_adapdata(adap, adlink);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_DEPRECATED;
	strscpy(adap->name, "ADLINK BMC I2C adapter", sizeof(adap->name));
	adap->algo = &adlink_i2c_algo;

	ret = adl_bmc_i2c_read_device(NULL, 0x30, 0, buf);
	if (ret < 0) {

		printk("i2c read error: %d\n", ret);
		return -ENODEV;
	}

	if(strncmp("NanoX,cExp-BT/BT2 3v3", buf, strlen("NanoX,cExp-BT/BT2 3v3")) != 0)
	{
		delay = BMC_DELAY_PER_BYTE;
	}

	return i2c_add_adapter(adap);
}

static int adl_bmc_i2c_remove(struct platform_device *pdev)
{
	struct adlink_i2c_dev *adlink = platform_get_drvdata(pdev);

	i2c_del_adapter(&adlink->adapter);
	return 0;
}

static struct platform_driver adl_bmc_i2c_driver = {
	.driver = {
		.name	= "adl-bmc-i2c",
	},

	.probe		= adl_bmc_i2c_probe,
	.remove		= adl_bmc_i2c_remove,
};

module_platform_driver(adl_bmc_i2c_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adlink ");
MODULE_DESCRIPTION("ADLINK BMC I2C driver");
