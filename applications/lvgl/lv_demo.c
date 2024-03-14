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

rt_uint8_t gpt4_used = 0;


/* 事件回调函数 */
static void screen_click_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t point;
    lv_indev_get_point(indev, &point); // 获取点击的坐标

    /* 获取屏幕或容器的高度 */
    lv_coord_t height = lv_obj_get_height(obj);
    lv_coord_t width = lv_obj_get_width(obj);

    /* 判断点击是在上半部分还是下半部分，并相应地滚动内容 */
    if (point.x < width / 3) {
        if (gpt4_used == 0) {
            gpt4_used = 1;
            rt_mq_send(&lvgl_msg_mq, "gpt4 used on.\n", sizeof("gpt4 used on.\n"));
            rt_kprintf("gpt4 used on.\n");
        }
        else {
            gpt4_used = 0;
            rt_mq_send(&lvgl_msg_mq, "gpt4 used off.\n", sizeof("gpt4 used off.\n"));
            rt_kprintf("gpt4 used off.\n");
        }
    }
    else {
        if (point.y > height / 2) {
            lv_obj_scroll_by(obj, 0, -height / 2, LV_ANIM_OFF); // 向上滚动
        } 
        else {
            lv_obj_scroll_by(obj, 0, height / 2, LV_ANIM_OFF);  // 向下滚动
        }
    }
}


static void init_demo_ui()
{
    // 创建label
    cz_label = lv_label_create(lv_scr_act());
    
    // 设置样式文本字体
    lv_obj_set_style_text_font(cz_label, &lv_font_myfont, 0);
    lv_label_set_long_mode(cz_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cz_label, 470);
    lv_obj_align(cz_label, LV_ALIGN_BOTTOM_LEFT, 5, 5);

    //设置样式文本行间距
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_line_space(&style, 7);
    lv_obj_add_style(cz_label, &style, 0);

    // 设置label的回调函数
    lv_obj_add_event_cb(lv_scr_act(), screen_click_event_cb, LV_EVENT_CLICKED, NULL);
}

static void show_message(const char *message)
{
    // lv_obj_set_style_text_font(cz_label, &lv_font_myfont, 0);
    // lv_obj_set_width(cz_label, 410);
    // lv_obj_align(cz_label, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_label_set_text(cz_label, message);

    // 计算需要滚动的距离，以便最新的内容能够显示出来
    // 这里我们直接滚动到文本区域的底部
    lv_obj_scroll_to_y(cz_label, 10000, LV_ANIM_ON);
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
    show_message("Hello, Wifi is connecting.\r");

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
            const char *current_text = lv_label_get_text(cz_label);
            size_t new_size = strlen(current_text) + strlen(buf) + 1;
            char *full_text = rt_malloc(new_size);

            if (full_text != RT_NULL) {
                // 将当前文本和新文本合并到新分配的内存中
                strcpy(full_text, current_text);
                strcat(full_text, buf);

                // 更新标签的文本
                show_message(full_text);
                // lv_label_set_text(label, full_text);

                // 释放临时分配的内存
                rt_free(full_text);
            }
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
