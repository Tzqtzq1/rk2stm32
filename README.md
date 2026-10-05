# STM32F103 → RK3588 批量 SPI 通信（协议 v2）

STM32 将数据按最多 1024 字节组帧。RK 是 SPI 主机，校验帧头和数据，写入标准输出后才确认接收。协议 v2 与旧版不兼容，两端必须一起更新。

实物为 **鲁班猫 5 + STM32F103RCT6** 时，按 [接线、烧录与首次验收指南](docs/lubancat5-stm32f103rct6.md) 操作。鲁班猫 5 与 5-V2 的 SPI CS0 物理脚分别为 24、38，不能混用。

Windows + ST-Link 首次验收可直接下载仓库内的 [预编译验收 HEX](firmware/stm32f103rct6-demo.hex)，通过 STM32CubeProgrammer 的 ST-LINK / SWD 接口写入并校验，无需先编译或安装 OpenOCD。PowerShell 命令及 OpenOCD 0.12.x 的 ST-Link 接法见上述指南；`cmsis-dap.cfg` 仅用于 CMSIS-DAP 下载器。

## 接线

共地，3.3V 电平。这里的 GPIO 编号必须换成实际板卡的 gpiochip 内线号。

| STM32F103 | RK3588 |
|---|---|
| PA5 SCK | SPI SCLK |
| PA6 MISO | SPI MISO |
| PA7 MOSI | SPI MOSI |
| PA4 NSS | SPI CS |
| PB0 READY 输出 | GPIO 输入 |
| **PB1 ACK 输入（新增）** | **GPIO 输出，初始低** |

READY/ACK 使用四阶段握手：STM32 武装 DMA 后 READY=1；RK 完成读取、CRC 校验和输出后 ACK=1；STM32 确认传输完成且 CS 已释放后 READY=0；RK 观察到 READY=0 后 ACK=0。STM32 观察到 ACK=0 才释放队首帧并开始下一帧。高、低阶段均保持到对端确认，不要求轮询捕获短脉冲。

## 完整固件示例

`925` 包含启动文件、HAL/CMSIS、主程序、DMA/中断配置和链接脚本。当前目标为 **STM32F103RCT6，256KB Flash / 48KB RAM**；使用 `STM32F103xE` 宏与 `startup_stm32f103xe.s`（ST 的高密度 F103 共用定义，适用于 RC）。默认使用内部 HSI 8MHz，无需外部晶振。旧 C8 链接脚本仅保留作参考，当前构建不使用；其他 F103 型号需要重新匹配芯片配置。

安装 ARM GNU 工具链并将 `arm-none-eabi-gcc` 放到 PATH 后：

```sh
make -C 925
# 输出：925/build-rct6/925.elf、925.bin、925.hex、925.map
```

也可以使用 CMake：

```sh
cmake -S 925 -B 925/build-cmake-rct6 -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake
cmake --build 925/build-cmake-rct6
```

固件默认等待应用提供数据，不产生虚构的采集数据。将实际采集逻辑接入 `925/Core/Src/main.c` 的主循环。

可选上板验收固件（单独构建目录，避免复用默认固件对象）：

```sh
make -C 925 BUILD=build-rct6-demo DEMO=1
```

烧录 `925/build-rct6-demo/925.hex` 后，在 RK 端按下方命令接收。正常结果应为 2500 字节（1024、1024、452 三帧），每个字节等于其位置对 256 取模。接收三帧后读取程序会继续等待，可按 Ctrl+C 停止，再检查：

```sh
python3 -c 'from pathlib import Path; data=Path("batch.bin").read_bytes(); assert data == bytes(i % 256 for i in range(2500)); print("PASS: 2500 bytes")'
```

CMake 对应选项为 `-DBATCH_DEMO=ON`。此模式只用于通信验收，正常构建默认关闭。

## 应用接口及迁移

必须在**同一个主循环上下文**调用 Init、Write、Flush、Process、Recover；中断只调用三个事件回调。不支持多个并发生产者。

`BatchLink_Write()` 现在返回**实际接收的字节数**；0 表示背压，-1 表示无效参数。禁止将旧版的“0 代表全部成功”逻辑沿用到新版本。调用者保留未接收的尾部，例如：

```c
uint16_t offset = 0;
while (offset < len) {
    BatchLink_Process();
    if (BatchLink_Error()) {
        /* 报告错误，保留 data 和 offset；由应用决定何时恢复。 */
        break;
    }
    int accepted = BatchLink_Write(data + offset, len - offset);
    if (accepted < 0) break;
    offset += (uint16_t)accepted;
    /* 实际程序在每次主循环中推进该状态，不要阻塞其他采集任务。 */
}
/* 全部输入提交后，在流结束时 Flush；返回 -1 就在后续主循环重试。 */
```

不足 1024 字节的尾部使用 `BatchLink_Flush()` 发送。持续低速采集可由应用按采集周期调用 Flush。Write/Flush 只排队，必须持续调用 Process 才能发送及完成握手。

集成到已有 CubeMX 工程时：SPI1 从机，mode 0，8bit、MSB first、硬件 NSS；TX DMA1 Channel3，normal、字节对齐。PA4 同时配置下降沿 EXTI，SPI1 引脚不重映射。PB0 输出，PB1 输入下拉。SysTick 优先级须高于 DMA 和 SPI 中断，以保证 HAL 中断内的超时能推进。参考示例中的完整初始化。

```c
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) BatchLink_OnTxDone();
}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) BatchLink_OnError();
}
void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    if (pin == GPIO_PIN_4) BatchLink_OnNssFall();
}
```

## RK3588

在目标 Linux 板上编译，系统须支持 GPIO character-device v1 接口，并在设备树中启用对应 spidev 和 GPIO 引脚：

```sh
make -C rk3588
# 鲁班猫 5 按接线指南连接，且 gpio1 对应 gpiochip1 时：
sudo ./rk3588/rk_read /dev/spidev0.0 /dev/gpiochip1 14 15 > batch.bin
```

最后两个参数分别为 READY、ACK 线号；上例对应鲁班猫 5 物理 8、10 脚的 GPIO1_B6、GPIO1_B7，其他接法不能照搬。GPIOCHIP 须根据目标板的 `gpiodetect` 确认。SPI 使用 mode 0、1MHz。没有数据时正常等待；ACK 后 READY 没有在 10 秒内拉低则报错退出。

## 错误与恢复

- STM32 DMA 启动失败、HAL 错误、CS 开始后传输超时（5 秒）、ACK 握手超时（10 秒）均拉低 READY，并保留队列。RK 尚未开始读取时，STM32 不因空闲等待超时。
- `BatchLink_Error()` 返回故障码：`0x80000000` 传输超时、`0x40000000 | HAL状态` 启动失败、`0x20000000` 握手超时；其他值为 HAL SPI 错误码。
- CRC 错误、短传输、序号跳变、输出失败时，RK 报错退出，不发送接收 ACK。STM32 随后进入超时故障，数据仍保留。
- 修复故障、停止 RK 读取、确认 CS 高且 ACK 低后，应用可调用 `BatchLink_Recover()`，成功返回 0。随后 Process 重发保留的队首帧。不要重新 Init 来恢复，否则会清空队列。
- 本版本采用显式故障停止和恢复，不自动无限重试。应用需要记录故障并协调重启。RK 进程内会去重；进程重启或设备复位后没有持久化的去重记录，恢复时可能重复输出。标准输出的 flush 表示交给操作系统，不表示文件已持久化到磁盘。

有限队列只能提供背压。若采集源无法暂停，应用必须提供额外队列或明确记录溢出；不能忽略 Write 返回的未接收字节。

## 验证

```sh
make -C tests test
```

主机测试使用 AddressSanitizer/UndefinedBehaviorSanitizer，覆盖队列背压、尾帧、四阶段握手、DMA 失败后保留与恢复、传输超时、帧头 CRC、RK 连续帧、短读、坏帧、输出失败、握手超时及资源释放。RK 测试模拟 Linux ioctl；不能替代目标板驱动及接线验证。

已用 ARM GCC 14.2.1 完成 STM32F103RCT6 正常/验收固件 Make 构建及正常固件 CMake 构建，并核对 48KB RAM 栈顶及 SPI/DMA/EXTI 中断向量。上板需要验证真实 SPI 时序、GPIO 编号和采集逻辑。本仓库未包含你原有的采集程序。

依赖版本及来源见 `925/Drivers/VERSIONS.md`；使用 ST 官方 [STM32F1 HAL](https://github.com/STMicroelectronics/stm32f1xx-hal-driver/tree/v1.1.10) 和 CMSIS，保留原始许可证。
