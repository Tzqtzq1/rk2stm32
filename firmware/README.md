# STM32F103RCT6 预编译固件

首次接线验收使用 `stm32f103rct6-demo.hex`。每次 STM32 复位发送 2500 字节，分为 1024、1024、452 三帧，内容为字节位置对 256 取模。

`stm32f103rct6.hex` 是默认固件，未接入真实采集逻辑时不发送数据。

这两个 HEX 面向 **STM32F103RCT6，256KB Flash / 48KB RAM**，由 Arm GNU Toolchain 14.2.Rel1（GCC 14.2.1）构建。HEX 自带 Flash 地址，使用 ST-Link 和 STM32CubeProgrammer 可直接下载。普通构建目录被 Git 忽略，本目录中的 HEX 则随仓库提供。

源码配置见 `925/Makefile`。重新构建默认/验收版本分别使用 `make -C 925` 和 `make -C 925 BUILD=build-rct6-demo DEMO=1`，复制对应构建目录中的 `925.hex` 更新本目录，同时更新 `SHA256SUMS`。

本次发布检查了 Intel HEX 记录校验和、HEX 与 BIN 的一致性、RCT6 栈顶以及 SPI/DMA/EXTI 中断向量。尚需按 [接线与验收指南](../docs/lubancat5-stm32f103rct6.md) 在实板验证。
