// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 Kancy Joe
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct wt0600_60hz {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline struct wt0600_60hz *to_wt0600_60hz(struct drm_panel *panel)
{
	return container_of(panel, struct wt0600_60hz, panel);
}

static void wt0600_60hz_reset(struct wt0600_60hz *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(55);
}

static int wt0600_60hz_on(struct wt0600_60hz *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(150);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(50);

	return 0;
}

static int wt0600_60hz_off(struct wt0600_60hz *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(50);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(150);

	return 0;
}

static int wt0600_60hz_prepare(struct drm_panel *panel)
{
	struct wt0600_60hz *ctx = to_wt0600_60hz(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	wt0600_60hz_reset(ctx);

	ret = wt0600_60hz_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int wt0600_60hz_unprepare(struct drm_panel *panel)
{
	struct wt0600_60hz *ctx = to_wt0600_60hz(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = wt0600_60hz_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode wt0600_60hz_mode = {
	.clock = (720 + 100 + 8 + 40) * (2560 + 8 + 2 + 7) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 100,
	.hsync_end = 720 + 100 + 8,
	.htotal = 720 + 100 + 8 + 40,
	.vdisplay = 2560,
	.vsync_start = 2560 + 8,
	.vsync_end = 2560 + 8 + 2,
	.vtotal = 2560 + 8 + 2 + 7,
	.width_mm = 0,
	.height_mm = 0,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int wt0600_60hz_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &wt0600_60hz_mode);
}

static const struct drm_panel_funcs wt0600_60hz_panel_funcs = {
	.prepare = wt0600_60hz_prepare,
	.unprepare = wt0600_60hz_unprepare,
	.get_modes = wt0600_60hz_get_modes,
};

static int wt0600_60hz_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct wt0600_60hz *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &wt0600_60hz_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void wt0600_60hz_remove(struct mipi_dsi_device *dsi)
{
	struct wt0600_60hz *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id wt0600_60hz_of_match[] = {
	{ .compatible = "ayaneo,wt0600-2k" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, wt0600_60hz_of_match);

static struct mipi_dsi_driver wt0600_60hz_driver = {
	.probe = wt0600_60hz_probe,
	.remove = wt0600_60hz_remove,
	.driver = {
		.name = "panel-wt0600-2k60hz",
		.of_match_table = wt0600_60hz_of_match,
	},
};
module_mipi_dsi_driver(wt0600_60hz_driver);

MODULE_AUTHOR("Kancy Joe <sunflower2333@outlook.com>");
MODULE_DESCRIPTION("DRM driver for wt0600 2k video mode dsi panel");
MODULE_LICENSE("GPL");
