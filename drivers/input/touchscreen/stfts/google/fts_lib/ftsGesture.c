/*
  *
  **************************************************************************
  **                        STMicroelectronics				 **
  **************************************************************************
  **                        marco.cali@st.com				**
  **************************************************************************
  *                                                                        *
  *                     FTS Gesture Utilities				**
  *                                                                        *
  **************************************************************************
  **************************************************************************
  *
  */

/*!
  * \file ftsGesture.c
  * \brief Contains all the functions and variable to handle the Gesture
  * Detection features
  */

#include "ftsSoftware.h"
#include "ftsCore.h"
#include "ftsError.h"
#include "ftsGesture.h"
#include "ftsIO.h"
#include "ftsTime.h"
#include "ftsTool.h"

/* /< store the gesture bitmask which the host want to enable.
 * If bit set 1 the corresponding gesture will be detected in Gesture Mode */
static u8 gesture_mask[GESTURE_MASK_SIZE] = { 0 };
/* /< store the x coordinates of the points draw by the user
 * when a gesture is detected */
u16 gesture_coordinates_x[GESTURE_MAX_COORDS_PAIRS_REPORT] = { 0 };
/* /< store the y coordinates of the points draw by the user
 * when a gesture is detected */
u16 gesture_coordinates_y[GESTURE_MAX_COORDS_PAIRS_REPORT] = { 0 };
/* /< number of coordinates pairs (points) reported with the detected gesture */
int gesture_coords_reported = ERROR_OP_NOT_ALLOW;
static u8 refreshGestureMask;	/* /< flag which indicate if there is
				 * the need to set the gesture mask in the FW */
struct mutex gestureMask_mutex;	/* /< mutex used to control access on gesture
				 * shared variables */

/**
  * Update the gesture mask stored in the driver and have to be used in gesture
  * mode
  * @param mask pointer to a byte array which store the gesture mask update
  * that want to be performed.
  * @param size dimension in byte of mask. This size can be <=
  * GESTURE_MASK_SIZE.
  * If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos
  * and starting from the less significant byte.
  * @param en 0 = enable the gestures set in mask, 1 = disable the gestures set
  * in mask
  * @return OK if success or an error code which specify the type of error
  */
int updateGestureMask(u8 *mask, int size, int en)
{
	u8 temp;
	int i;

	if (mask != NULL) {
		if (size <= GESTURE_MASK_SIZE) {
			if (en == FEAT_ENABLE) {
				mutex_lock(&gestureMask_mutex);
				pr_info("updateGestureMask: setting gesture mask to enable...\n");
				if (mask != NULL)
					for (i = 0; i < size; i++)
						gesture_mask[i] =
							gesture_mask[i] |
							mask[i];
				/* back up of the gesture enabled */
				refreshGestureMask = 1;
				pr_info("updateGestureMask: gesture mask to enable SET!\n");
				mutex_unlock(&gestureMask_mutex);
				return OK;
			} else if (en == FEAT_DISABLE) {
				mutex_lock(&gestureMask_mutex);
				pr_info("updateGestureMask: setting gesture mask to disable...\n");
				for (i = 0; i < size; i++) {
					temp = gesture_mask[i] ^ mask[i];
					/* enabled XOR disabled */
					gesture_mask[i] = temp &
							  gesture_mask[i];
					/* temp AND enabled
					 * disable the gestures that were
					 * enabled */
				}
				pr_info("updateGestureMask: gesture mask to disable SET!\n");
				refreshGestureMask = 1;
				mutex_unlock(&gestureMask_mutex);
				return OK;
			} else {
				pr_err("%s: Enable parameter Invalid! %d != %d or %d ERROR %08X\n",
					__func__,
					en,
					FEAT_DISABLE, FEAT_ENABLE,
					ERROR_OP_NOT_ALLOW);
				return ERROR_OP_NOT_ALLOW;
			}
		} else {
			pr_err("updateGestureMask: Size not valid! %d > %d ERROR %08X\n",
				size, GESTURE_MASK_SIZE, ERROR_OP_NOT_ALLOW);
			return ERROR_OP_NOT_ALLOW;
		}
	} else {
		pr_err("updateGestureMask: Mask NULL! ERROR %08X\n",
			ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
  * Enable in the FW the gesture mask to be used in gesture mode
  * @param mask pointer to a byte array which store the gesture mask update
  * that want to be sent to the FW, if NULL, will be used gesture_mask
  * set previously without any changes.
  * @param size dimension in byte of mask. This size can be <=
  * GESTURE_MASK_SIZE.
  * If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos and
  * starting from the less significant byte.
  * @return OK if success or an error code which specify the type of error
  */
int enableGesture(struct fts_ts_info *info, u8 *mask, int size)
{
	int i, res;

	pr_info("Trying to enable gesture...\n");

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL)
			for (i = 0; i < size; i++)
				gesture_mask[i] = gesture_mask[i] | mask[i];
		/* back up of the gesture enabled */

		res = setFeatures(info, FEAT_SEL_GESTURE, gesture_mask,
				  GESTURE_MASK_SIZE);
		if (res < OK) {
			pr_err("enableGesture: ERROR %08X\n", res);
			goto END;
		}

		pr_info("enableGesture DONE!\n");
		res = OK;

END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		pr_err("enableGesture: Size not valid! %d > %d ERROR %08X\n",
			size, GESTURE_MASK_SIZE, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
  * Disable in the FW the gesture mask to be used in gesture mode
  * @param mask pointer to a byte array which store the gesture mask update that
  * want to be sent to the FW, if NULL, all the gestures will be disabled.
  * @param size dimension in byte of mask. This size can be <=
  * GESTURE_MASK_SIZE.
  * If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos and
  * starting from the less significant byte.
  * @return OK if success or an error code which specify the type of error
  */
int disableGesture(struct fts_ts_info *info, u8 *mask, int size)
{
	u8 temp;
	int i, res;
	u8 *pointer;


	pr_info("Trying to disable gesture...\n");


	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size; i++) {
				temp = gesture_mask[i] ^ mask[i];
				/* enabled mask XOR disabled mask */
				gesture_mask[i] = temp & gesture_mask[i];
				/* temp AND enabled
				 * disable the gestures that are specified and
				 * previously enabled */
			}

			pointer = gesture_mask;
		} else {
			i = 0;	/* if NULL is passed disable all the possible
				 * gestures */
			pointer = (u8 *)&i;
		}

		res = setFeatures(info, FEAT_SEL_GESTURE, pointer, GESTURE_MASK_SIZE);
		if (res < OK) {
			pr_err("disableGesture: ERROR %08X\n", res);
			goto END;
		}

		pr_info("disableGesture DONE!\n");

		res = OK;

END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		pr_err("disableGesture: Size not valid! %d > %d ERROR %08X\n",
			size, GESTURE_MASK_SIZE, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
  * Perform all the steps required to put the chip in gesture mode
  * @param reload if set to 1, before entering in gesture mode it will re-enable
  * in the FW the last defined gesture mask
  * @return OK if success or an error code which specify the type of error
  */
int enterGestureMode(struct fts_ts_info *info, int reload)
{
	int res, ret;

	res = fts_enableInterrupt(info, false);
	if (res < OK) {
		pr_err("enterGestureMode: ERROR %08X\n",
			res | ERROR_DISABLE_INTER);
		return res | ERROR_DISABLE_INTER;
	}

	if (reload == 1 || refreshGestureMask == 1) {
		res = enableGesture(info, NULL, 0);
		if (res < OK) {
			pr_err("enterGestureMode: enableGesture ERROR %08X\n",
				res);
			goto END;
		}

		refreshGestureMask = 0;
	}

	res = setScanMode(info, SCAN_MODE_LOW_POWER, 0);
	if (res < OK) {
		pr_err("enterGestureMode: enter gesture mode ERROR %08X\n",
			res);
		goto END;
	}

	res = OK;
END:
	ret = fts_enableInterrupt(info, true);
	if (ret < OK) {
		pr_err("enterGestureMode: fts_enableInterrupt ERROR %08X\n",
			res | ERROR_ENABLE_INTER);
		res |= ret | ERROR_ENABLE_INTER;
	}


	return res;
}

/**
  * Check if one or more Gesture IDs are currently enabled in gesture_mask
  * @return FEAT_ENABLE if one or more gesture ids are enabled, FEAT_DISABLE if
  * all the gesture ids are currently disabled
  */
int isAnyGestureActive(void)
{
	int res = 0;

	while (res < (GESTURE_MASK_SIZE - 1) && gesture_mask[res] == 0)
		/* -1 because in any case the last gesture mask byte will
		 * be evaluated with the following if */
		res++;

	if (gesture_mask[res] != 0) {
		pr_info("%s: Active Gestures Found! gesture_mask[%d] = %02X !\n",
			__func__, res, gesture_mask[res]);
		return FEAT_ENABLE;
	} else {
		pr_info("%s: All Gestures Disabled!\n", __func__);
		return FEAT_DISABLE;
	}
}


/**
  * Read from the frame buffer the gesture coordinates pairs of the points draw
  * by an user when a gesture is detected
  * @param event pointer to a byte array which contains the gesture event
  * reported
  * by the fw when a gesture is detected
  * @return OK if success or an error code which specify the type of error
  */
int readGestureCoords(struct fts_ts_info *info, u8 *event)
{
	int i = 0;
	u64 address = 0;
	int res;

	u8 val[GESTURE_MAX_COORDS_PAIRS_REPORT * 4];

	/* the max coordinates to read are GESTURE_COORDS_REPORT_MAX*4
	 * (because each coordinate is a short(*2) and we have x and y) */


	if (event[0] == EVT_ID_USER_REPORT &&
	    event[1] == EVT_TYPE_USER_GESTURE) {
		address = (event[4] << 8) | event[3]; /* Offset in framebuff */
		gesture_coords_reported = event[5];	/* number of pairs
							 * coords reported */
		if (gesture_coords_reported > GESTURE_MAX_COORDS_PAIRS_REPORT) {
			pr_err("%s:  FW reported more than %d points for the gestures! Decreasing to %d\n",
				__func__, gesture_coords_reported,
				GESTURE_MAX_COORDS_PAIRS_REPORT);
			gesture_coords_reported =
				GESTURE_MAX_COORDS_PAIRS_REPORT;
		}

		pr_info("%s: Offset: %llx , coords pairs = %d\n",
			 __func__, address, gesture_coords_reported);

		res = fts_writeReadU8UX(info, FTS_CMD_FRAMEBUFFER_R, BITS_16,
					address, val,
					(gesture_coords_reported * 2 * 2),
					DUMMY_FRAMEBUFFER);
		/* *2 because each coord is made by 2 bytes,
		 * *2 because there are x and y */
		if (res < OK) {
			pr_err("%s: Cannot read the coordinates! ERROR %08X\n",
				__func__, res);
			gesture_coords_reported = ERROR_OP_NOT_ALLOW;
			return res;
		}

		/* all the points of the gesture are stored in val */
		for (i = 0; i < gesture_coords_reported; i++) {
			gesture_coordinates_x[i] =
				(((u16)val[i * 2 + 1]) & 0x0F) << 8 |
				(((u16)val[i * 2]) & 0xFF);
			gesture_coordinates_y[i] =
				(((u16)val[gesture_coords_reported *
					2 + i * 2 + 1]) & 0x0F) << 8 |
				(((u16)val[gesture_coords_reported *
					2 + i * 2]) & 0xFF);
		}


		pr_info("%s: Reading Gesture Coordinates DONE!\n", __func__);
		return OK;
	} else {
		pr_err("%s: The event passsed as argument is invalid! ERROR %08X\n",
			__func__, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
  * Return the coordinates of the points stored during the last detected gesture
  * @param x output parameter which will store the address of the array
  * containing the x coordinates
  * @param y output parameter which will store the address of the array
  * containing the y coordinates
  * @return the number of points (x,y) stored and therefore the size of the x
  * and y array returned.
  */
int getGestureCoords(u16 **x, u16 **y)
{
	*x = gesture_coordinates_x;
	*y = gesture_coordinates_y;
	pr_info("%s: Number of gesture coordinates pairs returned = %d\n",
		__func__, gesture_coords_reported);
	return gesture_coords_reported;
}
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics");
