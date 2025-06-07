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
#include <linux/pinctrl/consumer.h>
#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct wt0600 {
    struct device *dev;
	struct drm_panel panel;
    struct mipi_dsi_device *dsi[2];
    struct backlight_device *backlight;
    struct regulator *vddio;
    struct regulator *avdd;
    /* Pinctrls */
    struct gpio_desc *reset_gpio;
    struct pinctrl *pinctrl;
    struct pinctrl_state *state_default;
    struct pinctrl_state *state_suspend;
    enum drm_panel_orientation orientation;
    /* DSI device info */
    struct panel_desc *desc;
};

struct panel_desc {
	const struct mipi_dsi_device_info dsi_info;
};

static struct panel_desc wt0600_desc = {
    .dsi_info = {
        .type = "wt0600-2k",
        .channel = 0,
        .node = NULL,
    }
};

static inline struct wt0600 *panel_to_ctx(struct drm_panel *panel)
{
	return container_of(panel, struct wt0600, panel);
}

static void wt0600_reset(struct wt0600 *ctx)
{
    gpiod_set_value_cansleep(ctx->reset_gpio, 0);
    usleep_range(10000, 11000);
    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    usleep_range(10000, 11000);
    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    msleep(55);
}

static int wt0600_on(struct wt0600 *ctx)
{
    struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi[0] };
    struct mipi_dsi_multi_context dsi_ctx1 = { .dsi = ctx->dsi[1] };
    mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
    mipi_dsi_dcs_set_display_on_multi(&dsi_ctx1);
    mipi_dsi_msleep(&dsi_ctx, 150);
    mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
    mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx1);
    mipi_dsi_msleep(&dsi_ctx, 50);
    return dsi_ctx.accum_err;
}

static int wt0600_off(struct drm_panel *panel)
{
    struct wt0600 *ctx = panel_to_ctx(panel);
    struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi[0] };
    struct mipi_dsi_multi_context dsi_ctx1 = { .dsi = ctx->dsi[1] };
    mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
    mipi_dsi_dcs_set_display_off_multi(&dsi_ctx1);
    mipi_dsi_msleep(&dsi_ctx, 50);
    mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
    mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx1);
    mipi_dsi_msleep(&dsi_ctx, 150);
    return dsi_ctx.accum_err;
}

static int wt0600_prepare(struct drm_panel *panel)
{
	struct wt0600 *ctx = panel_to_ctx(panel);
	int ret = 0;
    
    // enable power
	ret = regulator_enable(ctx->avdd);
	if (ret) {
		dev_err(panel->dev, "failed to enable avdd regulator: %d\n", ret);
		return ret;
	}
    msleep(20);
	wt0600_reset(ctx);

	ret = wt0600_on(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "failed to initialize panel: %d\n", ret);
		// gpiod_set_value_cansleep(ctx->reset_gpio, 1);
        // regulator_disable(ctx->vddio);
		return ret;
	}

    return 0;
}

static int wt0600_unprepare(struct drm_panel *panel)
{
	struct wt0600 *ctx = panel_to_ctx(panel);
	int ret = 0;

    // Enter suspend mode 
    // pinctrl_select_state(ctx->pinctrl, ctx->state_suspend);

    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    // regulator_disable(ctx->avdd);
	return ret;
}

static const struct drm_display_mode wt0600_mode = {
    .clock = 2*(720 + 100 + 8 + 40) * (2560 + 15 + 2 + 8) * 60 / 1000,
    .hdisplay = 2*720,
    .hsync_start = 2*(720 + 100),
    .hsync_end = 2*(720 + 100 + 8),
    .htotal = 2*(720 + 100 + 8 + 40),
    .vdisplay = 2560,
    .vsync_start = 2560 + 15,
    .vsync_end = 2560 + 15 + 2,
    .vtotal = 2560 + 15 + 2 + 8,
    .width_mm = 74,
    .height_mm = 131,
    .type = DRM_MODE_TYPE_DRIVER,
};

static int wt0600_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &wt0600_mode);
}

static enum drm_panel_orientation nt36532e_get_orientation(struct drm_panel *panel)
{
	struct wt0600 *ctx = panel_to_ctx(panel);

	return ctx->orientation;
}

static const struct drm_panel_funcs wt0600_panel_funcs = {
    .disable = wt0600_off,
	.prepare = wt0600_prepare,
	.unprepare = wt0600_unprepare,
	.get_modes = wt0600_get_modes,
	.get_orientation = nt36532e_get_orientation,
};

static int wt0600_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct wt0600 *ctx;
    struct device_node *dsi1;
    struct mipi_dsi_host *dsi1_host;

	int ret=0,i=0;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
    
    // ctx->pinctrl = devm_pinctrl_get(dev);
    // if (IS_ERR(ctx->pinctrl))
    //     return dev_err_probe(dev, PTR_ERR(ctx->pinctrl), "failed to get pinctrl\n");

    // ctx->state_default = pinctrl_lookup_state(ctx->pinctrl, "default");
    // if (IS_ERR(ctx->state_default))
    //     return dev_err_probe(dev, PTR_ERR(ctx->state_default), "no default pinctrl state\n");

    // ctx->state_suspend = pinctrl_lookup_state(ctx->pinctrl, "suspend");
    // if (IS_ERR(ctx->state_suspend))
    //     return dev_err_probe(dev, PTR_ERR(ctx->state_suspend), "no suspend pinctrl state\n");


    // ctx->vddio = devm_regulator_get(dev, "vddio");
	// if (IS_ERR(ctx->vddio))
	// 	return dev_err_probe(dev, PTR_ERR(ctx->vddio), "failed to get vddio regulator\n");

    ctx->avdd = devm_regulator_get(dev, "avdd");
	if (IS_ERR(ctx->avdd))
		return dev_err_probe(dev, PTR_ERR(ctx->avdd), "failed to get avdd regulator\n");

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "failed to get reset gpio\n");
    }

    /* Get panel data */
    ctx->desc = (struct panel_desc *)of_device_get_match_data(dev);
	if (!ctx->desc){
        dev_err(dev, "failed to get panel description\n");
		return -ENODEV;
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
    ctx->dsi[1] = devm_mipi_dsi_device_register_full(dev, dsi1_host, &ctx->desc->dsi_info);
    if (IS_ERR(ctx->dsi[1])) {
        dev_err(dev, "failed to create dsi device\n");
        return PTR_ERR(ctx->dsi[1]);
    }

    /* Orientation */
    ret = of_drm_get_panel_orientation(dev->of_node, &ctx->orientation);
	if (ret < 0) {
		dev_err(dev, "failed to get orientation %d\n", ret);
		return ret;
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
        ctx->dsi[i]->mode_flags = MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO |
                                MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;
        ret = mipi_dsi_attach(ctx->dsi[i]);
        if (ret < 0) {
            return dev_err_probe(dev, ret, "dsi attach failed i = %d\n", i);
        }
    }
    return 0;
}

static void wt0600_remove(struct mipi_dsi_device *dsi)
{
    struct wt0600 *ctx = mipi_dsi_get_drvdata(dsi);
    drm_panel_remove(&ctx->panel);
}

static const struct of_device_id wt0600_of_match[] = {
	{ 
        .compatible = "ayaneo,wt0600-2k",
        .data = &wt0600_desc,
    },
	{}
};
MODULE_DEVICE_TABLE(of, wt0600_of_match);

static struct mipi_dsi_driver wt0600_driver = {
	.probe = wt0600_probe,
	.remove = wt0600_remove,
	.driver = {
		.name = "panel-wt0600-2k",
		.of_match_table = wt0600_of_match,
	},
};
module_mipi_dsi_driver(wt0600_driver);

MODULE_AUTHOR("Kancy Joe <kancy2333@outlook.com>");
MODULE_DESCRIPTION("DRM driver for wt0600 2k video mode dsi panel");
MODULE_LICENSE("GPL");
