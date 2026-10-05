# 鲁班猫 5 + STM32F103RCT6：本项目接线、下载与验收

本项目是 STM32 向 RK 发送批量数据：RK 为 SPI 主机，STM32 的 SPI1 为从机，另有 READY 和 ACK 两根握手线。配置为 mode 0、8 bit、MSB first、1MHz。UART、USB 转串口不参与两板的数据通信。

## 1. 两板之间接 7 根线

先断电接线。下表的鲁班猫编号都是 **40Pin 排针的物理脚号**。按板上 1 脚标识定位，奇数为一列、偶数为另一列，不要按芯片 GPIO 编号数排针。

| 信号 | STM32 板上接口 | 普通 LubanCat-5 | LubanCat-5-V2 | 方向 |
|---|---|---|---|---|
| MOSI | PA7 | 19，SPI0_MOSI_M1 | 19，SPI0_MOSI_M1 | RK → STM32 |
| MISO | PA6 | 21，SPI0_MISO_M1 | 21，SPI0_MISO_M1 | STM32 → RK |
| SCK | PA5 | 23，SPI0_CLK_M1 | 23，SPI0_CLK_M1 | RK → STM32 |
| CS0 / NSS | PA4 | **24，SPI0_CS0_M1** | **38，SPI0_CS0_M1** | RK → STM32 |
| READY | PB0 | 8，GPIO1_B6 | 8，GPIO1_B6 | STM32 → RK |
| ACK | PB1 | 10，GPIO1_B7 | 10，GPIO1_B7 | RK → STM32 |
| GND | GND | 6，GND | 6，GND | 共地 |

READY/ACK 的 RK 引脚是为本项目选择的接法；8、10 脚要作为普通 GPIO，不能同时启用 UART1_M1。10 脚的 PWM13_M2 复用也应关闭。其他程序不能同时占用这两根线。

两板按各自板卡规格单独供电，通信仅接上表 7 根线，信号电平为 3.3V。不要把鲁班猫的 5V、12V 接到 STM32 的 GPIO，单独供电时也不连接两板的 3.3V 电源脚。SPI 线尽量短，首次测试建议 10～20cm。避免一块板断电而另一块持续驱动它的 IO。

普通版与 V2 的 CS0 位置来自野火分别提供的 [40Pin 引脚图](https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/40pin/40pin.html)，不能把教程中其他板卡的第 24 脚片选直接套用到 V2。

## 2. 电脑通过 CMSIS-DAP 下载 STM32

CMSIS-DAP 是下载器，不连接鲁班猫的 SPI 排针。下载时这样接：

| CMSIS-DAP 标识 | STM32 |
|---|---|
| SWDIO / DIO | PA13 / SWDIO |
| SWCLK / CLK | PA14 / SWCLK |
| GND | GND |
| nRESET / RST（若有，建议接） | NRST |
| VTref（仅当手册确认是电压检测输入） | STM32 板上 3.3V |

STM32 板单独供电；下载器上标注 VCC、3.3V 的针可能是供电输出，不要直接当 VTref 使用。BOOT0 保持 0，从 Flash 启动。CMSIS-DAP 的 USB 连接电脑。

当前构建已匹配 STM32F103RCT6：`STM32F103xE` 宏、高密度 `startup_stm32f103xe.s`、256KB Flash、48KB RAM。ST 将 RC/RD/RE 归入共用的高密度芯片定义，宏中的 xE 不表示实物必须是 RE。参考 [ST 数据手册表 2](https://www.st.com/resource/en/datasheet/stm32f103rc.pdf)。

电脑需要 ARM GNU 工具链（`arm-none-eabi-gcc`、`arm-none-eabi-objcopy`）和 OpenOCD。macOS 可通过 `brew install openocd` 安装 OpenOCD；Linux 通常通过 `sudo apt install openocd` 安装。Windows 需准备 OpenOCD 可执行文件及随附 scripts 目录，保证 `openocd` 可在终端调用。

在本项目根目录中构建首次验收固件：

```sh
make -C 925 BUILD=build-rct6-demo DEMO=1
```

通过 CMSIS-DAP 下载并校验，完成后复位运行：

```sh
openocd -f 925/cmsis-dap.cfg \
  -c "program 925/build-rct6-demo/925.hex verify reset exit"
```

HEX 已包含 Flash 地址，无需另填 `0x08000000`。OpenOCD 的 `program` 命令会擦除将要写入的 Flash 扇区，覆盖这些扇区中的旧程序。下载成功应看到写入与校验成功的提示。命令参数参考 [OpenOCD 官方 Flash Programming](https://openocd.org/doc/html/Flash-Programming.html)。

如果提示找不到 CMSIS-DAP，检查数据 USB 线和下载器模式；双模式下载器应设为 CMSIS-DAP。若能发现下载器但连接不到 STM32，检查独立供电、共地、PA13/PA14 接线，必要时连接 NRST 并将配置中的 `adapter speed` 降至 100。

## 3. 鲁班猫准备 Linux 和 SPI

RK 端 `rk3588/rk_read.c` 是 Linux 用户态程序。鲁班猫需要运行其对应板型的 Debian/Ubuntu 镜像；已经能启动 Linux 时，只传输程序，不需要重刷系统。还没有系统时，先按野火 [RK3588 快速使用手册中的系统镜像烧录章节](https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/index.html) 安装对应普通版或 V2 的镜像。

在 **鲁班猫终端** 操作：

```sh
sudo fire-config
```

在外设/设备树配置中开启 **SPI0_M1 与 CS0**，使用对应当前板型的 SPI 片选配置。关闭 **UART1_M1** 和 **PWM13_M2**，让物理 8、10 脚空出来作 GPIO。保存退出后重启：

```sh
sudo reboot
```

重新登录鲁班猫后检查：

```sh
ls /dev/spidev*
gpiodetect
```

常见结果包含 `/dev/spidev0.0`。`gpiodetect` 应能定位到标签为 `gpio1`、拥有 32 条线的控制器，通常是 `/dev/gpiochip1`；若实际编号不同，以输出中 `gpio1` 对应的设备为准。若没有 `gpiodetect`，可在鲁班猫运行 `sudo apt install gpiod`。

本项目用 GPIO character-device 接口，其线号是 **控制器内偏移**：

- 物理 8 脚 GPIO1_B6：READY line = `8 + 6 = 14`。
- 物理 10 脚 GPIO1_B7：ACK line = `8 + 7 = 15`。

不要把物理脚号 8/10 或旧 sysfs 全局编号 46/47 填进 `rk_read`。驱动须支持 GPIO character-device v1；本项目直接使用 ioctl，不需要链接 libgpiod 库。SPI 启用步骤参考 [野火 SPI 教程](https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/40pin/spi/spi.html)，编号规则参考 [野火 GPIO 教程](https://doc.embedfire.com/linux/rk3588/quick_start/en/latest/quick_start/40pin/gpio/gpio.html)。

## 4. 将 RK 程序传到鲁班猫并运行

鲁班猫接网络后，通过 SSH/SCP 或 U 盘复制本项目的 `rk3588` 与 `common` 两个目录，保持它们并列。例如在电脑的项目根目录运行下列命令，先将用户名和 IP 换成实际值：

```sh
scp -r rk3588 common 用户名@鲁班猫IP:~/
```

然后在 **鲁班猫** 编译执行：

```sh
cd ~
make -C rk3588
sudo ./rk3588/rk_read /dev/spidev0.0 /dev/gpiochip1 14 15 > batch.bin
```

如果鲁班猫没有 gcc/make，安装 `sudo apt install build-essential`。若上一步检查发现 `gpio1` 对应另一 gpiochip 编号，替换命令中的 `/dev/gpiochip1`。同理，SPI 节点需对应实际启用的 SPI0_M1/CS0。

这一步是在鲁班猫上编译 Linux 程序；不要把 macOS/Windows 本机编译的可执行文件直接复制过去当 ARM64 Linux 程序运行。

## 5. 验收

验收固件每次 STM32 复位只发送一组 2500 字节。两板接线、上电并启动 `rk_read` 后，必要时按一次 STM32 复位键；RK 终端应看到：

```text
Accepted seq=1 length=1024
Accepted seq=2 length=1024
Accepted seq=3 length=452
```

读取程序之后会继续等待。按 Ctrl+C 停止，在鲁班猫检查数据：

```sh
python3 -c 'from pathlib import Path; data=Path("batch.bin").read_bytes(); assert data == bytes(i % 256 for i in range(2500)); print("PASS: 2500 bytes")'
```

看到 `PASS: 2500 bytes` 才说明验收通过。若没有输出，检查是否烧录了 `DEMO=1` 的验收固件、READY/ACK 接法及 GPIO 复用；若出现 `Invalid frame`，优先检查 CS0 版型差异、MISO/SCK、共地和 SPI 使能。

正式运行时将真实采集逻辑接入 `925/Core/Src/main.c`，按 README 的 Write/Flush/Process 接口处理背压。正常固件的构建和下载命令是：

```sh
make -C 925
openocd -f 925/cmsis-dap.cfg \
  -c "program 925/build-rct6/925.hex verify reset exit"
```

正常固件默认不生成采集数据，未接入采集逻辑时，RK 一直等待属于预期行为。电脑上的构建和协议测试通过不能代替上述实板验收。
