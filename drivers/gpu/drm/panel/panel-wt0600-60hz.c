// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct wt0600 {
    struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
    struct mipi_dsi_device *dsi[2];
    struct backlight_device *backlight;
    struct regulator *vddio;
};

static inline struct wt0600 *panel_to_ctx(struct drm_panel *panel)
{
	return container_of(panel, struct wt0600, panel);
}

static int wt0600_dcs_write(struct wt0600 *ctx, u32 command)
{
    int i, ret;

    for (i = 0; i < ARRAY_SIZE(ctx->dsi); i++) {
        ret = mipi_dsi_dcs_write(ctx->dsi[i], command, NULL, 0);
        if (ret < 0) {
            dev_err(ctx->dev, "cmd 0x%x failed for dsi = %d\n", command, i);
        }
    }

    return ret;
}

static void wt0600_reset(struct wt0600 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(55);
}

static int wt0600_on(struct wt0600 *ctx)
{
    int ret = 0;

    ctx->dsi[0]->mode_flags |= MIPI_DSI_MODE_LPM;
    ctx->dsi[1]->mode_flags |= MIPI_DSI_MODE_LPM;

    ret = wt0600_dcs_write(ctx, MIPI_DCS_SET_DISPLAY_ON);
    if (ret < 0) {
        dev_err(ctx->dev, "set_display_on cmd failed ret = %d\n", ret);
        return ret;
    }
    msleep(150);

    ret = wt0600_dcs_write(ctx, MIPI_DCS_EXIT_SLEEP_MODE);
    if (ret < 0) {
        dev_err(ctx->dev, "set_display_on cmd failed ret = %d\n", ret);
        return ret;
    }

    msleep(50);
    
	return 0;
}

static int wt0600_off(struct wt0600 *ctx)
{
    int ret = 0;

	ctx->dsi[0]->mode_flags &= ~MIPI_DSI_MODE_LPM;
    ctx->dsi[1]->mode_flags &= ~MIPI_DSI_MODE_LPM;

    ret = wt0600_dcs_write(ctx, MIPI_DCS_SET_DISPLAY_OFF);
    if (ret < 0) {
        dev_err(ctx->dev, "set_display_off cmd failed ret = %d\n", ret);
    }

    msleep(50);

    ret =  wt0600_dcs_write(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
    if (ret < 0) {
        dev_err(ctx->dev, "enter_sleep cmd failed ret = %d\n", ret);
    }
    msleep(150);

	return ret;
}

static int wt0600_prepare(struct drm_panel *panel)
{
	struct wt0600 *ctx = panel_to_ctx(panel);
	int ret;

	// ret = regulator_enable(ctx->vddio);
	// if (ret) {
	// 	dev_err(panel->dev, "failed to enable vddio regulator: %d\n", ret);
	// 	return ret;
	// }

	wt0600_reset(ctx);

	ret = wt0600_on(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
        // regulator_bulk_disable(ARRAY_SIZE(wt0600_supplies),
        //               ctx->supplies);
		return ret;
	}

	return 0;
}

static int wt0600_unprepare(struct drm_panel *panel)
{
	struct wt0600 *ctx = panel_to_ctx(panel);
	int ret;

	ret = wt0600_off(ctx);
	if (ret < 0)
		dev_err(ctx->dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    // regulator_bulk_disable(ARRAY_SIZE(wt0600_supplies),
                    //   ctx->supplies);
	return 0;
}

static const struct drm_display_mode wt0600_mode = {
    .clock = (1440 + 100 + 8 + 40) * (2560 + 8 + 2 + 7) * 60 / 1000,
    .hdisplay = 1440,
    .hsync_start = 1440 + 100,
    .hsync_end = 1440 + 100 + 8,
    .htotal = 1440 + 100 + 8 + 40,
    .vdisplay = 2560,
    .vsync_start = 2560 + 8,
    .vsync_end = 2560 + 8 + 2,
    .vtotal = 2560 + 8 + 2 + 7,
    .width_mm = 74,
    .height_mm = 131,
    .type = DRM_MODE_TYPE_DRIVER,
};

static int wt0600_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &wt0600_mode);
}

static const struct drm_panel_funcs wt0600_panel_funcs = {
	.prepare = wt0600_prepare,
	.unprepare = wt0600_unprepare,
	.get_modes = wt0600_get_modes,
};

static int wt0600_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct wt0600 *ctx;
    struct mipi_dsi_device *dsi1_device;
    struct device_node *dsi1;
    struct mipi_dsi_host *dsi1_host;

	int ret,i;

    const struct mipi_dsi_device_info info = {
        .type = "wt0600panel",
        .channel = 0,
        .node = NULL,
    };

	pr_warn("panel: wt0600 1080p probe in\n");

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
    
    ctx->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(ctx->vddio))
		return dev_err_probe(dev, PTR_ERR(ctx->vddio), "failed to get vddio regulator\n");

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "failed to get reset gpio\n");
    }

    /* Locate second dsi device */
    dsi1 = of_graph_get_remote_node(dsi->dev.of_node, 1, -1);
    if (!dsi1) {
        dev_err(dev, "failed to get remote node for dsi1_device\n");
        return -ENODEV;
    }

    dsi1_host = of_find_mipi_dsi_host_by_node(dsi1);
    of_node_put(dsi1);
    if (!dsi1_host)
        return dev_err_probe(dev, -EPROBE_DEFER, "failed to find dsi host\n");

    /* register the second DSI device */
    ctx->dsi[1] = mipi_dsi_device_register_full(dsi1_host, &info);
    if (IS_ERR(dsi1_device)) {
        dev_err(dev, "failed to create dsi device\n");
        return PTR_ERR(dsi1_device);
    }

    ctx->dev = dev;
    ctx->dsi[0] = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

    drm_panel_init(&ctx->panel, dev, &wt0600_panel_funcs, DRM_MODE_CONNECTOR_DSI);
	
	ctx->panel.prepare_prev_first = true;

    ret = drm_panel_of_backlight(&ctx->panel);
    if (ret) {
        return dev_err_probe(dev, ret, "Failed to get backlight\n");
	}
    
    drm_panel_add(&ctx->panel);

    for (i = 0; i < ARRAY_SIZE(ctx->dsi); i++) {
        ctx->dsi[i]->lanes = 4;
        ctx->dsi[i]->format = MIPI_DSI_FMT_RGB888;
        ctx->dsi[i]->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
            MIPI_DSI_CLOCK_NON_CONTINUOUS;
        ret = mipi_dsi_attach(ctx->dsi[i]);
        if (ret < 0) {
            return dev_err_probe(dev, ret, "dsi attach failed i = %d\n", i);
        }
    }

	pr_warn("panel: wt0600 1080p probe out\n");

    return 0;
}

static void wt0600_remove(struct mipi_dsi_device *dsi)
{
    struct wt0600 *ctx = mipi_dsi_get_drvdata(dsi);

    if (ctx->dsi[0])
        mipi_dsi_detach(ctx->dsi[0]);
    if (ctx->dsi[1]) {
        mipi_dsi_detach(ctx->dsi[1]);
        mipi_dsi_device_unregister(ctx->dsi[1]);
    }

    drm_panel_remove(&ctx->panel);
}

static const struct of_device_id wt0600_of_match[] = {
	{ .compatible = "ayaneo,wt0600-1080p" },
	{ }
};
MODULE_DEVICE_TABLE(of, wt0600_of_match);

static struct mipi_dsi_driver wt0600_driver = {
	.probe = wt0600_probe,
	.remove = wt0600_remove,
	.driver = {
		.name = "panel-wt0600-60hz-1080p",
		.of_match_table = wt0600_of_match,
	},
};
module_mipi_dsi_driver(wt0600_driver);

MODULE_AUTHOR("Linux Panel Generator");
MODULE_DESCRIPTION("DRM driver for wt0600 1080p video mode dsi panel");
MODULE_LICENSE("GPL");
