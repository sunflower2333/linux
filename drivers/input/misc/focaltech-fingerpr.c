// SPDX-License-Identifier: GPL-2.0-only
/*
 * Focaltech Fingerprint input driver.
 *
 * Copyright (c) 2025, Kancy Joe <kancy2333@outlook.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/wait.h>

#define FOCAL_FPR_DRIVER_NAME 

struct focal_fpr {
	struct device *fpr_dev;
	struct input_dev *input;
	struct platform_device *pdev;
	struct gpio_desc *reset_gpio;
	struct work_struct event_work;
	wait_queue_head_t wait_queue_head;
    struct wakeup_source *p_ws;
    struct wakeup_source *p_ws_ctl;
	int irq_num;
	bool event_flag;
};

static void focal_fpr_work_device_event(struct work_struct *ws)
{
	struct focal_fpr *ffpr_ctx = container_of(ws, struct focal_fpr, event_work);
    __pm_wakeup_event(ffpr_ctx->p_ws, jiffies_to_msecs(2000));
    dev_info(&ffpr_ctx->pdev->dev, "irq: %d trigger\n", ffpr_ctx->irq_num);
}

static irqreturn_t focal_fpr_irq_handler(int irq, void *dev_id)
{
	struct focal_fpr *ffpr_ctx = (struct focal_fpr *)dev_id;
	ffpr_ctx->event_flag = true;
    if(ffpr_ctx)
        schedule_work(&ffpr_ctx->event_work);

    return IRQ_HANDLED;
}

static int focal_fpr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct focal_fpr *ffpr_ctx = NULL;

	ffpr_ctx = devm_kzalloc(&pdev->dev, sizeof(*ffpr_ctx), GFP_KERNEL);
	if (!ffpr_ctx)
		return -ENOMEM;

	ffpr_ctx->pdev = pdev;
	platform_set_drvdata(pdev, ffpr_ctx);

	// Get reset gpio
	ffpr_ctx->reset_gpio =
		devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ffpr_ctx->reset_gpio)) {
		dev_err(&pdev->dev, "Failed to get reset gpio\n");
		return PTR_ERR(ffpr_ctx->reset_gpio);
	}

	// Driver reset gpio output high to exit reset mode
	gpiod_set_value(ffpr_ctx->reset_gpio, 1);

	// Get irq
	ffpr_ctx->irq_num = platform_get_irq(pdev, 0);
	if (ffpr_ctx->irq_num < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		return ffpr_ctx->irq_num;
	}

	// Setup workqueue for irq
	INIT_WORK(&ffpr_ctx->event_work, focal_fpr_work_device_event);
	init_waitqueue_head(&ffpr_ctx->wait_queue_head);
	ffpr_ctx->event_flag = false;

	// Register isr
	ret = devm_request_irq(&pdev->dev, ffpr_ctx->irq_num,
			       focal_fpr_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			       "focaltech fingerpr irq", ffpr_ctx);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", ret);
		return ret;
	}
    
    // Enable irq wake
    enable_irq_wake(ffpr_ctx->irq_num);

	return 0;
}

static const struct of_device_id focal_fpr_match[] = {
	{
		.compatible = "focaltech,fingerpr",
	},
	{},
};
MODULE_DEVICE_TABLE(of, focal_fpr_match);

static struct platform_driver focal_fpr_driver = {
	.driver		= {
		.name	= "focaltech-fingerpr",
        .owner = THIS_MODULE,
		.of_match_table = of_match_ptr(focal_fpr_match),
	},
	.probe		= focal_fpr_probe,
    // .remove     = focal_fpr_remove,
};

module_platform_driver(focal_fpr_driver);

MODULE_AUTHOR("Kancy Joe <kancy2333@outlook.com>");
MODULE_DESCRIPTION("Focaltech Fingerprint input driver");
MODULE_LICENSE("GPL v2");
