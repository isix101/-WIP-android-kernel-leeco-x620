/*
 * drivers/misc/tfa98xx.c
 *
 * Copyright (c) 2014, WPI (World Peace Industrial Group).  All rights reserved.
 * Author: Nick Li
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <sound/initval.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>


#include <linux/of_gpio.h>
#include <linux/of_device.h>

//#include <tfa98xx/tfa98xx.h>

#define I2C_MASK_FLAG    (0xffffffff)

#define I2C_ENEXT_FLAG   (0x00008000)

#define I2C_DMA_FLAG     (0x00020000)


/***************** DEBUG ****************/
//#define pr_debug printk
//#define pr_warning printk

/* For MTK platform */
#define USING_MTK_PLATFORM

/* print the bytes of I2C read and write, should be closed when debugging is finished.. */
#define TEST_DEBUG

/* print the log message, should be closed when debugging is finished.. */
#define TFA98XX_DEBUG

/* Check the chip version and test I2C's operation is OK or not */
#define CHECK_CHIP_VERSION

//for SPRD/ROCKCHIP
#define I2C_BUS_NUM_STATIC_ALLOC

/***************** DEBUG ****************/

#ifdef USING_MTK_PLATFORM

#include <linux/dma-mapping.h>

/* for MTK I2C burst */
#define I2C_USE_DMA

#ifdef I2C_USE_DMA
static u8 *I2CDMABuf_va = NULL;
static dma_addr_t I2CDMABuf_pa = 0;
#endif
#endif

#ifdef TFA98XX_DEBUG
#define PRINT_LOG printk
#else
#define PRINT_LOG(...)
#endif

struct tfa98xx_dev
{
	struct mutex		lock;
	struct i2c_client	*client;
	struct miscdevice	tfa98xx_device;
	bool deviceInit;
};

static struct tfa98xx_dev *tfa98xx;

/* tfa98xx I2C defination ++ */
#define TFA98XX_I2C_NAME   "i2c_smartpa"
#define TFA_I2CSLAVEBASE        0x34
#define TFA_I2CSLAVEBASE_R      0x36
#define I2C_STATIC_BUS_NUM        (0)

#define MAX_BUFFER_SIZE	255
/* tfa98xx I2C defination -- */

#define TFA98XX_WHOAMI 	0x03
#define TFA9890_REV 					0x80
#define TFA9887_REV 					0x12

static struct i2c_board_info __initdata tfa98xx_i2c_boardinfo = {
    I2C_BOARD_INFO(TFA98XX_I2C_NAME, TFA_I2CSLAVEBASE), 
};


#ifdef I2C_BUS_NUM_STATIC_ALLOC
int i2c_static_add_device(struct i2c_board_info *info)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int err;

	pr_info("tfa98xx: Attempting to get I2C adapter on bus number %d\n", I2C_STATIC_BUS_NUM);
	
	adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);
	if (!adapter) {
		pr_err("tfa98xx %s: FAILED: Cannot get I2C adapter on bus %d. I2C controller may not be initialized yet or bus number is incorrect.\n", 
			__FUNCTION__, I2C_STATIC_BUS_NUM);
		pr_err("tfa98xx: Available I2C buses: Check kernel config and Device Tree for I2C controller status.\n");
		err = -ENODEV;
		goto i2c_err;
	}

	pr_info("tfa98xx: Successfully got I2C adapter on bus %d. Now creating I2C device at address 0x%02x\n", 
		I2C_STATIC_BUS_NUM, info->addr);

	client = i2c_new_device(adapter, info);
	if (!client) {
		pr_err("tfa98xx %s: FAILED: Cannot add I2C device at address 0x%02x on bus %d. Device may not be present or address is wrong.\n",
			__FUNCTION__, (unsigned int)info->addr, I2C_STATIC_BUS_NUM);
		pr_err("tfa98xx: Please verify that TFA98xx chip is properly connected and powered.\n");
		err = -ENODEV;
		goto i2c_err;
	}

	pr_info("tfa98xx: I2C device successfully created at address 0x%02x on bus %d\n", 
		info->addr, I2C_STATIC_BUS_NUM);
	
	i2c_put_adapter(adapter);

	return 0;

i2c_err:
	if (adapter)
		i2c_put_adapter(adapter);
	pr_err("tfa98xx: i2c_static_add_device FAILED with error %d\n", err);
	return err;
}

#endif /*I2C_BUS_NUM_STATIC_ALLOC*/

//////////////////////// i2c R/W ////////////////////////////

#ifdef I2C_USE_DMA
static int tfa_i2c_write(struct i2c_client *client, const uint8_t *buf, int len)
{
	int i = 0;
	for(i = 0 ; i < len; i++)
	{
		I2CDMABuf_va[i] = buf[i];
	}

	if(len < 8)
	{
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
		return i2c_master_send(client, buf, len);
	}
	else
	{
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		return i2c_master_send(client, I2CDMABuf_va, len);
	}
}

static int tfa_i2c_read(struct i2c_client *client, uint8_t *buf, int len)
{
    int i = 0, ret = 0;

    if(len < 8)
    {
        client->addr = (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
        return i2c_master_recv(client, buf, len);
    }
    else
    {
        client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
        ret = i2c_master_recv(client, I2CDMABuf_va, len);

        if(ret < 0)
        {
            printk("%s: i2c_master_recv(len = %d) returned %d.\n", __func__, len, ret);
            return ret;
        }

        for(i = 0; i < len; i++)
        {
            buf[i] = I2CDMABuf_va[i];
        }
    }
    client->addr = client->addr & I2C_MASK_FLAG;
    return ret;
}
#endif

static ssize_t tfa98xx_dev_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
//	struct tfa98xx_dev *tfa98xx_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
#ifdef TEST_DEBUG
	int i;
#endif

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	PRINT_LOG("%s : reading %zu bytes.\n", __func__, count);

#ifdef I2C_USE_DMA
    //tfa98xx->client->addr = tfa98xx->client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
    mutex_lock(&tfa98xx->lock);
    ret = tfa_i2c_read(tfa98xx->client, tmp, count);
    mutex_unlock(&tfa98xx->lock);
    tfa98xx->client->addr = tfa98xx->client->addr & I2C_MASK_FLAG;
    if (ret < 0)
    {
        printk("%s: i2c_master_recv returned %d\n", __func__, ret);
        return ret;
    }
#else
	/* Read data */
	mutex_lock(&tfa98xx->lock);
	ret = i2c_master_recv(tfa98xx->client, tmp, count);
	mutex_unlock(&tfa98xx->lock);

	if (ret < 0)
	{
		printk("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
#endif
	if (ret > count)
	{
		printk("%s: received too many bytes from i2c (%d)\n", __func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret))
	{
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

#ifdef TEST_DEBUG
	PRINT_LOG("Read from tfa98xx:");
	for(i = 0; i < ret; i++)
	{
		PRINT_LOG(" %02X", tmp[i]);
	}
	PRINT_LOG("\n");
#endif

	return ret;
}

static ssize_t tfa98xx_dev_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	struct tfa98xx_dev  *tfa98xx_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
#ifdef TEST_DEBUG
	int i;
#endif

	tfa98xx_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
	{
		count = MAX_BUFFER_SIZE;
	}
	if (copy_from_user(tmp, buf, count))
	{
		printk("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	PRINT_LOG("%s : writing %zu bytes.\n", __func__, count);
#ifdef I2C_USE_DMA
    //tfa98xx->client->addr = tfa98xx->client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	mutex_lock(&tfa98xx->lock);
    ret = tfa_i2c_write(tfa98xx->client, tmp, count);
	mutex_unlock(&tfa98xx->lock);
    tfa98xx->client->addr = tfa98xx->client->addr & I2C_MASK_FLAG;
	if (ret < 0)
	{
		printk("%s: tfa_i2c_write returned %d\n", __func__, ret);
		return ret;
	}

#else
	/* Write data */
	mutex_lock(&tfa98xx->lock);
	ret = i2c_master_send(tfa98xx->client, tmp, count);
	mutex_unlock(&tfa98xx->lock);
	if (ret < 0)
	{
		printk("%s : i2c_master_send returned %d\n", __func__, ret);
		return ret;
	}
#endif

#ifdef TEST_DEBUG
	PRINT_LOG("Write to tfa98xx:");
	for(i = 0; i < count; i++)
	{
		PRINT_LOG(" %02X", tmp[i]);
	}
	PRINT_LOG("\n");
#endif
	return ret;
}
//////////////////////// i2c R/W ////////////////////////////

static int tfa98xx_dev_open(struct inode *inode, struct file *filp)
{

	struct tfa98xx_dev *tfa98xx_dev = container_of(filp->private_data, struct tfa98xx_dev, tfa98xx_device);

	filp->private_data = tfa98xx_dev;

	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static long tfa98xx_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
//	struct tfa98xx_dev *tfa98xx_dev = filp->private_data;
	//void __user *argp = (void __user *)arg;
	//int mode = 0;

	switch (cmd)
	{

		case I2C_SLAVE:
		case I2C_SLAVE_FORCE:
			if((arg == TFA_I2CSLAVEBASE) ||(arg == TFA_I2CSLAVEBASE_R))
			{
			    tfa98xx->client->addr = arg;
			    return 0;
			}
			else
			    return -1;

		default:
			printk("%s bad ioctl %u\n", __func__, cmd);
			return -EINVAL;
	}

	return 0;

}

static const struct file_operations tfa98xx_dev_fops =
{
	.owner	= THIS_MODULE,
	.open	= tfa98xx_dev_open,
	.unlocked_ioctl = tfa98xx_dev_ioctl,
	.llseek	= no_llseek,
	.read	= tfa98xx_dev_read,
	.write	= tfa98xx_dev_write,
};

static int tfa98xx_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	int ret = 0;
	int read_id = 0;
	int rev_value = 0;
//	char tmp[MAX_BUFFER_SIZE];
//	int pdn_gpio = -1;
//	int eint_gpio = -1;
//	struct device_node *np = client->dev.of_node;

	pr_info("tfa98xx: %s +, I2C client address: 0x%02x, adapter: %s (bus %d)\n", 
		__func__, client->addr, client->adapter->name, client->adapter->nr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		pr_err("tfa98xx %s: FAILED: I2C adapter on bus %d does not support I2C_FUNC_I2C functionality\n", 
			__func__, client->adapter->nr);
		return  -ENODEV;
	}

	pr_info("tfa98xx: I2C functionality check passed on bus %d\n", client->adapter->nr);

	tfa98xx = kzalloc(sizeof(*tfa98xx), GFP_KERNEL);
	if (tfa98xx == NULL)
	{
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	i2c_set_clientdata(client, tfa98xx);
	tfa98xx->client   = client;

	/* init mutex and queues */
	mutex_init(&tfa98xx->lock);

#ifdef I2C_USE_DMA
	I2CDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, MAX_BUFFER_SIZE, &I2CDMABuf_pa, GFP_KERNEL);
	if(!I2CDMABuf_va)
	{
			pr_err("tfa98xx: FAILED: Cannot allocate DMA coherent buffer for I2C transfers (size=%d)\n", MAX_BUFFER_SIZE);
			return -1;
	}
	pr_info("tfa98xx: DMA buffer allocated successfully (va=%p, pa=%pad)\n", I2CDMABuf_va, &I2CDMABuf_pa);
#endif

#ifdef CHECK_CHIP_VERSION
	pr_info("tfa98xx: Reading WHOAMI register (0x%02x) to detect chip...\n", TFA98XX_WHOAMI);
	
	mutex_lock(&tfa98xx->lock);
	read_id = i2c_smbus_read_word_data(client, TFA98XX_WHOAMI);
	mutex_unlock(&tfa98xx->lock);
	
	if (read_id < 0) {
		pr_err("tfa98xx: FAILED: I2C read error when reading WHOAMI register. Error code: %d\n", read_id);
		pr_err("tfa98xx: Possible causes: Device not powered, wrong I2C address (current: 0x%02x), or bad connection\n", 
			client->addr);
		ret = -1;
		goto i2c_error;
	}
	
	rev_value = ((read_id & 0x00FF)<< 8) | ((read_id & 0xFF00)>> 8);
	rev_value = rev_value & 0xFFFF;

	PRINT_LOG("tfa98xx_i2c_probe:rev_value=0x%x\n", rev_value);

	if (rev_value == TFA9887_REV)
	{
		pr_info("tfa98xx: SUCCESS: NXP TFA9887 detected! Chip ID: 0x%02x (expected: 0x%02x)\n", 
			rev_value, TFA9887_REV);
		pr_info("tfa98xx: TFA9887 registered I2C driver!\n");
	}
	else if(rev_value == TFA9890_REV)
	{
		pr_info("tfa98xx: SUCCESS: NXP TFA9890 detected! Chip ID: 0x%02x (expected: 0x%02x)\n", 
			rev_value, TFA9890_REV);
		pr_info("tfa98xx: TFA9890 registered I2C driver!\n");
	}
	else
	{
		pr_err("tfa98xx: FAILED: NXP device NOT found. Read WHOAMI returned 0x%04x, expected 0x%02x (TFA9887) or 0x%02x (TFA9890)\n", 
			rev_value, TFA9887_REV, TFA9890_REV);
		pr_err("tfa98xx: I2C communication error or device not present on bus %d at address 0x%02x\n", 
			client->adapter->nr, client->addr);
		ret = -1;
		goto i2c_error;
	}
#endif
#if 0
	//bypass
	tmp[0] = 0x04;
	tmp[1] = 0x78;
	tmp[2] = 0x0b;
	i2c_master_send(client, tmp, 3);
	tmp[0] = 0x09;
	tmp[1] = 0x82;
	tmp[2] = 0x19;
	i2c_master_send(client, tmp, 3);
	tmp[0] = 0x09;
	tmp[1] = 0x82;
	tmp[2] = 0x18;
	i2c_master_send(client, tmp, 3);
#endif
	tfa98xx->tfa98xx_device.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->tfa98xx_device.name = TFA98XX_I2C_NAME;
	tfa98xx->tfa98xx_device.fops = &tfa98xx_dev_fops;

	pr_info("tfa98xx: Registering misc device \"%s\" with dynamic minor number\n", TFA98XX_I2C_NAME);
	
	ret = misc_register(&tfa98xx->tfa98xx_device);
	if (ret)
	{
		pr_err("tfa98xx %s: FAILED: misc_register failed for device \"%s\" (error %d)\n", 
			__FILE__, TFA98XX_I2C_NAME, ret);
		ret = -1;
		goto err_misc_register;
	}

	pr_info("tfa98xx: Misc device registered successfully. Device node: /dev/%s\n", TFA98XX_I2C_NAME);

	if (tfa98xx) {
		mutex_lock(&tfa98xx->lock);
		tfa98xx->deviceInit = true;
		mutex_unlock(&tfa98xx->lock);
	}

	pr_info("tfa98xx: %s completed successfully! -\n", __func__);
	return 0;

err_misc_register:
	pr_err("tfa98xx: Cleaning up after misc_register failure\n");
	misc_deregister(&tfa98xx->tfa98xx_device);
i2c_error:
	pr_err("tfa98xx: I2C error path - cleaning up resources\n");
	mutex_destroy(&tfa98xx->lock);
	kfree(tfa98xx);
	tfa98xx = NULL;
err_exit:
	pr_err("tfa98xx: Probe FAILED with error %d\n", ret);
	return ret;
}


static int tfa98xx_i2c_remove(struct i2c_client *client)
{
	struct tfa98xx_dev *tfa98xx_dev;

	PRINT_LOG("Enter %s.  %d\n", __FUNCTION__, __LINE__);
#ifdef I2C_USE_DMA
    if(I2CDMABuf_va)
    {
		pr_info("tfa98xx: Freeing DMA buffer (va=%p, pa=%pad)\n", I2CDMABuf_va, &I2CDMABuf_pa);
		dma_free_coherent(&client->dev, MAX_BUFFER_SIZE, I2CDMABuf_va, I2CDMABuf_pa);
		I2CDMABuf_va = NULL;
		I2CDMABuf_pa = 0;
    }
#endif
	tfa98xx_dev = i2c_get_clientdata(client);
	pr_info("tfa98xx: Deregistering misc device /dev/%s\n", TFA98XX_I2C_NAME);
	misc_deregister(&tfa98xx_dev->tfa98xx_device);
	mutex_destroy(&tfa98xx_dev->lock);
	kfree(tfa98xx_dev);
	pr_info("tfa98xx: Driver removed successfully\n");

	return 0;
}

static void tfa98xx_i2c_shutdown(struct i2c_client *i2c)
{
	PRINT_LOG("Enter %s. +  %4d\n", __FUNCTION__, __LINE__);
}

static const struct of_device_id tfa98xx_of_match[] = {
	{ .compatible = "mediatek,i2c_smartpa", },
	{},
};
MODULE_DEVICE_TABLE(of, tfa98xx_of_match);

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ TFA98XX_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = TFA98XX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tfa98xx_of_match,
	},
	.probe =    tfa98xx_i2c_probe,
	.remove =   tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
	.shutdown = tfa98xx_i2c_shutdown,
};

static int __init tfa98xx_modinit(void)
{
	int ret = 0;

	PRINT_LOG("Loading tfa98xx driver\n");

	ret = i2c_static_add_device(&tfa98xx_i2c_boardinfo);
    if (ret < 0) {
        pr_err("%s: add i2c device error %d\n", __FUNCTION__, ret);
        return ret;
    }

	ret = i2c_add_driver(&tfa98xx_i2c_driver);
	if (ret != 0) {
	printk("Failed to register tfa98xx I2C driver: %d\n",
	    ret);
	}
	return ret;
}
late_initcall(tfa98xx_modinit);

static void __exit tfa98xx_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);
}
module_exit(tfa98xx_exit);

MODULE_AUTHOR("Nick Li <nick.li@wpi-group.com>");
MODULE_DESCRIPTION("TFA98xx Smart Audio I2C driver");
MODULE_LICENSE("GPL");

