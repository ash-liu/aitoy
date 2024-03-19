/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-09-02     RT-Thread    first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <wavrecorder.h>
#include "drv_common.h"
// #include <arpa/inet.h>
#include <netdev_ipaddr.h>
#include <netdev.h>
#include <lvgl.h>

enum {
    STATE_IDLE,
    STATE_START,
    STATE_RECOARDING,
    STATE_STOP
};

#define LED_PIN GET_PIN(I, 8)
#define KEY_PIN GET_PIN(H, 4)

extern void wlan_autoconnect_init(void);
static volatile rt_uint8_t state = STATE_IDLE;

extern int wav_recorder(int argc, char *argv[]);
extern struct rt_messagequeue lvgl_msg_mq;
extern struct netdev *netdev_default;
extern lv_obj_t * state_label;
extern do_airkiss_configwifi();
extern airkiss_demo_start();
extern append_flag;

rt_thread_t main_tid;

rt_sem_t ready_sem = RT_NULL;


void signal_handler(int sig)
{
    if (state == STATE_IDLE) {
        state = STATE_START;
        // rt_pin_irq_enable(KEY_PIN, PIN_IRQ_DISABLE);
    } 
    
    if (state == STATE_RECOARDING) {
        state = STATE_STOP;
        // rt_pin_irq_enable(KEY_PIN, PIN_IRQ_DISABLE);
    }
}


#if 0
void key_isr(void *args)
{
    rt_thread_kill(main_tid, SIGUSR1);
}
#endif


static void rt_wlan_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_uint8_t buf[100];
    rt_kprintf("event %d\n", event);

    if (event == RT_WLAN_EVT_READY) {
        rt_sem_release(ready_sem);
        // 获取ip地址
        rt_sprintf(buf, "Got IP address : %s\n", inet_ntoa(netdev_default->ip_addr));
        rt_mq_send(&lvgl_msg_mq, buf, rt_strlen(buf) + 1);
    }
}

int main(void)
{
    int wav_argc;
    char *const_argv_start[] = {"wavrecord", "-s", "/sdcard/test.wav"};
    // char *const_argv_stop[] = {"wavrecord", "-t"};
    struct wavrecord_info info = {
        .uri = "/sdcard/test.wav",
        .samplerate = 8000,
        .channels = 2,
        .samplebits = 16,
    };

    rt_int32_t ret = 0;
    rt_uint32_t retry = 0;
    rt_err_t  wait_ret;

    // get self tid
    main_tid = rt_thread_self();

    // enable SIG1 
    rt_signal_install(SIGUSR1, signal_handler);
    rt_signal_unmask(SIGUSR1);

    // build the ready sem
    ready_sem = rt_sem_create("ready_sem", 0, RT_IPC_FLAG_FIFO);

    // set the key
    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(KEY_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_write(LED_PIN, PIN_HIGH);

    /* init Wi-Fi auto connect feature */
    wlan_autoconnect_init();
    /* enable auto reconnect on WLAN device */
    rt_wlan_config_autoreconnect(RT_TRUE);
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, rt_wlan_handler, RT_NULL);

    // force to airkiss
    if (rt_pin_read(KEY_PIN) == PIN_LOW) {
        rt_kprintf("Force to do airkiss\n");
        rt_thread_mdelay(6000);
        rt_mq_send(&lvgl_msg_mq, 
                    "Force to do airkiss\n", 
                    sizeof("Force to do airkiss\n"));
        airkiss_demo_start();
    }
    // wifi timeout 15s, then do airkiss
    else if(rt_sem_take(ready_sem, 1000 * 15) != RT_EOK) {
        rt_kprintf("wifi not ready, do airkiss\n");
        rt_mq_send(&lvgl_msg_mq, 
                    "wifi not ready, do airkiss\n", 
                    sizeof("wifi not ready, do airkiss\n"));
        airkiss_demo_start();
    }
    else {
        //  just ok.
    }

    while(1) {
        switch (state) {
            case STATE_IDLE:
                break;
            
            case STATE_START:
                append_flag = 0;                // 开始识别后，不在追加
                state = STATE_RECOARDING;
                rt_pin_write(LED_PIN, PIN_LOW);
                lv_label_set_text(state_label, "Listening.");
                rt_kprintf("start recording\n");
                // wav_argc = 3;
                // wav_recorder(wav_argc, const_argv_start);
                wavrecorder_start(&info);
                rt_thread_mdelay(200);          // 用来防抖
                break;
            
            case STATE_RECOARDING:
                break;
            
            case STATE_STOP:
                state = STATE_IDLE;
                rt_kprintf("stop recording\n");
                rt_pin_write(LED_PIN, PIN_HIGH);
                lv_label_set_text(state_label, "Thinking.");
                // wav_argc = 2;
                // wav_recorder(wav_argc, const_argv_stop);
                wavrecorder_stop();
                retry = 0;
                do {
                    ret = webclient_post_test(1, "");
                }
                while (ret != 0 && retry++ < 4);

                lv_label_set_text(state_label, "");
                rt_thread_mdelay(200);          // 用来防抖
                break;
            
            default:
                state = STATE_IDLE;
                break;
        }
        // rt_thread_mdelay(500);
        // rt_pin_write(LED_PIN, PIN_HIGH);
        // rt_thread_mdelay(500);
        // rt_pin_write(LED_PIN, PIN_LOW);

        rt_thread_mdelay(100);
    }
    return RT_EOK;
}

#include "stm32h7xx.h"
static int vtor_config(void)
{
    /* Vector Table Relocation in Internal QSPI_FLASH */
    SCB->VTOR = QSPI_BASE;
    return 0;
}
INIT_BOARD_EXPORT(vtor_config);


