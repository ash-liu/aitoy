/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-10-17     Meco Man      First version
 */
#include <rtthread.h>
#include <lvgl.h>
#include <lv_port_indev.h>
#define DBG_TAG    "LVGL.demo"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#ifndef LV_THREAD_STACK_SIZE
    #define LV_THREAD_STACK_SIZE 4096
#endif

#ifndef LV_THREAD_PRIO
    #define LV_THREAD_PRIO (RT_THREAD_PRIORITY_MAX * 2 / 3)
#endif

static struct rt_thread lvgl_thread;
static rt_uint8_t lvgl_thread_stack[LV_THREAD_STACK_SIZE];

// for message queue
#define MSG_QUEUE_BUFFER_SIZE   1024*4
#define MSG_QUEUE_ELEMENT_SIZE  1024*2
struct rt_messagequeue lvgl_msg_mq;           // 消息队列控制块
lv_obj_t * cz_label;

static void init_demo_ui()
{
    cz_label = lv_label_create(lv_scr_act());
    // lv_label_set_text(cz_label, message);
    lv_obj_set_style_text_font(cz_label, &lv_font_myfont, 0);
    lv_obj_set_width(cz_label, 410);
    lv_obj_align(cz_label, LV_ALIGN_TOP_LEFT, 5, 5);
}

static void show_message(const char *message)
{
    lv_obj_set_style_text_font(cz_label, &lv_font_myfont, 0);
    lv_obj_set_width(cz_label, 410);
    lv_obj_align(cz_label, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_label_set_text(cz_label, message);
}


static void lvgl_entry(void *parameter)
{
    rt_err_t result;
    char *buf;
    rt_uint8_t *msg_pool; // 消息队列中用到的放置消息的内存池
    
    msg_pool = rt_malloc(MSG_QUEUE_BUFFER_SIZE);
    if (msg_pool == RT_NULL) {
        rt_kprintf("no memory for message pool.\n");
        return;
    }

    /* 初始化消息队列 */
    result = rt_mq_init(&lvgl_msg_mq,
                        "lvgl_msg",
                        &msg_pool[0],               /* 内存池指向msg_pool */
                        MSG_QUEUE_ELEMENT_SIZE,     /* 每个消息的大小 字节 */
                        MSG_QUEUE_BUFFER_SIZE,      /* 内存池的大小是msg_pool的大小 */
                        RT_IPC_FLAG_FIFO);          /* 如果有多个线程等待，按照先来先得到的方法分配消息 */
    if (result != RT_EOK) {
        rt_kprintf("init message queue failed.\n");
        return;
    }

    // extern void lv_demo_music(void);
    // lv_demo_music();
    init_demo_ui();
    show_message("hello.\n");

    buf = rt_malloc(1024*2);
    if (buf == RT_NULL) {
        rt_kprintf("no memory for message.\n");
        return;
    }

    while(1) {
        /* 从消息队列中接收消息 */
        if (rt_mq_recv(&lvgl_msg_mq, buf, MSG_QUEUE_ELEMENT_SIZE, RT_WAITING_NO) == RT_EOK) {
            for (int i = 0; i < rt_strlen(buf); i++) {
                rt_kprintf("%c", buf[i]);
            }
            show_message(buf);
        }

        lv_task_handler();
        rt_thread_mdelay(LV_DISP_DEF_REFR_PERIOD);
    }
}

static int lvgl_demo_init(void)
{
    rt_thread_init(&lvgl_thread,
                   "LVGL",
                   lvgl_entry,
                   RT_NULL,
                   &lvgl_thread_stack[0],
                   sizeof(lvgl_thread_stack),
                   LV_THREAD_PRIO,
                   10);
    rt_thread_startup(&lvgl_thread);

    return 0;
}
INIT_ENV_EXPORT(lvgl_demo_init);
