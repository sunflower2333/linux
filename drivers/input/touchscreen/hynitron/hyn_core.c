#include "hyn_core.h"
#define HYN_DRIVER_NAME  "hyn_ts"

static struct hyn_ts_data *hyn_data = NULL;
static const struct hyn_ts_fuc* hyn_fun = NULL;
static const struct of_device_id hyn_of_match_table[] = {
    {.compatible = "hyn,66xx", .data = &cst66xx_fuc,},   /*suport 36xx 35xx 66xx 68xx 148E*/
    {.compatible = "hyn,3240", .data = &cst3240_fuc,},   /*suport 3240 */
    {.compatible = "hyn,92xx", .data = &cst92xx_fuc,},   /*suport 9217、9220 */
    {.compatible = "hyn,3xx",  .data = &cst3xx_fuc,},    /*suport 340 348*/
    {.compatible = "hyn,1xx",  .data = &cst1xx_fuc,},    /*suport 328 128 140 148*/
    {.compatible = "hyn,7xx",  .data = &cst7xx_fuc,},    /*suport 726 826 836u*/
    {.compatible = "hyn,8xxt", .data = &cst8xxT_fuc,},   /*suport 816t 816d 820 08C*/
    {},
};
MODULE_DEVICE_TABLE(of, hyn_of_match_table);

static int hyn_check_ic(struct hyn_ts_data *ts_data)
{
    const struct of_device_id *of_dev;
    of_dev = of_match_device(hyn_of_match_table,ts_data->dev);
	if (!of_dev)
		return -EINVAL;
    hyn_fun = of_dev->data;
    if(IS_ERR_OR_NULL(hyn_fun) || IS_ERR_OR_NULL(hyn_fun->tp_chip_init) 
        || hyn_fun->tp_chip_init(ts_data)){
        return -ENODEV;
    }
    ts_data->hyn_fuc_used = hyn_fun;
    return 0;
}

static int hyn_parse_dt(struct hyn_ts_data *ts_data)
{
    int ret = 0;
    struct device *dev = ts_data->dev;
    struct hyn_plat_data* dt = &ts_data->plat_data;
    HYN_ENTER();
    if(dev->of_node){
        u32 buf[8];
        struct device_node *np = dev->of_node;
        dt->vdd_i2c = NULL;
        dt->vdd_ana = NULL;
        if(of_property_read_bool(np,"vdd_ana")){
           dt->vdd_ana = regulator_get(dev, "vdd_ana");
            if(IS_ERR_OR_NULL(dt->vdd_ana)){
                dt->vdd_ana = NULL;
                HYN_ERROR("regulator_get vdd_ana failed");
            }
        }
        else{
            HYN_INFO("vdd_ana not config");
        }

        if(of_property_read_bool(np,"vcc_i2c")){
            dt->vdd_i2c = regulator_get(dev, "vcc_i2c");
            if(IS_ERR_OR_NULL(dt->vdd_i2c)){
                dt->vdd_i2c = NULL;
                HYN_ERROR("regulator_get vdd_i2c failed");
            }
        }
        else{
            HYN_INFO("vdd_i2c not config");
        }

        dt->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
        dt->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
        if(dt->reset_gpio < 0 || dt->irq_gpio < 0){
            HYN_ERROR("dts get gpio failed");
            return -ENODEV;
        }
        else{
            HYN_INFO("reset_gpio:%d irq_gpio:%d",dt->reset_gpio,dt->irq_gpio);
        }

        dt->pinctl = devm_pinctrl_get(dev);
        if (!IS_ERR_OR_NULL(dt->pinctl)){
            dt->pin_active = pinctrl_lookup_state(dt->pinctl, "ts_active");
            dt->pin_suspend= pinctrl_lookup_state(dt->pinctl, "ts_suspend");
            if(IS_ERR_OR_NULL(dt->pin_active) || IS_ERR_OR_NULL(dt->pin_suspend)){
                HYN_ERROR("dts get \"ts-active\" \"ts_suspend\" failed");
                return -EINVAL;
            }
        }
        else{
            HYN_INFO("pinctrl not config");
            dt->pinctl = NULL;
            dt->pin_active = NULL;
            dt->pin_suspend= NULL;
        }

        ret = of_property_read_u32(np, "max-touch-number", &dt->max_touch_num);
        ret |= of_property_read_u32(np, "pos-swap", &dt->swap_xy);
        ret |= of_property_read_u32(np, "posx-reverse", &dt->reverse_x);
        ret |= of_property_read_u32(np, "posy-reverse", &dt->reverse_y);
        ret |= of_property_read_u32_array(np, "display-coords", buf, 4);
        dt->x_resolution = buf[2];
        dt->y_resolution = buf[3];
        HYN_INFO("dts x_res = %d,y_res = %d,touch-number = %d swap_xy = %d reversex = %d reversey = %d",
                    dt->x_resolution,dt->y_resolution,dt->max_touch_num,dt->swap_xy,dt->reverse_x,dt->reverse_y);
        if(ret < 0){
            HYN_ERROR("dts get screen failed");
            return -EINVAL;
        }

        ret = of_property_read_u32(np, "key-number", &dt->key_num);
        if(ret>=0 && dt->key_num && dt->key_num<=8){
            ret |= of_property_read_u32(np, "key-y-coord", &dt->key_y_coords);
            ret |= of_property_read_u32_array(np, "key-x-coords", dt->key_x_coords, dt->key_num);
            ret |= of_property_read_u32_array(np, "keys", dt->key_code, dt->key_num);
            if(ret < 0){
                HYN_ERROR("dts get screen failed");
                return -EINVAL;
            }
        }
        else{
            HYN_INFO("key not config");
            dt->key_num = 0;
        }
        return 0;
    }
    else{
        HYN_ERROR("dts match failed");
        return -ENODEV;
    }
}

static int hyn_power_source_ctrl(int enable)
{
    int ret = 0;
    HYN_ENTER();
    if(IS_ERR_OR_NULL(hyn_data->plat_data.vdd_ana)){
        return ret;
    }
    if(hyn_data->power_is_on != enable){
        if(enable){
            if(!IS_ERR_OR_NULL(hyn_data->plat_data.vdd_ana)){
                ret |= regulator_enable(hyn_data->plat_data.vdd_ana);
            }
            if(!IS_ERR_OR_NULL(hyn_data->plat_data.vdd_i2c)){
                ret |= regulator_enable(hyn_data->plat_data.vdd_i2c);
            }
        }
        else{
            if(!IS_ERR_OR_NULL(hyn_data->plat_data.vdd_ana)){
                ret |= regulator_disable(hyn_data->plat_data.vdd_ana);
            }
            if(!IS_ERR_OR_NULL(hyn_data->plat_data.vdd_i2c)){
                ret |= regulator_disable(hyn_data->plat_data.vdd_i2c);
            }
        }
        hyn_data->power_is_on = enable;
    }
    if(ret)
        HYN_ERROR("set vdd %s regulator failed,ret=%d",enable ? "off":"on",ret);
    return ret;
}

static int hyn_poweron(struct hyn_ts_data *ts_data)
{
    int ret = 0;
    struct hyn_plat_data* dt = &ts_data->plat_data;
    if(!IS_ERR_OR_NULL(hyn_data->plat_data.pinctl)){
        if(pinctrl_select_state(dt->pinctl, dt->pin_active)){
            HYN_ERROR("pin active set failed");
        }
    }

    ret = gpio_request(dt->irq_gpio, "hyn_irq_gpio");
    ret |= gpio_request(dt->reset_gpio, "hyn_reset_gpio");
    if(ret < 0){
        HYN_ERROR("gpio_request failed");
        goto GPIO_SET_FAILE;
    }
    
    ret = gpio_direction_input(dt->irq_gpio);
    ret |= gpio_direction_output(dt->reset_gpio, 0);
    if(ret < 0){
        HYN_ERROR("set gpio_direction failed");
        goto GPIO_SET_FAILE;
    }

    if(!IS_ERR_OR_NULL(dt->vdd_ana)){
        if(regulator_count_voltages(dt->vdd_ana) > 0){
            if(regulator_set_voltage(dt->vdd_ana, 2800000, 3300000)){
                HYN_ERROR("set voltage vdd_ana failed");
            }
        }
    }

    if(!IS_ERR_OR_NULL(dt->vdd_i2c)){
        if(regulator_count_voltages(dt->vdd_i2c) > 0){
            if(regulator_set_voltage(dt->vdd_i2c, 1800000, 1800000)){
                HYN_ERROR("set voltage vdd_i2c failed");
            }
        }
    }
    if(hyn_power_source_ctrl(1)){
        HYN_ERROR("enable power failed");
    }
    mdelay(5);
    gpio_set_value(dt->reset_gpio, 1);
    return 0;
GPIO_SET_FAILE:
    return ret;
}

static int hyn_input_dev_init(struct hyn_ts_data *ts_data)
{
    int key_num = 0;//,ret =0;
    struct hyn_plat_data *dt = &ts_data->plat_data;
    struct input_dev *input_dev;

    HYN_ENTER(); 
    input_dev = input_allocate_device();
    if (!input_dev) {
        HYN_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }
    input_dev->name = HYN_DRIVER_NAME;
    input_dev->id.bustype = ts_data->bus_type;
    input_dev->dev.parent = ts_data->dev;
    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    for (key_num = 0; key_num < dt->key_num; key_num++)
			input_set_capability(input_dev, EV_KEY, dt->key_code[key_num]);
#if HYN_MT_PROTOCOL_B_EN
    set_bit(BTN_TOOL_FINGER,input_dev->keybit);
    //input_mt_init_slots(input_dev, dt->max_touch_num);
    input_mt_init_slots(input_dev, dt->max_touch_num, INPUT_MT_DIRECT);
#else
    input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,  dt->max_touch_num, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, dt->x_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, dt->y_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);

    ts_data->input_dev = input_dev;

    return 0;
}

static void release_all_finger(struct hyn_ts_data *ts_data)
{
    HYN_ENTER();
#if HYN_MT_PROTOCOL_B_EN
    {
    u8 i;
	for(i=0; i< ts_data->plat_data.max_touch_num; i++) {	
		input_mt_slot(ts_data->input_dev, i);
		input_report_abs(ts_data->input_dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);
	}
	input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
    }
#else
    input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
	input_mt_sync(ts_data->input_dev);
#endif
    ts_data->rp_buf.report_need = REPORT_NONE;
}

static void touch_updata(u8 idx,u8 event)
{
    struct ts_frame *rep_frame = &hyn_data->rp_buf;
    struct input_dev *dev = hyn_data->input_dev;
    u16 zpress = rep_frame->pos_info[idx].pres_z;
    if(zpress < 10){
        zpress = zpress+(rep_frame->pos_info[idx].pos_x&0x03) + (rep_frame->pos_info[idx].pos_y&0x03);
    }
#if HYN_MT_PROTOCOL_B_EN
    if(event){
        input_report_key(dev, BTN_TOUCH, 1);
        input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
        input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
        input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
        input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
        input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_PRESSURE, zpress);
    }
    else{
        input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
        input_mt_report_slot_state(dev, MT_TOOL_FINGER, 0);
    }
#else
    if(event){
        input_report_key(dev, BTN_TOUCH, 1);
        input_report_abs(dev, ABS_MT_PRESSURE, zpress);
        input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
        input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
        // input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
        input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
        input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
        input_mt_sync(dev);
    }
#endif
}

static void hyn_irq_report_work(struct work_struct *work)
{
    struct hyn_ts_data *ts_data = hyn_data;
    struct ts_frame *rep_frame = &hyn_data->rp_buf;
    struct input_dev *dev = hyn_data->input_dev;
    struct hyn_plat_data *dt = &hyn_data->plat_data;
    u16 xpos,ypos;
    hyn_fun->tp_report();
    mutex_lock(&ts_data->mutex_report);
    if(rep_frame->report_need & REPORT_KEY){ //key
#if KEY_USED_POS_REPORT
        rep_frame->pos_info[0].pos_id = 0;
        rep_frame->pos_info[0].pos_x = dt->key_x_coords[rep_frame->key_id];
        rep_frame->pos_info[0].pos_y = dt->key_y_coords;
        rep_frame->pos_info[0].pres_z = 100;
        touch_updata(0,rep_frame->key_state ? 1:0);
#else
        input_report_key(dev,dt->key_code[rep_frame->key_id],rep_frame->key_state ? 1:0); 
#endif
        if(rep_frame->key_state==0){
            rep_frame->report_need = REPORT_NONE;
        } 
        input_sync(dev);
        HYN_INFO2("report keyid:%d keycode:%d",rep_frame->key_id,dt->key_code[rep_frame->key_id]);
    }

    if(rep_frame->report_need & REPORT_POS){ //pos
        u8 i;
        if(rep_frame->rep_num == 0){
            release_all_finger(ts_data);
        }
        else{
            u8 touch_down = 0;
            for(i = 0; i < rep_frame->rep_num; i++){
                HYN_INFO2("id,%d,xy,%d,%d",rep_frame->pos_info[i].pos_id,rep_frame->pos_info[i].pos_x,rep_frame->pos_info[i].pos_y);
                if(dt->swap_xy){
                    xpos = rep_frame->pos_info[i].pos_y;
                    ypos = rep_frame->pos_info[i].pos_x;
                }
                else{
                    xpos = rep_frame->pos_info[i].pos_x;
                    ypos = rep_frame->pos_info[i].pos_y;
                }
                if(ypos > dt->y_resolution || xpos > dt->x_resolution || rep_frame->pos_info[i].pos_id >= ts_data->plat_data.max_touch_num){
                    HYN_ERROR("Please check dts or FW config !!!");
                    continue;
                }
                if(dt->reverse_x){
                    xpos = dt->x_resolution-xpos;
                }
                if(dt->reverse_y){
                    ypos = dt->y_resolution-ypos;
                }
                rep_frame->pos_info[i].pos_x = xpos;
                rep_frame->pos_info[i].pos_y = ypos;
                touch_updata(i,rep_frame->pos_info[i].event? 1:0);
                if(rep_frame->pos_info[i].event) touch_down++;
            }
            if(touch_down==0){
                rep_frame->report_need = REPORT_NONE;
                input_report_key(dev, BTN_TOUCH, 0);
#if HYN_MT_PROTOCOL_B_EN==0
                input_mt_sync(dev);
#endif
            }
        }
        input_sync(dev);
    }
    else if(rep_frame->report_need & REPORT_PROX){
        hyn_proximity_report(ts_data->prox_state);
        rep_frame->report_need = REPORT_NONE;
    }
#if (HYN_GESTURE_EN)
    else if(rep_frame->report_need & REPORT_GES){
        hyn_gesture_report(ts_data);
        rep_frame->report_need = REPORT_NONE;
    }
#endif 
    mutex_unlock(&ts_data->mutex_report);
}

static void hyn_esdcheck_work(struct work_struct *work)
{
#if ESD_CHECK_EN
    int ret;
    HYN_ENTER(); 
    ret = hyn_fun->tp_check_esd();
    // HYN_INFO("esd:%04x",ret);
    if(hyn_data->esd_last_value != ret){
        hyn_data->esd_fail_cnt = 0;
        hyn_data->esd_last_value = ret;
    }
    else{
        hyn_data->esd_fail_cnt++;
        if(hyn_data->esd_fail_cnt > 2){
            hyn_data->esd_fail_cnt = 0;
            hyn_power_source_ctrl(0);
            mdelay(1);
            hyn_power_source_ctrl(1);
            hyn_fun->tp_rest();
        }
    }
    queue_delayed_work(hyn_data->hyn_workqueue, &hyn_data->esdcheck_work,
                           msecs_to_jiffies(1000));
#endif
}


static void hyn_resum(struct device *dev)
{
    int ret = 0;
    HYN_ENTER();
#if (HYN_WAKE_LOCK_EN==1)
    wake_unlock(&hyn_data->tp_wakelock);
#endif
    if(!IS_ERR_OR_NULL(hyn_data->plat_data.pinctl)){
      pinctrl_select_state(hyn_data->plat_data.pinctl, hyn_data->plat_data.pin_active);  
    }
    hyn_power_source_ctrl(1);
    hyn_fun->tp_resum();
    if(hyn_data->prox_is_enable){
        hyn_fun->tp_prox_handle(1);
    }
    else if(hyn_data->gesture_is_enable){
        hyn_irq_set(hyn_data,DISABLE);
        ret = disable_irq_wake(hyn_data->client->irq);
        ret |= irq_set_irq_type(hyn_data->client->irq,hyn_data->plat_data.irq_gpio_flags); 
        if(ret < 0){
            HYN_ERROR("gesture irq_set_irq failed");
        }  
        hyn_irq_set(hyn_data,ENABLE);
    }
    release_all_finger(hyn_data);
    input_sync(hyn_data->input_dev);
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void hyn_suspend(struct device *dev)
{
    int ret = 0;
    HYN_ENTER();
#if (HYN_WAKE_LOCK_EN==1)
    wake_lock(&hyn_data->tp_wakelock);
#endif
    if(hyn_data->prox_is_enable ==1){
    }
    else if(hyn_data->gesture_is_enable){
        hyn_irq_set(hyn_data,DISABLE);
        ret = enable_irq_wake(hyn_data->client->irq);
        ret |= irq_set_irq_type(hyn_data->client->irq,IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND|IRQF_ONESHOT); 
        if(ret < 0){
            HYN_ERROR("gesture irq_set_irq failed");
        }  
        hyn_fun->tp_set_workmode(GESTURE_MODE,0);
        hyn_irq_set(hyn_data,ENABLE);
        hyn_power_source_ctrl(1);
    }
    else{
        hyn_fun->tp_supend();
        if(!IS_ERR_OR_NULL(hyn_data->plat_data.pinctl)){
            pinctrl_select_state(hyn_data->plat_data.pinctl, hyn_data->plat_data.pin_suspend);
        }
        hyn_power_source_ctrl(0);
    }
}
#endif

static void hyn_updata_fw_work(struct work_struct *work)
{
    int ret = 0;
    if(!IS_ERR_OR_NULL(hyn_data->fw_updata_addr)){
        ret = hyn_fun->tp_updata_fw(hyn_data->fw_updata_addr,hyn_data->fw_updata_len);
    }
    else{
        HYN_ERROR("fw_updata_addr is erro");
    }
}

static void hyn_resum_work(struct work_struct *work)
{
    hyn_resum(hyn_data->dev);
}

static irqreturn_t hyn_irq_handler(int irq, void *data)
{
	atomic_set(&hyn_data->hyn_irq_flg,1);
    if(hyn_data->work_mode < DIFF_MODE){
        // queue_work(hyn_data->hyn_workqueue,&hyn_data->work_report);
        flush_workqueue(hyn_data->hyn_workqueue);
        hyn_irq_report_work(NULL);
    }
    else{
        wake_up(&hyn_data->wait_irq);
    }
    return IRQ_HANDLED;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void hyn_ts_early_suspend(struct early_suspend *handler)
{
    hyn_suspend(hyn_data->dev);
}
static void hyn_ts_late_resume(struct early_suspend *handler)
{
    hyn_resum(hyn_data->dev);
}
#endif

#ifdef I2C_PORT
static void hyn_ts_remove(struct i2c_client *client);
static int hyn_ts_probe(struct i2c_client *client)
#else
static int hyn_ts_remove(struct spi_device *client);
static int hyn_ts_probe(struct spi_device *client)
#endif
{
    int ret = 0;
    u16 bus_type;
    struct hyn_ts_data *ts_data = 0;

    HYN_ENTER();
    HYN_INFO(HYN_DRIVER_VERSION);
#ifdef I2C_PORT
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        HYN_ERROR("I2C not supported");
        return -ENODEV;
    }
    bus_type = BUS_I2C;
#else
    client->mode = SPI_MODE;
	client->max_speed_hz = SPI_CLOCK_FREQ;
	client->bits_per_word = 8;
    if (spi_setup(client) < 0){
        HYN_ERROR("SPI not supported");
        return -ENODEV;
    }
    bus_type = BUS_SPI;
#endif
    if(!IS_ERR_OR_NULL(hyn_data)){
        HYN_ERROR("other dev is insmode");
        return -ENOMEM;
    }
    ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        HYN_ERROR("alloc ts data failed");
        return -ENOMEM;
    }
    ts_data->bus_type = bus_type;
    ts_data->rp_buf.key_id = 0xFF;
    ts_data->work_mode = NOMAL_MODE;
    ts_data->log_level = 0;
    
    hyn_data = ts_data;
    ts_data->client = client;
    ts_data->dev = &client->dev;
    dev_set_drvdata(ts_data->dev, ts_data);

    #if (I2C_USE_DMA==2)
        ts_data->dma_buff_va = (u8 *)dma_alloc_coherent(NULL,
                        2048, &ts_data->dma_buff_pa, GFP_KERNEL);
        if (!ts_data->dma_buff_va) {
            HYN_ERROR("Allocate I2C DMA Buffer fail");
        }
    #endif
    ret = hyn_parse_dt(ts_data);
    if(ret){
        HYN_ERROR("hyn_parse_dt failed");
        goto FREE_RESOURCE;
    }

    ret = hyn_poweron(ts_data);
    if(ret){
        HYN_ERROR("hyn_poweron failed");
        goto FREE_RESOURCE;
    }
    // spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->mutex_report);
    mutex_init(&ts_data->mutex_bus);
    mutex_init(&ts_data->mutex_fs);
    init_waitqueue_head(&ts_data->wait_irq);

    ret = hyn_check_ic(ts_data);
    if(ret){
        HYN_ERROR("hyn_check_ic failed");
        goto FREE_RESOURCE;
    }

    INIT_WORK(&ts_data->work_report,hyn_irq_report_work);
    INIT_WORK(&ts_data->work_updata_fw,hyn_updata_fw_work);
    INIT_WORK(&ts_data->work_resume,hyn_resum_work);
    INIT_DELAYED_WORK(&ts_data->esdcheck_work,hyn_esdcheck_work);

    ts_data->hyn_workqueue = create_singlethread_workqueue("hyn_wq");
    if (IS_ERR_OR_NULL(ts_data->hyn_workqueue)){
        HYN_ERROR("create work queue failed");
        goto FREE_RESOURCE;
    }
    
    ret = hyn_input_dev_init(ts_data);
    if(ret){
        if(!IS_ERR_OR_NULL(ts_data->input_dev)){
            input_set_drvdata(ts_data->input_dev, NULL);
            input_free_device(ts_data->input_dev);
            ts_data->input_dev = NULL;
        }
        HYN_ERROR("hyn_input_dev_init failed");
        goto FREE_RESOURCE;
    }
    ret = input_register_device(ts_data->input_dev);
    if(ret){
        HYN_ERROR("input_register_device failed");
        goto FREE_RESOURCE;
    }

#if (HYN_WAKE_LOCK_EN==1)
    wake_lock_init(&ts_data->tp_wakelock, WAKE_LOCK_SUSPEND, "suspend_tp_lock");
#endif

#if (HYN_GESTURE_EN)
    ret = hyn_gesture_init(ts_data);
    if(ret){
        HYN_ERROR("gesture_init failed");
        goto FREE_RESOURCE;
    }
#endif

    ts_data->gpio_irq =  gpio_to_irq(ts_data->plat_data.irq_gpio);
    ret = request_threaded_irq(ts_data->gpio_irq, NULL, hyn_irq_handler,
                                (IRQF_TRIGGER_FALLING | IRQF_ONESHOT), HYN_DRIVER_NAME, ts_data);
    if(ret){
        HYN_ERROR("request_threaded_irq failed");
        goto FREE_RESOURCE;
    }
    atomic_set(&ts_data->irq_is_disable,ENABLE);
    hyn_irq_set(ts_data , DISABLE);

#if defined(CONFIG_HAS_EARLYSUSPEND)
    ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts_data->early_suspend.suspend = hyn_ts_early_suspend;
    ts_data->early_suspend.resume = hyn_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif
    hyn_create_sysfs(ts_data);
#if (HYN_APK_DEBUG_EN)
    hyn_tool_fs_int(ts_data);
#endif
    hyn_proximity_int(ts_data);
    hyn_irq_set(ts_data,ENABLE);
    hyn_esdcheck_switch(ts_data,ENABLE);

#if HYN_POWER_ON_UPDATA
    if(ts_data->need_updata_fw){
        queue_work(ts_data->hyn_workqueue,&ts_data->work_updata_fw);
    }
#endif
    return 0;
FREE_RESOURCE:
    hyn_ts_remove(client);
    return -1;
}


#ifdef I2C_PORT
static void hyn_ts_remove(struct i2c_client *client)
#else
static int hyn_ts_remove(struct spi_device *client)
#endif
{
    struct hyn_ts_data *ts_data = hyn_data;
    HYN_ENTER();    
    if(!IS_ERR_OR_NULL(ts_data)){
        if (ts_data->hyn_workqueue){
            flush_workqueue(ts_data->hyn_workqueue);
            hyn_esdcheck_switch(ts_data,DISABLE);
            destroy_workqueue(ts_data->hyn_workqueue);
        } 
        HYN_INFO("ts_remove1");
#if (HYN_APK_DEBUG_EN)
        hyn_tool_fs_exit();
#endif
        hyn_proximity_exit();
#if (HYN_GESTURE_EN)
        hyn_gesture_exit(ts_data);
#endif
        hyn_release_sysfs(ts_data);
        HYN_INFO("ts_remove2");
        if(ts_data->gpio_irq != 0)
            free_irq(ts_data->gpio_irq, ts_data);
        HYN_INFO("ts_remove3");
        if(!IS_ERR_OR_NULL(ts_data->input_dev)){
            input_unregister_device(ts_data->input_dev);
        }
        HYN_INFO("ts_remove4");
#if defined(CONFIG_HAS_EARLYSUSPEND)
        unregister_early_suspend(&ts_data->early_suspend);
#endif
        if(gpio_is_valid(ts_data->plat_data.irq_gpio))
            gpio_free(ts_data->plat_data.irq_gpio);
        if(gpio_is_valid(ts_data->plat_data.reset_gpio))
            gpio_free(ts_data->plat_data.reset_gpio);
        HYN_INFO("ts_remove5"); 
        hyn_power_source_ctrl(0);
        if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
            regulator_put(ts_data->plat_data.vdd_ana);
        }
        if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)){
            regulator_put(ts_data->plat_data.vdd_i2c);
        }
        HYN_INFO("ts_remove6");
#if (I2C_USE_DMA==2)
        if(!IS_ERR_OR_NULL(ts_data->dma_buff_va)){
            dma_free_coherent(NULL, 2048, ts_data->dma_buff_va, ts_data->dma_buff_pa);
        }
#endif
        kfree(ts_data);
        hyn_data = NULL;
        HYN_INFO("ts_remove7");
    }
}

#ifdef I2C_PORT
static const struct i2c_device_id hyn_id_table[] = {
    {.name = HYN_DRIVER_NAME, .driver_data = 0,},
    {},
};

static struct i2c_driver hyn_ts_driver = {
    .probe = hyn_ts_probe,
    .remove = hyn_ts_remove,
    .driver = {
        .name = HYN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = hyn_of_match_table,
    },
    .id_table = hyn_id_table,
};
#else
static struct spi_driver hyn_ts_driver = {
	.driver = {
		   .name = HYN_DRIVER_NAME,
		   .of_match_table = hyn_of_match_table,
		   .owner = THIS_MODULE,
		   },
	.probe = hyn_ts_probe,
	.remove = hyn_ts_remove,
};
#endif

static int __init hyn_ts_init(void)
{
    int ret = 0;
    HYN_ENTER();
#ifdef I2C_PORT  
    ret = i2c_add_driver(&hyn_ts_driver);
#else
    ret = spi_register_driver(&hyn_ts_driver);
#endif
    if (ret) {
        HYN_ERROR("add i2c driver failed");
        return -ENODEV;
    }
    return 0;
}

static void __exit hyn_ts_exit(void)
{
    HYN_ENTER();
#ifdef I2C_PORT  
    i2c_del_driver(&hyn_ts_driver);
#else
    spi_unregister_driver(&hyn_ts_driver);
#endif
}

late_initcall(hyn_ts_init);
// module_init(hyn_ts_init);
module_exit(hyn_ts_exit);

MODULE_AUTHOR("Hynitron Driver Team");
MODULE_DESCRIPTION("Hynitron Touchscreen Driver");
MODULE_LICENSE("GPL v2");

