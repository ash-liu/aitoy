#include <rtthread.h>
#include <rtdevice.h>
#include <time.h>
static int rtc_sample(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    time_t now;

    // ./rtc_sample 2024 2 26 18 14 30
    if (argc != 7) {
        rt_kprintf("Usage: rtc_sample year month day hour minute second\n");
        return -RT_ERROR;
    }

    ret = set_date(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC date failed\n");
        return ret;
    }

    ret = set_time(atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC time failed\n");
        return ret;
    }
    
    ret = set_date(2024, 2, 26);
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC date failed\n");
        return ret;
    }
    
    ret = set_time(18, 14, 30);
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC time failed\n");
        return ret;
    }
    
    rt_thread_mdelay(3000);
    
    now = time(RT_NULL);
    rt_kprintf("%s\n", ctime(&now));
    return ret;
}

MSH_CMD_EXPORT(rtc_sample, rtc sample);
