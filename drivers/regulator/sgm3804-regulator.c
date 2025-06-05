#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/gpio/consumer.h>

struct sgm3804_data {
    struct regmap *regmap;
    struct gpio_desc *reset_gpio[2];
    bool enabled;
};

static const struct regmap_config sgm3804_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0x03,
};

static int sgm3804_enable(struct regulator_dev *rdev)
{
    struct sgm3804_data *data = rdev_get_drvdata(rdev);
    struct regmap *regmap = data->regmap;
    int ret;

    pr_info("sgm3804_enable: called\n");

    /* Set reset GPIO high to enable the device if available */
    if (data->reset_gpio[0]) {
        gpiod_set_value_cansleep(data->reset_gpio[0], 1);
    }
    if (data->reset_gpio[1]) {
        gpiod_set_value_cansleep(data->reset_gpio[1], 1);
    }

    ret = regmap_write(regmap, 0x00, 0x0c);
    if (ret) {
        pr_err("sgm3804_enable: regmap_write 0x00 failed: %d\n", ret);
        return ret;
    }

    ret = regmap_write(regmap, 0x01, 0x0c);
    if (ret) {
        pr_err("sgm3804_enable: regmap_write 0x01 failed: %d\n", ret);
        return ret;
    }

    ret = regmap_write(regmap, 0x03, 0x03);
    if (ret) {
        pr_err("sgm3804_enable: regmap_write 0x03 failed: %d\n", ret);
        return ret;
    }

    pr_info("sgm3804_enable: success\n");
    data->enabled = true;
    return 0;
}

static int sgm3804_disable(struct regulator_dev *rdev)
{
    struct sgm3804_data *data = rdev_get_drvdata(rdev);

    pr_info("sgm3804_disable: called\n");

    if (data->reset_gpio[0]) {
        gpiod_set_value_cansleep(data->reset_gpio[0], 0);
    }
    if (data->reset_gpio[1]) {
        gpiod_set_value_cansleep(data->reset_gpio[1], 0);
    }

    pr_info("sgm3804_disable: success\n");
    data->enabled = false;
    return 0;
}

static int sgm3804_is_enabled(struct regulator_dev *rdev)
{
    struct sgm3804_data *data = rdev_get_drvdata(rdev);
    return data->enabled;
}

static int sgm3804_get_voltage(struct regulator_dev *rdev)
{
    return 5000000;  // 5V
}

static const struct regulator_ops sgm3804_ops = {
    .enable = sgm3804_enable,
    .disable = sgm3804_disable,
    .is_enabled = sgm3804_is_enabled,
    .get_voltage = sgm3804_get_voltage,
};

static const struct regulator_desc sgm3804_reg = {
    .name = "SGM3804",
    .id = 0,
    .ops = &sgm3804_ops,
    .type = REGULATOR_VOLTAGE,
    .n_voltages = 1,
    .min_uV = 5000000,
    .owner = THIS_MODULE,
};

static int sgm3804_i2c_probe(struct i2c_client *i2c)
{
    struct device *dev = &i2c->dev;
    struct regulator_config config = { };
    struct regulator_dev *rdev;
    struct sgm3804_data *data;
    int error;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->regmap = devm_regmap_init_i2c(i2c, &sgm3804_regmap_config);
    if (IS_ERR(data->regmap))
        return dev_err_probe(dev, PTR_ERR(data->regmap), "failed to init regmap\n");

    /* Get reset-gpio from device tree */
    data->reset_gpio[0] = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
    if (IS_ERR(data->reset_gpio[0]))
        return dev_err_probe(dev, PTR_ERR(data->reset_gpio[0]), "failed to get first reset GPIO\n");

    data->reset_gpio[1] = devm_gpiod_get_index(dev, "reset", 1, GPIOD_OUT_HIGH);
    if (IS_ERR(data->reset_gpio[1]))
        dev_warn(dev, "failed to get second reset GPIO\n");

    config.dev = dev;
    config.regmap = data->regmap;
    config.driver_data = data;
    config.of_node = dev->of_node;
    config.init_data = of_get_regulator_init_data(dev, dev->of_node, &sgm3804_reg);
    if (!config.init_data)
        return -ENOMEM;
    data->enabled = false;
    rdev = devm_regulator_register(dev, &sgm3804_reg, &config);
    if (IS_ERR(rdev)) {
        error = PTR_ERR(rdev);
        dev_err(dev, "Failed to register SGM3804 regulator: %d\n", error);
        return error;
    }

    return 0;
}

static const struct i2c_device_id sgm3804_i2c_id[] = {
    { "sgm3804" },
    { }
};
MODULE_DEVICE_TABLE(i2c, sgm3804_i2c_id);

static const struct of_device_id sgm3804_i2c_of_match[] = {
    { .compatible = "sgmicro,sgm3804" },
    { }
};
MODULE_DEVICE_TABLE(of, sgm3804_i2c_of_match);

static struct i2c_driver sgm3804_regulator_driver = {
    .driver = {
        .name = "sgm3804",
        .probe_type = PROBE_PREFER_ASYNCHRONOUS,
        .of_match_table = sgm3804_i2c_of_match,
    },
    .probe = sgm3804_i2c_probe,
    .id_table = sgm3804_i2c_id,
};

module_i2c_driver(sgm3804_regulator_driver);

MODULE_DESCRIPTION("SGMicro sgm3804 regulator Driver");
MODULE_AUTHOR("Kancy Joe <kancy2333@outlook.com>");
MODULE_LICENSE("GPL");