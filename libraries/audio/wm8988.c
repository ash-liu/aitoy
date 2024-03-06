/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-02     liuduanfei   first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <wm8988.h>

#define DBG_TAG "wm8988"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define  LEFT_INPUT_VOLUME     (0x00 << 9)  /* R0  */
#define  RIGHT_INPUT_VOLUME    (0x01 << 9)  /* R1  */
#define  LOUT1_VOLUME          (0x02 << 9)  /* R2  */
#define  ROUT1_VOLUME          (0x03 << 9)  /* R3  */
                                            /* R4  */
#define  ADC_DAC_CONTROL       (0x05 << 9)  /* R5  */
                                            /* R6  */
#define  AUDIO_INTERFACE       (0x07 << 9)  /* R7  */
#define  SAMPLE_RATE           (0x08 << 9)  /* R8  */
                                            /* R9  */
#define  LEFT_DAC_VOLUME       (0x0A << 9)  /* R10 */
#define  RIGHT_DAC_VOLUME      (0x0B << 9)  /* R11 */
#define  BASS_CONTROL          (0x0C << 9)  /* R12 */
#define  TREBLE_CONTROL        (0x0D << 9)  /* R13 */
#define  RESET                 (0x0F << 9)  /* R15 */
#define  THREED_CONTROL        (0x10 << 9)  /* R16 */
#define  ALC1                  (0x11 << 9)  /* R17 */
#define  ALC2                  (0x12 << 9)  /* R18 */
#define  ALC3                  (0x13 << 9)  /* R19 */
#define  NOISE_GATE            (0x14 << 9)  /* R20 */
#define  LEFT_ADC_VOLUME       (0x15 << 9)  /* R21 */
#define  RIGHT_ADC_VOLUME      (0x16 << 9)  /* R22 */
#define  ADDITIONAL_CONTROL_1  (0x17 << 9)  /* R23 */
#define  ADDITIONAL_CONTROL_2  (0x18 << 9)  /* R24 */
#define  PWR_MGMT_1            (0x19 << 9)  /* R25 */
#define  PWR_MGMT_2            (0x1A << 9)  /* R26 */
#define  ADDITIONAL_CONTROL    (0x1B << 9)  /* R27 */
#define  ADC_INPUT_MODE        (0x1F << 9)  /* R31 */
#define  ADCL_SIGNAL_PATH      (0x20 << 9)  /* R32 */
#define  ADCR_SIGNAL_PATH      (0x21 << 9)  /* R33 */
#define  LEFT_OUT_MIX_1        (0x22 << 9)  /* R34 */
#define  LEFT_OUT_MIX_2        (0x23 << 9)  /* R35 */
#define  RIGHT_OUT_MIX_1       (0x24 << 9)  /* R36 */
#define  RIGHT_OUT_MIX_2       (0x25 << 9)  /* R37 */
                                            /* R38 */
                                            /* R39 */
#define  LOUT2_VOLUME          (0x28 << 9)  /* R40 */
#define  ROUT2_VOLUME          (0x29 << 9)  /* R41 */
                                            /* R42 */
#define  LOW_POWER_PLAYBACK    (0x43 << 9)  /* R67 */

/* R10 */
#define LDVU      (1<<8)
#define LDACVOL

/* R11 */
#define RDVU      (1<<8)
#define RDACVOL


static rt_uint16_t wm8988_regval_tbl[68] =
{
    0x0097, 0x0097, 0x0079, 0x0079, 0x0000, 0x0008, 0x0000, 0x000A,   /* 00-07 */
    0x0000, 0x0000, 0x00FF, 0x00FF, 0x000F, 0x000F, 0X0000, 0X0000,   /* 08-15 */
    0x0000, 0x007B, 0X012C, 0X002C, 0X002C, 0X002C, 0X002C, 0X0000,   /* 16-23 */
    0X0032, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,   /* 24-31 */
    0X0038, 0X000B, 0X0032, 0X0000, 0X0008, 0X000C, 0X0093, 0X00E9,   /* 32-39 */
    0X0000, 0X0000, 0X0000, 0X0000, 0X0003, 0X0010, 0X0010, 0X0100,   /* 40-47 */
    0X0100, 0X0002, 0X0001, 0X0001, 0X0039, 0X0039, 0X0039, 0X0039,   /* 48-55 */
    0X0100, 0X0002, 0X0001, 0X0001, 0X0039, 0X0039, 0X0039, 0X0039,   /* 56-63 */
    0X0100, 0X0002, 0X0001, 0X0001,                                   /* 64-67 */
};

#define wm8988_addr (0x1A)

static void wm8988_reg_wrtie(struct rt_i2c_bus_device *dev, rt_uint16_t data)
{
    rt_uint8_t buf[2];

    wm8988_regval_tbl[data >> 9] = data & 0x1FF;

    buf[0] = (rt_uint8_t)(data >> 8);
    buf[1] = (rt_uint8_t)(data);

    struct rt_i2c_msg msg;

    msg.addr  = wm8988_addr;
    msg.buf   = buf;
    msg.flags = RT_I2C_WR;
    msg.len   = 2;

    if (rt_i2c_transfer(dev, &msg, 1) != 1)
    {
        LOG_D("wm8988 write register failed.");
    }
}

static rt_uint16_t wm8988_reg_read(struct rt_i2c_bus_device *dev, rt_uint16_t reg)
{
    return wm8988_regval_tbl[reg >> 9];
}

void wm8988_set_out_valume(struct rt_i2c_bus_device *dev, rt_uint8_t val)
{
    wm8988_reg_wrtie(dev, LOUT1_VOLUME | (1<<8) | (1<<7) | ((128*val)/100));
    wm8988_reg_wrtie(dev, ROUT1_VOLUME | (1<<8) | (1<<7) | ((128*val)/100));
}

void wm8988_set_in_valume(struct rt_i2c_bus_device *dev, rt_uint8_t val)
{
    // wm8988_reg_wrtie(dev, LEFT_INPUT_VOLUME | (1<<8) | val);
    // wm8988_reg_wrtie(dev, RIGHT_INPUT_VOLUME | (1<<8) | val);
}

#if 0
void wm8988_init(struct rt_i2c_bus_device *dev)
{
    /* reset */
    wm8988_reg_wrtie(dev, RESET | 0x0);
    rt_thread_mdelay(100);

    /* power up */

    /* Enable Vmid and VREF. */
    wm8988_reg_wrtie(dev, PWR_MGMT_1 | 0xFC);
    /* Enable DACs as required */
    /* Enable line and / or headphone output buffers as required. */
    wm8988_reg_wrtie(dev, PWR_MGMT_2 | 0x1E0);

    rt_thread_mdelay(100);

    /* Set DACMU = 0 to soft-un-mute the audio DACs */
    wm8988_reg_wrtie(dev, ADC_DAC_CONTROL | 0x4);
    /* Thermal shutdown enabled */
    wm8988_reg_wrtie(dev, ADDITIONAL_CONTROL_1 | 0x1C0);
    wm8988_reg_wrtie(dev, ADDITIONAL_CONTROL_2 | 0x80);
    /* Disable master mode 16-bit */
    wm8988_reg_wrtie(dev, AUDIO_INTERFACE | 0x2);

    wm8988_reg_wrtie(dev, LEFT_OUT_MIX_1 | 0x150);

    wm8988_reg_wrtie(dev, LEFT_OUT_MIX_2 | 0x150);

    wm8988_reg_wrtie(dev, RIGHT_OUT_MIX_1 | 0x150);

    wm8988_reg_wrtie(dev, RIGHT_OUT_MIX_2 | 0x150);

//    wm8988_reg_wrtie(dev, LEFT_DAC_VOLUME | 0x01);

//    wm8988_reg_wrtie(dev, RIGHT_DAC_VOLUME | 0x01);

    wm8988_reg_wrtie(dev, BASS_CONTROL | 0x07);

    wm8988_reg_wrtie(dev, TREBLE_CONTROL | 0x07);

    wm8988_set_out_valume(dev, 58);

    wm8988_reg_read(dev, 0x00);
}
#endif

void wm8988_init(struct rt_i2c_bus_device *dev)
{
    // R15寄存器，向此寄存器写入数据，则会软件复位芯片
    wm8988_reg_wrtie(dev, RESET | 0x0);
    // 等待芯片复位
    rt_thread_mdelay(100);
    // 先关闭电源配置，完成基础配置之后再打开电源配置
    // R25 配置master clock disable，关闭ADCR,ADCL,AINR,AINL,VREF的power，VMIDSEL OFF
    // R26 配置关闭ROUT2,LOUT2,ROUT1,LOUT1,DACR,DACL的power
    wm8988_reg_wrtie(dev, PWR_MGMT_1|0x01);
    wm8988_reg_wrtie(dev, PWR_MGMT_2|0x00);   //inactive
    // 配置ADC输入
    // R32 配置Left Channel Microphone Gain Boost=00，Boost off (bypassed)
    //     配置Left Channel Input Select=01，LINPUT2
    // R33 配置Right Channel Microphone Gain Boost=00，Boost off (bypassed)
    //     配置Right Channel Input Select=01，RINPUT2
    wm8988_reg_wrtie(dev, ADCL_SIGNAL_PATH|0x00);  //0x00选通输入1  //0x40选通输入2
    wm8988_reg_wrtie(dev, ADCR_SIGNAL_PATH|0x00);  //0x00选通输入1  //0x40选通输入2
    // Left out Mix 1/2  //这里只走DAC转换的数据
    // R34 Left Input Selection for Output Mix = 010,Reserved //000选linput1，001选linput2，011选leftADCinput
    //     LMIXSEL Signal to Left Mixer Volume = 010
    //     LMIXSEL Signal to Left Mixer = 0,Disable (Mute)
    //     Left DAC to Left Mixe = 1,Enable Path
    // R35 RMIXSEL Signal to Left Mixer Volume = 111,-15dB
    //     RMIXSEL Signal to Left Mixer = 0,Disable (Mute)
    //     Right DAC to Left Mixer = 0,Disable (Mute)
    wm8988_reg_wrtie(dev, LEFT_OUT_MIX_1|0x122);       //0x122 dac输出     //0xa3 mic输出
    wm8988_reg_wrtie(dev, LEFT_OUT_MIX_2|0x70);        //0x70 dac输出       //0x23 mic输出
    //Right out Mix 1/2
    // R36 Right Input Selection for Output Mix = 010,Reserved ////000选rinput1，001选rinput2，011选rightADCinput
    //     LMIXSEL Signal to Right Mixer Volum = 111,-15dB
    //     LMIXSEL Signal to Right Mixer = 0,Disable (Mute)
    //     Left DAC to Right Mixer = 1,Disable (Mute)
    // R37 RMIXSEL Signal to Right Mixer Volume = 010
    //     RMIXSEL Signal to Right Mixer = 0,Disable (Mute)
    //     Right DAC to Right Mixer = 1,Enable Path
    wm8988_reg_wrtie(dev, RIGHT_OUT_MIX_1|0x70);      //0x70 dac输出       //0x23 mic输出
    wm8988_reg_wrtie(dev, RIGHT_OUT_MIX_2|0x122);     //0x122 dac输出      //0xa3 mic输出
    // R25 配置master clock enabled，配置使能ADCR,ADCL,AINR,AINL,VREF的power，配置VMIDSEL=10,500k divider enabled (for low-power standby)
    // R26 配置使能LOUT2,ROUT1,LOUT1,DACR,DACL的power，关闭ROUT2的power
    // wm8988_DVE0_write_reg(WM8988_PWR1,0x1fc);  //
    // wm8988_DVE0_write_reg(WM8988_PWR2,0x1f8);  //active·
    wm8988_reg_wrtie(dev, PWR_MGMT_1|0xfc);            //0xfc //设置50K欧divider //0x17c //设置500k欧
    wm8988_reg_wrtie(dev, PWR_MGMT_2|0x1f8);            //0x1e0//关闭ROUT2,LOUT2的power//0X1F8//打开LOUT2,ROUT2
    // ADC and DAC Contrl
    // R5 配置Digital Soft Mute=1，mute，静音
    //    De-emphasis Control=00，No De-emphasis
    wm8988_reg_wrtie(dev, ADC_DAC_CONTROL|0x08);
    // LR Input volume
    // R0 R1配置左右输入声道音量
    // Channel Input Volume Control = 010111，0dB
    // Channel Zero Cross Detector = 0，Change gain immediately
    // Channel Input Analogue Mute = 0，Disable Mute
    // Volume Update = 1，Update left and right channel gains
    wm8988_reg_wrtie(dev, LEFT_INPUT_VOLUME|0x127);     //  100111，0dB
    wm8988_reg_wrtie(dev, RIGHT_INPUT_VOLUME|0x127);    //  100111，0dB
    // LR Out1 volume
    // R2 R3配置左右输出1声道音量
    // OUT1 Volume = 0
    // zero cross enable = 0，Change gain immediately
    // Right Volume Update = 1，Update left and right channel gains
    wm8988_reg_wrtie(dev, LOUT1_VOLUME|0x100);  //0dB:179
    wm8988_reg_wrtie(dev, ROUT1_VOLUME|0x100);
    // LR OUT2 VOL
    // R40 配置LOUT2音量 LOUT2 Volume = 1111001，0dB
    //     Left zero cross enable = 0，Change gain immediately
    //     Left Volume Update = 1，Update left and right channel gains
    // R41 配置ROUT2音量 ROUT2 Volume = 0
    //     Right zero cross enable = 0
    //     Right Volume Update = 1，Update left and right channel gains
    wm8988_reg_wrtie(dev, LOUT2_VOLUME|0x100);  //0dB
    wm8988_reg_wrtie(dev, ROUT2_VOLUME|0x100);  //
    // LR ADC Digital Vol
    // R21 Left ADC Digital Volume Control = 11000011,0dB
    //     Left ADC Volume Update = 1,Update left and right channel gains
    // R22 Right ADC Digital Volume Control = 11000011,0dB
    //     Right ADC Volume Update = 1,Update left and right channel gains
    // wm8988_reg_wrtie(dev, LEFT_ADC_VOLUME|0x1c3);  //MIC 默认 0 dB
    // wm8988_reg_wrtie(dev, RIGHT_ADC_VOLUME|0x1c3);
    wm8988_reg_wrtie(dev, LEFT_ADC_VOLUME|0x1d3);  //MIC 默认 0 dB
    wm8988_reg_wrtie(dev, RIGHT_ADC_VOLUME|0x1d3);
    // LR DAC Volume
    // R10 R11 配置 DAC Digital Volume Control = 11111111，0dB
    //         DAC Volume Update = 1，Update left and right channel gains
    wm8988_reg_wrtie(dev, LEFT_DAC_VOLUME|0x1ff);  //0dB  //0x1ff
    wm8988_reg_wrtie(dev, RIGHT_DAC_VOLUME|0x1ff);  //
    // R7 配置Audio Data Format Select = 10，I2S Format
    //    Audio Data Word Length = 00，16bit
    wm8988_reg_wrtie(dev, AUDIO_INTERFACE|0x02);  //16bit I2s
    // Sample Rate
    // R8 配置 Clocking Mode Select = 0，‘Normal’ Mode
    //    Sample Rate Control = 00001
    wm8988_reg_wrtie(dev, SAMPLE_RATE|0x20);
    // Bass/Treble Control应该是音效调节，感觉效果不明显
    // R12 配置 BASS=1111,disable
    // R13 配置 Treble=1111，disable
    wm8988_reg_wrtie(dev, BASS_CONTROL|0x07);  //0x07
    wm8988_reg_wrtie(dev, TREBLE_CONTROL|0x07);//0x07
    // 3D enhance
    // R16 配置3D环绕音效=0，全部取消
    wm8988_reg_wrtie(dev, THREED_CONTROL|0x00);
    // ALG Ctrl 1/2/3 auto level control
    // R17 按照默认值写入
    // R18 按照默认值写入
    // R19 按照默认值写入
    wm8988_reg_wrtie(dev, ALC1|0x7B);
    wm8988_reg_wrtie(dev, ALC2|0x00);
    wm8988_reg_wrtie(dev, ALC3|0x32);
    // Noise Gate
    // R20 按照默认值写入
    wm8988_reg_wrtie(dev, NOISE_GATE|0x00);
    // Additional ctrl 1/2/3
    // R23 bit8 1，使能温度保护措施，温度超过阈值会关闭OUT1L/OUT1R,OUT2L/OUT2R
    //     bit7：6， Lowest bias current, optimized for AVDD=3.3V
    // R24 bit7 1，Enable Common Mode Feedback
    // R27 默认值0
    wm8988_reg_wrtie(dev, ADDITIONAL_CONTROL_1|0x1C0);
    wm8988_reg_wrtie(dev, ADDITIONAL_CONTROL_2|0x80);
    wm8988_reg_wrtie(dev, ADDITIONAL_CONTROL|0x00);
    // Low Power Playback
    // R67 默认值
    wm8988_reg_wrtie(dev, LOW_POWER_PLAYBACK|0x00);
    // ADC input mode
    // R31 配置 Differential input select = 1， LINPUT2 – RINPUT2
    //     mode = 00，Stereo
    //     Right Channel DC Measurement = 0，Normal Operation, PGA Enabled
    //     Left Channel DC Measurement = 0，Normal Operation, PGA Enabled
    wm8988_reg_wrtie(dev, ADC_INPUT_MODE|0x100);   //0x00
    // ADC and DAC Contrl R5寄存器，BIT3写0，不静音，BIT2:1写10，44.1khz采样率，针对DAC输出的就配置这三个bit
    wm8988_reg_wrtie(dev, ADC_DAC_CONTROL|0x04);
    wm8988_set_out_valume(dev, 58);
    wm8988_reg_read(dev, 0x00);
}
