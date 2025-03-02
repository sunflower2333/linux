// SPDX-License-Identifier: GPL-2.0-only
/*
 * Backlight driver for the Silergy sy7758vgs
 *
 * Copyright (C) 2025 Kancy Joe <kancy2333@outlook.com>
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define DEFAULT_BRIGHTNESS 1500
#define MAX_BRIGHTNESS 4080
#define REG_MAX 0xa9

/* reg */
#define BL_BRT_L 0x10
#define BL_BRT_H 0x11

struct sy7758vgs {
	struct i2c_client *client;
	struct regmap *regmap;
	bool led_on;
};

static void sy7758vgs_init(struct sy7758vgs *sydev);

static const struct regmap_config sy7758vgs_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static int sy7758vgs_write(struct sy7758vgs *sydev, unsigned int reg,
			 unsigned int val)
{
	return regmap_write(sydev->regmap, reg, val);
}

static int sy7758vgs_read(struct sy7758vgs *sydev, unsigned int reg,
			 unsigned int *val)
{
	return regmap_read(sydev->regmap, reg, val);
}

static int sy7758vgs_backlight_update_status(struct backlight_device *backlight_dev)
{
	struct sy7758vgs *sydev = bl_get_data(backlight_dev);
	unsigned int brightness = backlight_get_brightness(backlight_dev);

	if (!sydev->led_on && brightness > 0) {
		// Init
		sy7758vgs_init(sydev);
		sydev->led_on = true;
	} else if (brightness == 0) {
		sydev->led_on = false;
	}

	/* Set brightness */
	sy7758vgs_write(sydev, BL_BRT_L, brightness & 0xf0);
	sy7758vgs_write(sydev, BL_BRT_H, (brightness >> 8) & 0xf);

	return 0;
}

static const struct backlight_ops sy7758vgs_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = sy7758vgs_backlight_update_status,
};

static void sy7758vgs_init(struct sy7758vgs *sydev)
{
	unsigned int val = 0;
	sydev->led_on = true;

    sy7758vgs_read(sydev, 0x03, &val);
    if(val != 0x63)
		pr_warn("no.1 sy7758vgs init 0x63\n");

	sy7758vgs_read(sydev, 0x01, &val);
	if(val != 0x01)
		pr_warn("no.2 sy7758vgs init 0x01\n");

	sy7758vgs_read(sydev, 0xa5, &val);
	if(val != 0x00)
		pr_warn("no.3 sy7758vgs init 0x00\n");

	sy7758vgs_write(sydev, 0x01, 0x85);
	sy7758vgs_write(sydev, 0x10, 0x00);
	sy7758vgs_write(sydev, 0x11, 0x00);
	sy7758vgs_write(sydev, 0xa5, 0x64);
	sy7758vgs_write(sydev, 0xa0, 0x55);
	sy7758vgs_write(sydev, 0xa1, 0x9a);
	sy7758vgs_write(sydev, 0xa9, 0x80);
	sy7758vgs_write(sydev, 0xa2, 0x28);

	sy7758vgs_read(sydev, 0x01, &val);
	if(val != 0x85)
		pr_warn("no.4 sy7758vgs init 0x85\n");

	sy7758vgs_read(sydev, 0x10, &val);
	if(val != 0x00)
		pr_warn("no.5 sy7758vgs init 0x00\n");

	sy7758vgs_read(sydev, 0x11, &val);
	if(val != 0x00)
		pr_warn("no.6 sy7758vgs init 0x00\n");

	sy7758vgs_read(sydev, 0xa5, &val);
	if(val != 0x64)
		pr_warn("no.7 sy7758vgs init 0x64\n");

	sy7758vgs_read(sydev, 0xa0, &val);
	if(val != 0x55)
		pr_warn("no.8 sy7758vgs init 0x55\n");

	sy7758vgs_read(sydev, 0xa1, &val);
	if(val != 0x9a)
		pr_warn("no.9 sy7758vgs init 0x9a\n");

	sy7758vgs_read(sydev, 0xa2, &val);
	if(val != 0x28)
		pr_warn("no.10 sy7758vgs init 0x28\n");

	sy7758vgs_read(sydev, 0xa9, &val);
	if(val != 0x80)
		pr_warn("no.11 sy7758vgs init 0x80\n");

	sy7758vgs_read(sydev, 0x02, &val);
	if(val != 0x3c)
		pr_warn("no.12 sy7758vgs init 0x3c\n");

	sy7758vgs_write(sydev, 0x10, 0x00);
	sy7758vgs_write(sydev, 0x11, 0x00);
	sy7758vgs_write(sydev, 0x10, 0x80);
	sy7758vgs_write(sydev, 0x11, 0x02);
	// Max brightness
	// 0x10: 0xf0 Low
	// 0x11: 0x0f High

	// Min brightness
	// 0x10: 0x10  Low
	// 0x11: 0x00  High
}

static int sy7758vgs_probe(struct i2c_client *client)
{
	struct backlight_device *backlight_dev;
	struct backlight_properties props;
	struct sy7758vgs *sydev;

	sydev = devm_kzalloc(&client->dev, sizeof(*sydev), GFP_KERNEL);
	if (!sydev)
		return -ENOMEM;

	sydev->client = client;
	sydev->regmap = devm_regmap_init_i2c(client, &sy7758vgs_regmap_config);
	if (IS_ERR(sydev->regmap))
		return dev_err_probe(&client->dev, PTR_ERR(sydev->regmap), "failed to init regmap\n");

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = DEFAULT_BRIGHTNESS;
	props.scale = BACKLIGHT_SCALE_LINEAR;

	backlight_dev = devm_backlight_device_register(&client->dev, "sy7758vgs-backlight",
					&client->dev, sydev, &sy7758vgs_backlight_ops, &props);
	if (IS_ERR(backlight_dev))
		return dev_err_probe(&client->dev, PTR_ERR(backlight_dev),
				"failed to register backlight device\n");

	sy7758vgs_init(sydev);

	i2c_set_clientdata(client, backlight_dev);
	backlight_update_status(backlight_dev);

	return 0;
}

static void sy7758vgs_remove(struct i2c_client *client)
{
	struct backlight_device *backlight_dev = i2c_get_clientdata(client);
	backlight_dev->props.brightness = 0;
	backlight_update_status(backlight_dev);
}

static const struct i2c_device_id sy7758vgs_ids[] = {
	{ "sy7758vgs" },
	{}
};
MODULE_DEVICE_TABLE(i2c, sy7758vgs_ids);

static const struct of_device_id sy7758vgs_match_table[] = {
	{
		.compatible = "silergy,sy7758vgs",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sy7758vgs_match_table);

static struct i2c_driver sy7758vgs_driver = {
	.driver = {
		.name = "sy7758vgs",
		.of_match_table = sy7758vgs_match_table,
	},
	.probe = sy7758vgs_probe,
	.remove = sy7758vgs_remove,
	.id_table = sy7758vgs_ids,
};

module_i2c_driver(sy7758vgs_driver);

MODULE_DESCRIPTION("Silergy sy7758vgs Backlight Driver");
MODULE_AUTHOR("Kancy Joe <kancy2333@outlook.com>");
MODULE_LICENSE("GPL");
