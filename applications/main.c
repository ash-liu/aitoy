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

void key_isr(void *args)
{
    if (state == STATE_IDLE) {
        state = STATE_START;
    } 
    
    if (state == STATE_RECOARDING) {
        state = STATE_STOP;
    }
}


static void rt_wlan_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_uint8_t buf[100];
    rt_kprintf("event %d\n", event);

    if (event == RT_WLAN_EVT_READY) {
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

    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(KEY_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(KEY_PIN, PIN_IRQ_MODE_FALLING, key_isr, RT_NULL);
    rt_pin_irq_enable(KEY_PIN, PIN_IRQ_ENABLE);

    /* init Wi-Fi auto connect feature */
    wlan_autoconnect_init();
    /* enable auto reconnect on WLAN device */
    rt_wlan_config_autoreconnect(RT_TRUE);
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, rt_wlan_handler, RT_NULL);


    while(1) {
        switch (state) {
            case STATE_IDLE:
                break;
            
            case STATE_START:
                state = STATE_RECOARDING;
                rt_pin_write(LED_PIN, PIN_LOW);
                rt_kprintf("start recording\n");
                // wav_argc = 3;
                // wav_recorder(wav_argc, const_argv_start);
                wavrecorder_start(&info);
                break;
            
            case STATE_RECOARDING:
                break;
            
            case STATE_STOP:
                state = STATE_IDLE;
                rt_pin_write(LED_PIN, PIN_HIGH);
                rt_kprintf("stop recording\n");
                // wav_argc = 2;
                // wav_recorder(wav_argc, const_argv_stop);
                wavrecorder_stop();
                retry = 0;
                do {
                    ret = webclient_post_test(1, "");
                }
                while (ret != 0 && retry++ < 4);
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


