/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef _XE_PLATFORM_INFO_TYPES_H_
#define _XE_PLATFORM_INFO_TYPES_H_

/*
 * Keep this in graphics version based order and chronological order within a
 * version
 */
enum xe_platform {
	XE_PLATFORM_UNINITIALIZED = 0,
	XE_TIGERLAKE,
	XE_ROCKETLAKE,
	XE_ALDERLAKE_S,
	XE_ALDERLAKE_P,
	XE_ALDERLAKE_N,
	XE_DG1,
	XE_DG2,
	XE_PVC,
	XE_METEORLAKE,
	XE_LUNARLAKE,
	XE_BATTLEMAGE,
	XE_PANTHERLAKE,
};

enum xe_subplatform {
	XE_SUBPLATFORM_UNINITIALIZED = 0,
	XE_SUBPLATFORM_NONE,
	XE_SUBPLATFORM_ALDERLAKE_P_RPLU,
	XE_SUBPLATFORM_ALDERLAKE_S_RPLS,
	XE_SUBPLATFORM_DG2_G10,
	XE_SUBPLATFORM_DG2_G11,
	XE_SUBPLATFORM_DG2_G12,
};

#endif
