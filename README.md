# AITOY

## 简介

这是一个玩具demo，基于rt-thread的art-pi开发板，实现了一个可以与Chat类AI对话，并显示结果的功能。

## 硬件说明

硬件分为两个部分：[底板](https://item.taobao.com/item.htm?abbucket=12&id=688233426975&ns=1&spm=a21n57.1.0.0.4887523cRG820E)与多媒体[扩展板](https://item.taobao.com/item.htm?id=631635658284&scene=taobao_shop&spm=a1z10.1-c-s.w5003-25322139764.12.10c258f5k9nrLe)。

详细的信息，可以参考上面的淘宝链接，或者在如下[链接](https://art-pi.gitee.io/website/)获取到更多开发相关的知识。

## 环境说明

既然是rt-thread的官方开发板，肯定是基于rt-thread全套来实现。开发环境是rt studio，直接把本项目导入到studio的workspace即可。这个项目也是从SDK的wifi demo起步的。

## 运行效果

看[这里](https://www.bilibili.com/video/BV1cu4m1u7XQ/?vd_source=466089d0ec8d6900e05ca70f2544ac18)

整体的对话体验，识别准确度，以及相应速度，都还行。

## 其他

### 有关模型

1. 语音转文字：Baidu语音转文字，极速版。新用户注册，白嫖的额度；

2. Chart AI：国内的零一大模型，创新工场的出来的，也是注册送60元额度，白嫖。

3. 服务端：为了减轻client端的压力，以及方便后续的管理，有一个run在阿里云的web server；

待续..
