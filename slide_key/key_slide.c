#define TOUCH_NUM           3

#define SLIDE_TICK_MAX                        0xffff
#define SLIDE_TICK_ADD(tick_a, tick_b)        ((uint16_t)(((tick_b) + (tick_a)) % SLIDE_TICK_MAX))      
#define SLIDE_TICK_SUB(tick_a, tick_b)        ((uint16_t)(((tick_a) + SLIDE_TICK_MAX - (tick_b)) % SLIDE_TICK_MAX))  

enum slide_key_step {
    SLIDE_KEY_IDLE,
    SLIDE_KEY_PUSH_1,
    SLIDE_KEY_PUSH_2,
    SLIDE_KEY_FINISH,
};

typedef struct
{
    u8 debounce_cnt[TOUCH_NUM];
    u8 up_cnt[TOUCH_NUM];
    u16 push_tick[TOUCH_NUM];   
    u16 release_tick[TOUCH_NUM];
    
    u8 step;
    u8 last_push_idx;
    u8 first_push_idx;
} slide_key_t;

typedef struct
{
    u8 time_diff_max[TOUCH_NUM];                //每个touch点push和release的差值上下限
    u8 time_diff_min[TOUCH_NUM];
    u8 velo_max[TOUCH_NUM-1];                   //每个touch点之间滑动的速度上下限
    u8 velo_min[TOUCH_NUM-1];
    u8 distance[TOUCH_NUM-1];
    u8 release_delay;
#if TOUCH_NUM > 2
    u8 velo_diff_max[TOUCH_NUM-2];              //每个touch点之间滑动的速度差值上限，例如ab点之间速度v1,bc点之间速度v2，v1-v2的值不应该大于此值
    u16 velo_diff_total_max;
#else
    u8 velo_diff_max[TOUCH_NUM-1];
    u16 velo_diff_total_max;
#endif
} slide_key_api_t;

AT(.data.bsp.key)
static u16 key_val_table[TOUCH_NUM] = {KEY_NULL};

slide_key_t slide_key;
slide_key_api_t slide_key_api;
gpio_t tch_gpio[TOUCH_NUM];

void key_multi_reset(void);

AT(.com_text.bsp.key.str)
char slide_debug_str[] = "slide val:%x step:%x\n";

AT(.com_text.bsp.key.str)
char slide_debug_str2[] = "slide judge:%x val:%x\n";

AT(.com_text.bsp.key.str)
char slide_debug_str3[] = "slide judge:%x %x\n";

T(.com_text.bsp.key)
u16 bsp_key_slide_judge(void)
{
    u16 time_diff[TOUCH_NUM];
    u16 velocity[TOUCH_NUM-1];
    u16 velocity_last = 0;
    u32 velocity_total_diff = 0;
    u8 i = slide_key.first_push_idx ? TOUCH_NUM-1 : 0;
    while(i < TOUCH_NUM) {
        time_diff[i] = SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i], slide_key.push_tick[i]);
        printf(slide_debug_str2, __LINE__, time_diff[i]);
        if((time_diff[i] < slide_key_api.time_diff_min[i]) || (time_diff[i] > slide_key_api.time_diff_max[i])) {
            printf(slide_debug_str3, slide_key.release_tick[i], slide_key.push_tick[i]);
            return NO_KEY;
        }
        if(i != 0) {
            if(!slide_key.first_push_idx) {
                if(SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i], slide_key.release_tick[i-1])) {
                    velocity[i-1] = ((slide_key_api.distance[i-1]))/SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i], slide_key.release_tick[i-1]);
                    printf(slide_debug_str2, __LINE__, velocity[i-1]);
                    printf(slide_debug_str3, slide_key_api.distance[i-1], SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i], slide_key.release_tick[i-1]));
                }
                printf(slide_debug_str2, __LINE__, velocity[i-1]);
                if((velocity[i-1] < slide_key_api.velo_min[i-1]) || (velocity[i-1] > slide_key_api.velo_max[i-1])) {
                    return NO_KEY;
                }
                if(velocity_last) {
                    u16 vole_diff = abs_s(velocity[i-1]-velocity_last);
                    printf(slide_debug_str2, __LINE__, vole_diff);
                    if(vole_diff > slide_key_api.velo_diff_max[i-2]) {
                        return NO_KEY;
                    }
                    velocity_total_diff += vole_diff;
                }
                velocity_last = velocity[i-1];
            } else {
                if(SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i-1], slide_key.release_tick[i])) {
                    velocity[i-1] = ((slide_key_api.distance[i-1]))/SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i-1], slide_key.release_tick[i]);
                    printf(slide_debug_str2, __LINE__, velocity[i-1]);
                    printf(slide_debug_str3, slide_key_api.distance[i-1], SLIDE_TICK_DIFF_ABS(slide_key.release_tick[i-1], slide_key.release_tick[i]));
                }
                printf(slide_debug_str2, __LINE__, velocity[i-1]);
                if((velocity[i-1] < slide_key_api.velo_min[i-1]) || (velocity[i-1] > slide_key_api.velo_max[i-1])) {
                    return NO_KEY;
                }
                if(velocity_last) {
                    u16 vole_diff = abs_s(velocity[i-1]-velocity_last);
                    printf(slide_debug_str2, __LINE__, vole_diff);
                    if(vole_diff > slide_key_api.velo_diff_max[i-1]) {
                        return NO_KEY;
                    }
                    velocity_total_diff += vole_diff;
                }
                velocity_last = velocity[i-1];
            }
        }
        i = slide_key.first_push_idx ? i-1 : i+1;
    }
    printf(slide_debug_str2, __LINE__, velocity_total_diff);
    if(slide_key_api.velo_diff_total_max < velocity_total_diff) {
        return NO_KEY;
    }
    if(slide_key.first_push_idx == 0) {
        return KEY_SLIDE_1;
    } else {
        return KEY_SLIDE_2;
    }
}

AT(.com_text.bsp.key)
u16 bsp_key_slide_scan(void)
{
    static u16 slide_tick = 0;
    slide_tick++;
    u8 push_tch_num = 0;
    u8 tch_idx = 0;
    u8 push_tch_idx = 0;
    u16 key_val = KEY_NULL;
    
    while(tch_idx < TOUCH_NUM) {
        if(!(tch_gpio[tch_idx].sfr[GPIOx] & BIT(tch_gpio[tch_idx].num))) {
            push_tch_num++;
            push_tch_idx = tch_idx;
            if(slide_key.debounce_cnt[tch_idx] < key_shake_table.scan_cnt) {
                slide_key.debounce_cnt[tch_idx]++;
                if(slide_key.debounce_cnt[tch_idx] == key_shake_table.scan_cnt) {
                    slide_key.push_tick[tch_idx] = slide_tick;
                    slide_key.up_cnt[tch_idx] = 0;
                }
            } else {
                slide_key.up_cnt[tch_idx] = 0;
            }
        } else {
            if(slide_key.up_cnt[tch_idx] < key_shake_table.up_cnt) {
                slide_key.up_cnt[tch_idx]++;
                if(slide_key.up_cnt[tch_idx] == key_shake_table.up_cnt) {
                    slide_key.release_tick[tch_idx] = slide_tick;
                }
            } else {
                slide_key.debounce_cnt[tch_idx] = 0;
            }
        }
        tch_idx++;
    }
    if((push_tch_num <= 1) && slide_key.step <= SLIDE_KEY_PUSH_1) {
        if(push_tch_num == 0) {
            key_val = key_process(KEY_NULL);
        } else {
            key_val = key_process(key_val_table[push_tch_idx]);
        }
    } else {
        key_process(KEY_NULL);
        key_val = NO_KEY;
        key_multi_reset();
    }
    if(slide_key.step == SLIDE_KEY_IDLE) {
        //或许可以考虑扩充为起始点并不需要在最开头，而是一个设定的最晚起始点之前
        if((push_tch_num == 1) && ((push_tch_idx == 0) || (push_tch_idx == (TOUCH_NUM-1)))) {
            if(slide_key.debounce_cnt[push_tch_idx] == key_shake_table.scan_cnt) {
                slide_key.step = SLIDE_KEY_PUSH_1;
                slide_key.first_push_idx = slide_key.last_push_idx = push_tch_idx;
                printf(slide_debug_str, key_val, slide_key.step);
            }
        }
    } else if(slide_key.step == SLIDE_KEY_PUSH_1) {
        if((push_tch_num == 1) && (push_tch_idx != slide_key.last_push_idx)) {
            if(slide_key.debounce_cnt[push_tch_idx] == key_shake_table.scan_cnt) {
                slide_key.step = SLIDE_KEY_PUSH_2;
                slide_key.last_push_idx = push_tch_idx;
                printf(slide_debug_str, key_val, slide_key.step);
            }
        }
    } else if(slide_key.step == SLIDE_KEY_PUSH_2) {
        if(push_tch_num == 1) {
            if(slide_key.debounce_cnt[push_tch_idx] == key_shake_table.scan_cnt) {
                slide_key.last_push_idx = push_tch_idx;
                if(((push_tch_idx == 0) || (push_tch_idx == (TOUCH_NUM-1))) && (push_tch_idx != slide_key.first_push_idx)) {
                    slide_key.step = SLIDE_KEY_FINISH;
                    printf(slide_debug_str, key_val, slide_key.step);
                }
            }
        }
    }
    if(slide_key.step == SLIDE_KEY_FINISH) {
        if(slide_key.up_cnt[slide_key.last_push_idx] == key_shake_table.up_cnt) {
            key_val = bsp_key_slide_judge();
            slide_key.step = SLIDE_KEY_IDLE;
            printf(slide_debug_str, key_val, slide_key.step);
        }
    }
    if(slide_key.step != SLIDE_KEY_IDLE) {
        if(slide_key.up_cnt[slide_key.last_push_idx] == key_shake_table.up_cnt) {
            if(SLIDE_TICK_SUB(slide_tick,slide_key.release_tick[slide_key.last_push_idx]) > slide_key_api.release_delay) {
                slide_key.step = SLIDE_KEY_IDLE;
            }
        }
    }
    if(key_val)
    printf(slide_debug_str, key_val, slide_key.step);
    return key_val;
}

void bsp_key_slide_init(void)
{
    gpio_t *tch_gpio_p = tch_gpio;
    bsp_gpio_cfg_init(tch_gpio_p, IO_PB5);
    tch_gpio_p->sfr[GPIOxDE] |= BIT(tch_gpio_p->num);
    tch_gpio_p->sfr[GPIOxPU] |= BIT(tch_gpio_p->num);     //上拉
    tch_gpio_p->sfr[GPIOxDIR] |= BIT(tch_gpio_p->num);    //输入
    key_val_table[0] = KEY_MULTI(KEY_1);
    tch_gpio_p++;
    bsp_gpio_cfg_init(tch_gpio_p, IO_PB0);
    tch_gpio_p->sfr[GPIOxDE] |= BIT(tch_gpio_p->num);
    tch_gpio_p->sfr[GPIOxPU] |= BIT(tch_gpio_p->num);     //上拉
    tch_gpio_p->sfr[GPIOxDIR] |= BIT(tch_gpio_p->num);    //输入
    tch_gpio_p++;
    bsp_gpio_cfg_init(tch_gpio_p, IO_PB1);
    tch_gpio_p->sfr[GPIOxDE] |= BIT(tch_gpio_p->num);
    tch_gpio_p->sfr[GPIOxPU] |= BIT(tch_gpio_p->num);     //上拉
    tch_gpio_p->sfr[GPIOxDIR] |= BIT(tch_gpio_p->num);    //输入
    slide_key_api.release_delay = get_double_key_time();
    slide_key_api.distance[0] = slide_key_api.distance[1] = 100;
    slide_key_api.time_diff_max[0] = slide_key_api.time_diff_max[1] = slide_key_api.time_diff_max[2] = 0xff;
    slide_key_api.velo_max[0] = slide_key_api.velo_max[1] = 0xff;
    slide_key_api.velo_diff_max[0] = slide_key_api.velo_diff_total_max = 0xff;
}