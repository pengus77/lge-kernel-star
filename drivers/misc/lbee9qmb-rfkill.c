/*
 * Bluetooth+WiFi Murata LBEE19QMBC rfkill power control via GPIO
 *
 * Copyright (C) 2012 LGE Inc.
 * Copyright (C) 2010 NVIDIA Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define DEBUG	1

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/lbee9qmb-rfkill.h>
#include <linux/clk.h>

struct bcm_bt_lpm {
	struct device *dev;

	struct rfkill *rfkill;

	int gpio_reset;
	int reset_delay;
	int active_low;

	int irq;

	spinlock_t lock;

	int blocked; /* 0: on, 1: off */
	//LGE_CHANGE_S [munho2.lee@lge.com] 2012-02-06 for Bluetooth bring-up
	struct clk *bt_32k_clk;     //munho2.lee@lge.com only for SU660 LGE ADD
	//LGE_CHANGE_E		
};

static int lbee9qmb_rfkill_set_power(void *data, bool blocked)
{
	struct bcm_bt_lpm *lpm = data;

	dev_dbg(lpm->dev, "%s: enabled=%d\n", __func__, !blocked);

	lpm->blocked = blocked;

	if (blocked) {
		//LGE_CHANGE_S [munho2.lee@lge.com] 2012-02-06 for Bluetooth bring-up
		if (lpm->bt_32k_clk)
			clk_disable(lpm->bt_32k_clk);
		
		gpio_set_value(lpm->gpio_reset, 0);
		dev_dbg(lpm->dev, "%s: reset low\n", __func__);
	}
	else {
		//LGE_CHANGE_S [munho2.lee@lge.com] 2012-02-06 for Bluetooth bring-up
		if (lpm->bt_32k_clk)
			clk_enable(lpm->bt_32k_clk);
		
		gpio_set_value(lpm->gpio_reset, 0);
		dev_dbg(lpm->dev, "%s: reset low\n", __func__);
		msleep(lpm->reset_delay);
		gpio_set_value(lpm->gpio_reset, 1);
		dev_dbg(lpm->dev, "%s: reset high\n", __func__);
		msleep(lpm->reset_delay);
	}

	return 0;
}

static struct rfkill_ops lbee9qmb_rfkill_ops = {
	.set_block = lbee9qmb_rfkill_set_power,
};

static int lbee9qmb_rfkill_probe(struct platform_device *pdev)
{
	struct lbee9qmb_platform_data *pdata = pdev->dev.platform_data;
	struct bcm_bt_lpm *lpm;

	int rc;
	
	dev_dbg(&pdev->dev, "%s\n", __func__);
	
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		return -ENOSYS;
	}

	lpm = kzalloc(sizeof(struct bcm_bt_lpm), GFP_KERNEL);
	if (!lpm) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		rc = -ENOMEM;
		goto err_kzalloc;
	}

	//LGE_CHANGE_S [munho2.lee@lge.com] 2012-02-06 for Bluetooth bring-up
	lpm->bt_32k_clk = NULL; /* clk_get(NULL, "blink");*/  //LGE_CHANGE_S [munho2.lee@lge.com] 2012-03-20 Sleep clock no Control 
	if (IS_ERR(lpm->bt_32k_clk)) {
		pr_warn("%s: can't find lbee9qmb_32k_clk.\
				assuming 32k clock to chip\n", __func__);
		lpm->bt_32k_clk = NULL;
	}
	//LGE_CHANGE_S

	lpm->gpio_reset = pdata->gpio_reset;

	lpm->reset_delay = pdata->delay;
	lpm->blocked = 1; /* off */

	lpm->dev = &pdev->dev;

	/* request gpios */
	rc = gpio_request(pdata->gpio_reset, "bt_reset");
	if (rc < 0) {
		dev_err(&pdev->dev, "gpio_request(bt_reset:%d) failed\n",pdata->gpio_reset);
		goto err_gpio_bt_reset;
	}

	lpm->rfkill = rfkill_alloc("lbee9qmb-rfkill", &pdev->dev,
			RFKILL_TYPE_BLUETOOTH, &lbee9qmb_rfkill_ops, lpm);
	if (!lpm->rfkill) {
		rc = -ENOMEM;
		dev_err(&pdev->dev, "%s: rfkill_alloc failed\n", __func__);
		goto err_rfkill;
	}

	rc = rfkill_register(lpm->rfkill);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: rfkill_register failed\n", __func__);
		goto err_rfkill_register;
	}

	platform_set_drvdata(pdev, lpm);

	gpio_direction_output(pdata->gpio_reset, 0);

	/* bt chip off on booting */
	lbee9qmb_rfkill_set_power(lpm, 1);

	dev_info(&pdev->dev, "%s: probed\n", __func__);
	return 0;

	rfkill_unregister(lpm->rfkill);
err_rfkill_register:
	rfkill_destroy(lpm->rfkill);
err_rfkill:
	gpio_free(pdata->gpio_reset);
err_gpio_bt_reset:
	kfree(lpm);
err_kzalloc:
	return rc;
}

static int lbee9qmb_rfkill_remove(struct platform_device *pdev)
{
	struct bcm_bt_lpm *lpm = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill_unregister(lpm->rfkill);
	rfkill_destroy(lpm->rfkill);

	gpio_free(lpm->gpio_reset);

	kfree(lpm);

	return 0;
}

static struct platform_driver lbee9qmb_rfkill_driver = {
	.probe = lbee9qmb_rfkill_probe,
	.remove = lbee9qmb_rfkill_remove,
	.driver = {
		.name = "lbee9qmb-rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init lbee9qmb_rfkill_init(void)
{
	return platform_driver_register(&lbee9qmb_rfkill_driver);
}

static void __exit lbee9qmb_rfkill_exit(void)
{
	platform_driver_unregister(&lbee9qmb_rfkill_driver);
}

module_init(lbee9qmb_rfkill_init);
module_exit(lbee9qmb_rfkill_exit);

MODULE_DESCRIPTION("Murata LBEE9QMBC rfkill");
MODULE_AUTHOR("NVIDIA/LGE");
MODULE_LICENSE("GPL");
