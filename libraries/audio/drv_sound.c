/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-15     liuduanfei     first version
 */

#include "board.h"
#include "wm8988.h"
#include "drv_sound.h"
#include <rtthread.h>

//#define DRV_DEBUG

#define DBG_TAG              "drv.sound"
#ifdef  DRV_DEBUG
#define DBG_LVL              DBG_LOG
#else
#define DBG_LVL              DBG_INFO
#endif /* DRV_DEBUG */

#include <rtdbg.h>

#define CODEC_I2C_NAME  ("i2c3")

#define TX_DMA_FIFO_SIZE (2048)
#define RX_DMA_FIFO_SIZE (2048)

static struct sai_reg * sai1 = (struct sai_reg *)SAI2BASE;

struct stm32_audio
{
    struct rt_i2c_bus_device *i2c_bus;
    struct rt_audio_device audio;
    struct rt_audio_configure replay_config;
    rt_int32_t replay_volume;
    rt_uint8_t *tx_fifo;
    rt_bool_t startup;
};
struct stm32_audio _stm32_audio_play = {0};

struct stm32_record
{
    struct rt_i2c_bus_device *i2c_bus;
    struct rt_audio_device audio;
    struct rt_audio_configure record_config;
    rt_uint8_t *rx_fifo;
    rt_int32_t record_volume;
    rt_bool_t startup;
};
static struct stm32_record _stm32_audio_record = {0};



/* sample_rate, PLLI2SN(50.7), PLLI2SQ, PLLI2SDivQ, MCKDIV */
const rt_uint32_t SAI_PSC_TBL[][4] =
{
    {AUDIO_FREQUENCY_008K,256,5,25},
    {AUDIO_FREQUENCY_008K,302,107,0},
    {AUDIO_FREQUENCY_011K,426,2,52},
    {AUDIO_FREQUENCY_016K,429,38,2},
    {AUDIO_FREQUENCY_032K,426,1,52},
    {AUDIO_FREQUENCY_044K,429,1,38},
    {AUDIO_FREQUENCY_048K,467,1,38},
    {AUDIO_FREQUENCY_088K,429,1,19},
    {AUDIO_FREQUENCY_096K,467,1,19},
    {AUDIO_FREQUENCY_174K,271,1,6},
    {AUDIO_FREQUENCY_192K,295,6,0},
};

void SAIA_samplerate_set(rt_uint32_t freq)
{
    rt_uint16_t retry = 0;
    rt_uint8_t i = 0;
    rt_uint32_t temp = 0;
    for(i=0; i<(sizeof(SAI_PSC_TBL) / sizeof(SAI_PSC_TBL[0])); i++)
    {
        if(freq == SAI_PSC_TBL[i][0])break;
    }
    if(i == (sizeof(SAI_PSC_TBL) / sizeof(SAI_PSC_TBL[0])))
        return ;

    RCC->CR&=~(1<<26);
    while(((RCC->CR&(1<<27)))&&(retry<0X1FFF))retry++;
    RCC->PLLCKSELR &=~ (0X3F<<12);
    RCC->PLLCKSELR |= 25<<12;
    temp = RCC->PLL2DIVR;
    temp &=~ (0xFFFF);
    temp |= (SAI_PSC_TBL[i][1]-1)<<0;
    temp |= (SAI_PSC_TBL[i][2]-1)<<9;
    RCC->PLL2DIVR = temp;

    RCC->PLLCFGR |= 1<<19;
    RCC->CR |= 1<<26;
    while((RCC->CR&1<<27)==0);

    temp = sai1->acr1;
    temp &=~ (0X3F<<20);
    temp |= (rt_uint32_t)SAI_PSC_TBL[i][3]<<20;
    temp |= 1<<16;
    temp |= 1<<17;
    sai1->acr1 = temp;
}

// 配置SAIB的采样频率，通过查表法快速获得寄存器配置参数
void SAIB_samplerate_set(rt_uint32_t freq)
{
    // SAI2B是从模块，因此这里要配置的是主模块A
    SAIA_samplerate_set(freq);
    // 使能SAI2A的数据流发送
    DMA2_Stream3->CR |= 1<<0;
    // 使能SAI2B
    sai1->bcr1 |= 1<<17;// 使能DMA传送
    sai1->bcr1 |= 1<<16;// 使能音频模块
    // 调试需要，后期可以注释掉
    //rt_kprintf("samplerate_set_sai_bcr1:0x%08x \n",sai1->bcr1);
    //rt_kprintf("sai_bsr:0x%08x \n",(rt_uint32_t)sai1->bsr);
}


void SAIA_channels_set(rt_uint16_t channels)
{
    if (channels == 2)
    {
        sai1->acr1 &= ~(1 << 12);
        wm8988_set_in_channel(_stm32_audio_play.i2c_bus, 2);
    }
    else
    {
        sai1->acr1 |= (1 << 12);
        wm8988_set_in_channel(_stm32_audio_play.i2c_bus, 1);
    }
}

// 配置SAIB的频道
void SAIB_channels_set(rt_uint16_t channels)
{
    // SAI2B是从模块，因此这里要配置的是主模块A
    SAIA_channels_set(channels);
    // 调试需要，后期可以注释掉
    //rt_kprintf("channels_set_sai_bcr1:0x%08x \n",sai1->bcr1);
    //rt_kprintf("sai_bsr:0x%08x \n",(rt_uint32_t)sai1->bsr);
}

void SAIA_samplebits_set(rt_uint16_t samplebits)
{
    LOG_D("%s",__func__);
    rt_uint32_t temp;
    switch (samplebits)
    {
    case 8:
        temp = 2;
        break;
    case 10:
        temp = 3;
        break;
    case 16:
        temp = 4;
        DMA2_Stream3->CR |= (1<<11);
        DMA2_Stream3->CR |= (1<<13);
        DMA2_Stream3->NDTR = TX_DMA_FIFO_SIZE / 4;
        break;
    case 20:
        temp = 5;
        break;
    case 24:
        temp = 6;
        DMA2_Stream3->CR &= ~(0xF<<11);
        DMA2_Stream3->CR |= (2<<11);
        DMA2_Stream3->CR |= (2<<13);
        DMA2_Stream3->NDTR = TX_DMA_FIFO_SIZE / 8;
        break;
    case 32:
        temp = 7;
        break;
    default:
        temp = 4;
        break;
    }
    sai1->acr1 &= ~(7<<5);
    sai1->acr1 |= (temp<<5);
}

// SAIA采样点设置
void SAIB_samplebits_set(rt_uint16_t samplebits)
{
    // SAI2B是从模块，因此这里要配置的是主模块A
    SAIA_samplebits_set(16);
    // 调试需要，后期可以注释掉
    //rt_kprintf("samplebits_set_sai_bcr1:0x%08x \n",sai1->bcr1);
    //rt_kprintf("sai_bsr:0x%08x \n",(rt_uint32_t)sai1->bsr);
}

void SAIA_config_set(struct rt_audio_configure config)
{
    SAIA_channels_set(config.channels);
    SAIA_samplebits_set(config.samplebits);
    SAIA_samplerate_set(config.samplerate);
}

// SAIB配置
void SAIB_config_set(struct rt_audio_configure config)
{
    SAIB_channels_set(config.channels);
    SAIB_samplebits_set(config.samplebits);
    SAIB_samplerate_set(config.samplerate);
}

/* initial sai A */
rt_err_t SAIA_config_init()
{
    rt_uint32_t temp;
    /*
    Configuration register 1
      Master transmitter
      Free protocol
      Signals generated by the SAI change on SCK rising edge
      audio sub-block in asynchronous mode.
      Stereo mode
      Audio block output driven immediately after the setting of this bit.
      Master clock generator is enabled
    */
    temp = sai1->acr1;
    temp |= 1<<9;
    temp |= 1<<13;
    sai1->acr1 = temp;

    /*
    Frame configuration register
      Frame length 64
      Frame synchronization active level length 32
      FS signal is a start of frame signal + channel side identification
      FS is asserted one bit before the first bit of the slot 0
    */
    temp = (64-1)<<0;
    temp |= (32-1)<<8;
    temp |= 1<<16;
    temp |= 1<<18;
    sai1->afrcr = temp;

    /*
      Slot register
      Slot size 16
      Number of slots in an audio frame 2
      Slot0 Slot1 enable
    */
    temp = 1<<6;
    temp |= (2-1)<<8;
    temp |= (1<<17)|(1<<16);
    temp |= (1<<16);
    sai1->aslotr = temp;

    sai1->acr2 = 1<<0;
    sai1->acr2 |= 1<<3;

    return RT_EOK;
}


/* initial sai B */
rt_err_t SAIB_config_init(void)
{
    rt_uint32_t tempreg;
    tempreg = 0;
    tempreg|=3<<0;              //设置SAI1工作模式，00,主发送器;01,主接收器;10,从发送器;11,从接收器
    tempreg|=0<<2;              //设置SAI1协议为:自由协议(支持I2S/LSB/MSB/TDM/PCM/DSP等协议)
    tempreg|=4<<5;              //设置数据大小，0/1,未用到,2,8位;3,10位;4,16位;5,20位;6,24位;7,32位
    tempreg|=0<<8;              //数据MSB位优先
    tempreg|=1<<9;              //数据在时钟的上升/下降沿选通，0,时钟下降沿选通;1,时钟上升沿选通
    tempreg|=1<<10;             //使能同步模式
    tempreg|=0<<12;             //立体声模式
    tempreg|=1<<13;             //立即驱动音频模块输出
    //tempreg |= 1<<17;           // 使能DMA传送
    //tempreg |= 1<<16;           // 使能音频模块
    sai1->bcr1 = tempreg;
    tempreg = 0;
    tempreg=(64-1)<<0;          //设置帧长度为64,左通道32个SCK,右通道32个SCK.
    tempreg|=(32-1)<<8;         //设置帧同步有效电平长度,在I2S模式下=1/2帧长.
    tempreg|=1<<16;             //FS信号为SOF信号+通道识别信号
    tempreg|=0<<17;             //FS低电平有效(下降沿)
    tempreg|=1<<18;             //在slot0的第一位的前一位使能FS,以匹配飞利浦标准
    sai1->bfrcr = tempreg;
    tempreg = 0;
    tempreg=0<<0;               //slot偏移(FBOFF)为0
    tempreg|=2<<6;              //slot大小为32位
    tempreg|=(2-1)<<8;          //slot数为2个
    tempreg|=(1<<17)|(1<<16);   //使能slot0和slot1
    sai1->bslotr = tempreg;
    sai1->bcr2 = 1<<0;          // FIFI阈值，001，1/4FIFO // 010，1/2FIFO
    sai1->bcr2 |= 1<<3;
    // 调试需要，后期可以注释掉
//    rt_kprintf("init_sai_bcr1:0x%08x \n",sai1->bcr1);
//    rt_kprintf("init_sai_bfrcr:0x%08x \n",sai1->bfrcr);
//    rt_kprintf("init_sai_bslotr:0x%08x \n",sai1->bslotr);
//    rt_kprintf("init_sai_bcr2:0x%08x \n",sai1->bcr2);
//    rt_kprintf("init_sai_bsr:0x%08x \n",(rt_uint32_t)sai1->bsr);
    return RT_EOK;
}


rt_err_t SAIA_tx_dma(void)
{
    RCC->AHB1ENR |= 1<<1;
    RCC->D3AMR |=  1<<0;
    while(DMA2_Stream3->CR & 0X01);
    DMAMUX1_Channel11->CCR = 89;

    DMA2->LIFCR|=0X3D<<22;
    DMA2_Stream3->FCR=0X0000021;

    DMA2_Stream3->CR = 0;
    DMA2_Stream3->CR |= 1<<6;
    DMA2_Stream3->CR |= 1<<8;
    DMA2_Stream3->CR &= ~(1<<9);
    DMA2_Stream3->CR |= 1<<10;

    DMA2_Stream3->PAR  = (rt_uint32_t)&sai1->adr;
    DMA2_Stream3->M0AR = (rt_uint32_t)_stm32_audio_play.tx_fifo;
    DMA2_Stream3->M1AR = (rt_uint32_t)_stm32_audio_play.tx_fifo + (TX_DMA_FIFO_SIZE / 2);

    DMA2_Stream3->CR |= 2<<16;
    DMA2_Stream3->CR |= 1<<18;
    DMA2_Stream3->CR &= ~(1<<21);
    DMA2_Stream3->CR &= ~(1<<23);

    DMA2_Stream3->FCR &= ~(1<<2);
    DMA2_Stream3->FCR &= ~(3<<0);

    DMA2_Stream3->CR |= 1<<4;

    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    return RT_EOK;
}

// 使用DMA2_STREAM2作为SAI发送传输的配置
rt_err_t SAIB_rx_dma(void)
{
    RCC->AHB1ENR|=1<<1;             // 使能DMA2时钟
    RCC->D3AMR|=1<<0;               // DMAMUX 时钟使能
    while(DMA2_Stream2->CR&0X01);   // 等待 DMA2_Stream2 可配置
    DMAMUX1_Channel10->CCR=90;      // 请求线复用器通道x配置寄存器，指向通道SAI2A（90是SAI2B）
    DMA2->LIFCR|=0X3D<<16;          // 低中断标志清零寄存器，对应位写1，可以清零中断标志位
    DMA2_Stream2->FCR=0X0000021;    // FIFO控制寄存器，FIFO容量1/2，FIFO为空
    // DMA数据流配置寄存器
    DMA2_Stream2->PAR  = (rt_uint32_t)&sai1->bdr;                                           // 外设地址寄存器
    DMA2_Stream2->M0AR = (rt_uint32_t)_stm32_audio_record.rx_fifo;                          // 存储器0地址寄存器
    DMA2_Stream2->M1AR = (rt_uint32_t)_stm32_audio_record.rx_fifo + (RX_DMA_FIFO_SIZE/2);   // 存储器地址1寄存器
    DMA2_Stream2->NDTR = RX_DMA_FIFO_SIZE/4;
    // DMA数据流配置寄存器
    DMA2_Stream2->CR=0;             // 寄存器清零，复位后默认值就是0
    DMA2_Stream2->CR|=0<<6;         // 数据传输方向，01存储器到外设（00外设到存储器）
    DMA2_Stream2->CR|=1<<8;         // 循环模式，1使能循环模式
    DMA2_Stream2->CR|=0<<9;         // 外设递增模式，0外设地址指针固定b
    DMA2_Stream2->CR|=1<<10;        // 存储器递增模式，1每次数据传输后，存储器地址指针递增
    DMA2_Stream2->CR|=1<<11;        // 外设数据长度:16 位/32 位
    DMA2_Stream2->CR|=1<<13;        // 存储器数据长度:16 位/32 位
    DMA2_Stream2->CR|=2<<16;        // 优先级，10，高
    DMA2_Stream2->CR|=1<<18;        // 双缓冲区模式，1，DMA传输结束时切换目标存储区
    DMA2_Stream2->CR|=0<<21;        // 外设突发传输配置，00，单次传输
    DMA2_Stream2->CR|=0<<23;        // 存储器突发传输配置，00，单次传输
    DMA2_Stream2->FCR&=~(1<<2);     // 不使用FIFO，FIFO控制寄存器，使能直接模式
    DMA2_Stream2->FCR&=~(3<<0);     // 无 FIFO 设置，FIFO阈值选择，FIFO容量的1/4
    DMA2_Stream2->CR|=1<<4;         // 传输完成中断使能
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    rt_kprintf("SAIB_RX_DMA!\n");
    return RT_EOK;
}

void DMA2_Stream3_IRQHandler(void)
{
    rt_interrupt_enter();
    if(DMA2->LISR&(1<<27))
    {
        DMA2->LIFCR|=1<<27;
        rt_audio_tx_complete(&_stm32_audio_play.audio);
    }
    rt_interrupt_leave();
}

void DMA2_Stream2_IRQHandler(void)
{
    rt_interrupt_enter();
    // 打印输出结果是0x04680000，解析如下
    //rt_kprintf("-0x%08x",DMA2->LISR);
    if(DMA2->LISR&(1<<21))  // 是否为传输完成中断标志־
    {
        //rt_kprintf("-");
        DMA2->LIFCR|=1<<21; // 写1清空传输完成中断标志־
        // 采用双缓冲DMA
        if (DMA2_Stream2->CR&(1<<19)){
            rt_audio_rx_done(&_stm32_audio_record.audio, &_stm32_audio_record.rx_fifo[0], RX_DMA_FIFO_SIZE/2);
        }
        else{
            rt_audio_rx_done(&_stm32_audio_record.audio, &_stm32_audio_record.rx_fifo[RX_DMA_FIFO_SIZE/2], RX_DMA_FIFO_SIZE/2);
        }
        //清除无效的D-Cache
        SCB_CleanInvalidateDCache();
    }
    rt_interrupt_leave();
}

rt_err_t sai_a_init()
{
    /* set sai_a DMA */
    SAIA_tx_dma();
    SAIA_config_init();
    return RT_EOK;
}

// SAI_B初始化
rt_err_t sai_b_init(void)
{
    /* set sai_a DMA */
    SAIB_rx_dma();      // 初始化RX_DMA配置
    SAIB_config_init(); // 初始化SAI配置
    return RT_EOK;
}

static rt_err_t stm32_player_getcaps(struct rt_audio_device *audio, struct rt_audio_caps *caps)
{
    rt_err_t result = RT_EOK;
    struct stm32_audio *st_audio = (struct stm32_audio *)audio->parent.user_data;

    LOG_D("%s:main_type: %d, sub_type: %d", __FUNCTION__, caps->main_type, caps->sub_type);

    switch (caps->main_type)
    {
    case AUDIO_TYPE_QUERY: /* query the types of hw_codec device */
    {
        switch (caps->sub_type)
        {
        case AUDIO_TYPE_QUERY:
            caps->udata.mask = AUDIO_TYPE_OUTPUT | AUDIO_TYPE_MIXER;
            break;

        default:
            result = -RT_ERROR;
            break;
        }

        break;
    }

    case AUDIO_TYPE_OUTPUT: /* Provide capabilities of OUTPUT unit */
    {
        switch (caps->sub_type)
        {
        case AUDIO_DSP_PARAM:
            caps->udata.config.channels     = st_audio->replay_config.channels;
            caps->udata.config.samplebits   = st_audio->replay_config.samplebits;
            caps->udata.config.samplerate   = st_audio->replay_config.samplerate;
            break;

        case AUDIO_DSP_SAMPLERATE:
            caps->udata.config.samplerate   = st_audio->replay_config.samplerate;
            break;

        case AUDIO_DSP_CHANNELS:
            caps->udata.config.channels     = st_audio->replay_config.channels;
            break;

        case AUDIO_DSP_SAMPLEBITS:
            caps->udata.config.samplebits     = st_audio->replay_config.samplebits;
            break;

        default:
            result = -RT_ERROR;
            break;
        }

        break;
    }

    case AUDIO_TYPE_MIXER: /* report the Mixer Units */
    {
        switch (caps->sub_type)
        {
        case AUDIO_MIXER_QUERY:
            caps->udata.mask = AUDIO_MIXER_VOLUME | AUDIO_MIXER_LINE;
            break;

        case AUDIO_MIXER_VOLUME:
            caps->udata.value = st_audio->replay_volume;
            break;

        case AUDIO_MIXER_LINE:
            break;

        default:
            result = -RT_ERROR;
            break;
        }

        break;
    }

    default:
        result = -RT_ERROR;
        break;
    }

    return result;
}

static rt_err_t stm32_record_getcaps(struct rt_audio_device *audio, struct rt_audio_caps *caps)
{
    rt_err_t result = RT_EOK;
    struct stm32_record *st_record = (struct stm32_record *)audio->parent.user_data;
    LOG_D("%s:main_type: %d, sub_type: %d", __FUNCTION__, caps->main_type, caps->sub_type);
    switch (caps->main_type)
    {
    case AUDIO_TYPE_QUERY: /* query the types of hw_codec device */
    {
        switch (caps->sub_type)
        {
        case AUDIO_TYPE_QUERY:
            caps->udata.mask = AUDIO_TYPE_INPUT | AUDIO_TYPE_MIXER;
            break;
        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }
    case AUDIO_TYPE_INPUT: /* Provide capabilities of INPUT unit */
    {
        switch (caps->sub_type)
        {
        case AUDIO_DSP_PARAM:
            caps->udata.config.channels     = st_record->record_config.channels;
            caps->udata.config.samplebits   = st_record->record_config.samplebits;
            caps->udata.config.samplerate   = st_record->record_config.samplerate;
            break;
        case AUDIO_DSP_SAMPLERATE:
            caps->udata.config.samplerate   = st_record->record_config.samplerate;
            break;
        case AUDIO_DSP_CHANNELS:
            caps->udata.config.channels     = st_record->record_config.channels;
            break;
        case AUDIO_DSP_SAMPLEBITS:
            caps->udata.config.samplebits   = st_record->record_config.samplebits;
            break;
        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }
    case AUDIO_TYPE_MIXER: /* report the Mixer Units */
    {
        switch (caps->sub_type)
        {
        case AUDIO_MIXER_QUERY:
            caps->udata.mask = AUDIO_MIXER_VOLUME | AUDIO_MIXER_LINE;
            break;
        case AUDIO_MIXER_VOLUME:
            caps->udata.value = st_record->record_volume;
            break;
        case AUDIO_MIXER_LINE:
            break;
        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }
    default:
        result = -RT_ERROR;
        break;
    }
    return result;
}

static rt_err_t stm32_player_configure(struct rt_audio_device *audio, struct rt_audio_caps *caps)
{
    rt_err_t result = RT_EOK;
    struct stm32_audio *st_audio = (struct stm32_audio *)audio->parent.user_data;

    LOG_D("%s:main_type: %d, sub_type: %d", __FUNCTION__, caps->main_type, caps->sub_type);

    switch (caps->main_type)
    {
    case AUDIO_TYPE_MIXER:
    {
        switch (caps->sub_type)
        {
        case AUDIO_MIXER_MUTE:
        {
            break;
        }

        case AUDIO_MIXER_VOLUME:
        {
            int volume = caps->udata.value;

            st_audio->replay_volume = volume;
            /* set mixer volume */
            wm8988_set_out_valume(_stm32_audio_play.i2c_bus, volume);
            break;
        }

        default:
            result = -RT_ERROR;
            break;
        }

        break;
    }

    case AUDIO_TYPE_OUTPUT:
    {
        switch (caps->sub_type)
        {
        case AUDIO_DSP_PARAM:
        {
            struct rt_audio_configure config = caps->udata.config;

            st_audio->replay_config.samplerate = config.samplerate;
            st_audio->replay_config.samplebits = config.samplebits;
            st_audio->replay_config.channels = config.channels;

            SAIA_config_set(config);
            break;
        }

        case AUDIO_DSP_SAMPLERATE:
        {
            st_audio->replay_config.samplerate = caps->udata.config.samplerate;
            SAIA_samplerate_set(caps->udata.config.samplerate);
            break;
        }

        case AUDIO_DSP_CHANNELS:
        {
            st_audio->replay_config.channels = caps->udata.config.channels;
            SAIA_channels_set(caps->udata.config.channels);
            break;
        }

        case AUDIO_DSP_SAMPLEBITS:
        {
            st_audio->replay_config.samplebits = caps->udata.config.samplebits;
            SAIA_samplebits_set(caps->udata.config.samplebits);
            break;
        }

        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }

    default:
        break;
    }

    return result;
}


static rt_err_t stm32_record_configure(struct rt_audio_device *audio, struct rt_audio_caps *caps)
{
    rt_err_t result = RT_EOK;
    struct stm32_record *st_record = (struct stm32_record *)audio->parent.user_data;
    LOG_D("%s:main_type: %d, sub_type: %d", __FUNCTION__, caps->main_type, caps->sub_type);
    switch (caps->main_type)
    {
    case AUDIO_TYPE_MIXER:
    {
        switch (caps->sub_type)
        {
        case AUDIO_MIXER_MUTE:
        {
            break;
        }
        case AUDIO_MIXER_VOLUME:
        {
            int volume = caps->udata.value;
            st_record->record_volume = volume;
            /* set mixer volume */
            wm8988_set_in_valume(_stm32_audio_record.i2c_bus, volume);
            break;
        }
        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }
    case AUDIO_TYPE_INPUT:
    {
        switch (caps->sub_type)
        {
        case AUDIO_DSP_PARAM:
        {
            struct rt_audio_configure config = caps->udata.config;
            st_record->record_config.samplerate = config.samplerate;
            st_record->record_config.samplebits = config.samplebits;
            st_record->record_config.channels = config.channels;
            SAIB_config_set(config);
            break;
        }
        case AUDIO_DSP_SAMPLERATE:
        {
            st_record->record_config.samplerate = caps->udata.config.samplerate;
            SAIB_samplerate_set(caps->udata.config.samplerate);
            break;
        }
        case AUDIO_DSP_CHANNELS:
        {
            st_record->record_config.channels = caps->udata.config.channels;
            SAIB_channels_set(caps->udata.config.channels);
            break;
        }
        case AUDIO_DSP_SAMPLEBITS:
        {
            st_record->record_config.samplebits = caps->udata.config.samplebits;
            SAIB_samplebits_set(caps->udata.config.samplebits);
            break;
        }
        default:
            result = -RT_ERROR;
            break;
        }
        break;
    }
    default:
        break;
    }
    return result;
}


static rt_err_t stm32_player_init(struct rt_audio_device *audio)
{
    /* initialize wm8988 */
    _stm32_audio_play.i2c_bus = rt_i2c_bus_device_find(CODEC_I2C_NAME);

    sai_a_init();
    wm8988_init(_stm32_audio_play.i2c_bus);
    return RT_EOK;
}


static rt_err_t stm32_record_init(struct rt_audio_device *audio)
{
    /* initialize wm8988 */
    // wm8988通过I2C3进行寄存器配置
    _stm32_audio_record.i2c_bus = rt_i2c_bus_device_find(CODEC_I2C_NAME);
    // wm8988的音频流输入是通过SAI_A传输的
    sai_b_init();
    return RT_EOK;
}


static rt_err_t stm32_player_start(struct rt_audio_device *audio, int stream)
{
    if (stream == AUDIO_STREAM_REPLAY)
    {
        DMA2_Stream3->CR |= 1<<0;
    }

    return RT_EOK;
}

static rt_err_t stm32_record_start(struct rt_audio_device *audio, int stream)
{
    rt_kprintf("record start! stream:%d\n",stream);
    if (stream == AUDIO_STREAM_RECORD)
    {
        rt_kprintf("record start!\n");
        DMA2_Stream2->CR |= 1<<0;   // 使能数据流
    }
    return RT_EOK;
}

static rt_err_t stm32_player_stop(struct rt_audio_device *audio, int stream)
{
    if (stream == AUDIO_STREAM_REPLAY)
    {
        DMA2_Stream3->CR &= ~(1<<0);
    }

    return RT_EOK;
}


static rt_err_t stm32_record_stop(struct rt_audio_device *audio, int stream)
{
    rt_kprintf("record stop! stream:%d\n",stream);
    if (stream == AUDIO_STREAM_RECORD)
    {
        rt_kprintf("record stop!\n");
        DMA2_Stream2->CR &= ~(1<<0);    // 禁止数据流
    }
    return RT_EOK;
}

rt_size_t stm32_player_transmit(struct rt_audio_device *audio, const void *writeBuf, void *readBuf, rt_size_t size)
{
    SCB_CleanInvalidateDCache();
    return RT_EOK;
}

rt_size_t stm32_record_transmit(struct rt_audio_device *audio, const void *writeBuf, void *readBuf, rt_size_t size)
{
    SCB_CleanInvalidateDCache();
    return RT_EOK;
}

static void stm32_player_buffer_info(struct rt_audio_device *audio, struct rt_audio_buf_info *info)
{
    /**
     *               TX_FIFO
     * +----------------+----------------+
     * |     block1     |     block2     |
     * +----------------+----------------+
     *  \  block_size  / \  block_size  /
     */
    info->buffer = _stm32_audio_play.tx_fifo;
    info->total_size = TX_DMA_FIFO_SIZE;
    info->block_size = TX_DMA_FIFO_SIZE / 2;
    info->block_count = 2;
}

static void stm32_record_buffer_info(struct rt_audio_device *audio, struct rt_audio_buf_info *info)
{
    /**
     *               TX_FIFO
     * +----------------+----------------+
     * |     block1     |     block2     |
     * +----------------+----------------+
     *  \  block_size  / \  block_size  /
     */
    info->buffer = _stm32_audio_record.rx_fifo;
    info->total_size = RX_DMA_FIFO_SIZE;
    info->block_size = RX_DMA_FIFO_SIZE / 2;
    info->block_count = 2;
}

static struct rt_audio_ops _p_audio_ops =
{
    .getcaps     = stm32_player_getcaps,
    .configure   = stm32_player_configure,
    .init        = stm32_player_init,
    .start       = stm32_player_start,
    .stop        = stm32_player_stop,
    .transmit    = stm32_player_transmit,
    .buffer_info = stm32_player_buffer_info,
};

// 注册设备所需要的各种函数入口
static struct rt_audio_ops _p_record_ops =
{
    .getcaps     = stm32_record_getcaps,
    .configure   = stm32_record_configure,
    .init        = stm32_record_init,
    .start       = stm32_record_start,
    .stop        = stm32_record_stop,
    .transmit    = stm32_record_transmit,
    .buffer_info = stm32_record_buffer_info,
};

int rt_hw_sound_init(void)
{
    rt_uint8_t *tx_fifo;

    tx_fifo = rt_malloc(TX_DMA_FIFO_SIZE);
    if (tx_fifo == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_memset(tx_fifo, 0, TX_DMA_FIFO_SIZE);

    _stm32_audio_play.tx_fifo = tx_fifo;

    _stm32_audio_play.audio.ops = &_p_audio_ops;
    rt_audio_register(&_stm32_audio_play.audio, "sound0", RT_DEVICE_FLAG_WRONLY, &_stm32_audio_play);

    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_sound_init);

// 申请内存空间2Kbyte，这里TX_DMA_FIFO_SIZE的宏定义是2048
static uint8_t rx_fifo[RX_DMA_FIFO_SIZE] __attribute__((section(".ARM.__at_0x24000000")));
int rt_hw_sound_record_init(void)
{
//    rt_uint8_t *rx_fifo;
//    // 申请内存空间2Kbyte，这里TX_DMA_FIFO_SIZE的宏定义是2048
//    rx_fifo = rt_malloc(RX_DMA_FIFO_SIZE);
//    if (rx_fifo == RT_NULL)
//    {
//        return -RT_ENOMEM;
//    }
//    // 申请到的内存空间清零
//    rt_memset(rx_fifo, 0, RX_DMA_FIFO_SIZE);
    // 内存首地址传送给audio_play
    _stm32_audio_record.rx_fifo = rx_fifo;
    // 注册设备mic0，设备类型是read_only
    _stm32_audio_record.audio.ops = &_p_record_ops;
    rt_audio_register(&_stm32_audio_record.audio, "mic0", RT_DEVICE_FLAG_RDONLY, &_stm32_audio_record);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_sound_record_init);

int sai_pin(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    RCC->D2CCIP1R &=~(0x7<<6);
    RCC->D2CCIP1R |=(0x1<<6);

    __HAL_RCC_SAI2_CLK_ENABLE(); 

    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* PI4,7,5,6 */
    GPIO_Initure.Pin=GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;
    GPIO_Initure.Pull=GPIO_PULLUP;
    GPIO_Initure.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_Initure.Alternate=GPIO_AF10_SAI2;

    HAL_GPIO_Init(GPIOI, &GPIO_Initure);

    /* PG10 */
    // GPIO_Initure.Pin=GPIO_PIN_10;
    // HAL_GPIO_Init(GPIOG,&GPIO_Initure);
    GPIO_Initure.Pin        = GPIO_PIN_10;
    GPIO_Initure.Mode       = GPIO_MODE_AF_PP;
    GPIO_Initure.Pull       = GPIO_NOPULL;
    GPIO_Initure.Speed      = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_Initure.Alternate  = GPIO_AF10_SAI2;
    HAL_GPIO_Init(GPIOG, &GPIO_Initure);

    return 0;
}
INIT_BOARD_EXPORT(sai_pin);
