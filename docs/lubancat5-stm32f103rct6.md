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

## 2. Windows 电脑通过 ST-Link 下载 STM32

ST-Link 的 USB 插电脑，SWD 引脚连接 STM32 最小系统板。板上标注 SWDIO、SWCLK 的接口分别就是 PA13、PA14，可以直接按丝印连接：

| ST-Link 标识 | STM32 最小系统板 |
|---|---|
| SWDIO / DIO | PA13 / SWDIO |
| SWCLK / CLK | PA14 / SWCLK |
| GND | GND |
| NRST / RST（若有，建议接） | NRST / RST |
| VTref / VAPP（目标电压检测输入） | STM32 板上 3.3V |

STM32 板通过正常供电接口单独供电；ST-Link 上只有 VCC、3.3V 标识时，需要先确认它是供电输出还是电压检测脚。BOOT0 保持 0，从 Flash 启动。ST-Link 不连接鲁班猫的 SPI 排针。

当前构建已匹配 STM32F103RCT6：`STM32F103xE` 宏、高密度 `startup_stm32f103xe.s`、256KB Flash、48KB RAM。ST 将 RC/RD/RE 归入共用的高密度芯片定义，宏中的 xE 不表示实物必须是 RE。参考 [ST 数据手册表 2](https://www.st.com/resource/en/datasheet/stm32f103rc.pdf)。

### 2.1. 先取得可直接下载的 HEX

仓库的 [firmware 目录](../firmware/README.md) 提供了预编译固件，Windows 首次验收无需安装编译器。下面假设项目路径为 `F:\rk2stm32`，在 PowerShell 更新仓库并确认验收 HEX 存在：

```powershell
Set-Location F:\rk2stm32
git pull --ff-only
Test-Path .\firmware\stm32f103rct6-demo.hex
```

`Test-Path` 应输出 `True`。首次用 `stm32f103rct6-demo.hex`，不要选择未接入采集逻辑的默认固件。

### 2.2. 推荐使用 STM32CubeProgrammer

从 [ST 官方页面](https://www.st.com/en/development-tools/stm32cubeprog.html) 下载并安装 Windows 版 STM32CubeProgrammer 及 ST-Link 驱动。打开软件后：

1. 右侧选择 **ST-LINK**，端口选择 **SWD**，点击 **Connect**。
2. 打开 `F:\rk2stm32\firmware\stm32f103rct6-demo.hex`，勾选下载后校验，执行下载。
3. 校验成功后，启动鲁班猫的接收程序，再按一次 STM32 复位键。

也可以调用 STM32CubeProgrammer 自带的 CLI。以下使用默认安装目录；若安装到了其他位置，替换 EXE 路径。整条命令在 PowerShell 中作为一行执行：

```powershell
& "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD freq=1000 -w "F:\rk2stm32\firmware\stm32f103rct6-demo.hex" -v -rst
```

`-w` 写入、紧接的 `-v` 校验、`-rst` 软件复位。HEX 自带下载地址，无需另填地址。参数来源为 [ST 官方 MCU CLI 文档](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html)。

### 2.3. 如果继续使用 OpenOCD 0.12.x

报错“无法将 openocd 项识别为命令”表示没有安装 OpenOCD，或它的 `bin` 目录不在 PATH。Conda 的 `(base)` 提示并不表示安装了 OpenOCD。

OpenOCD 官网列出了 [xPack Windows 二进制发行版](https://openocd.org/pages/getting-openocd.html)。从 [xPack 官方 Releases](https://github.com/xpack-dev-tools/openocd-xpack/releases) 下载适合普通 64 位 Windows 的 `win32-x64.zip`，解压后保留 `bin`、`share` 等目录的相对结构。不要只复制 `openocd.exe`。

假设解压后的 `bin\openocd.exe` 位于 `C:\Tools\OpenOCD`，先检查版本，再用 ST-Link 配置下载；如果实际解压位置不同，替换 EXE 路径：

```powershell
& "C:\Tools\OpenOCD\bin\openocd.exe" --version
& "C:\Tools\OpenOCD\bin\openocd.exe" -f "F:/rk2stm32/925/stlink.cfg" -c "program F:/rk2stm32/firmware/stm32f103rct6-demo.hex verify reset exit"
```

`925/stlink.cfg` 适用于 OpenOCD 0.12.x 的 ST-Link HLA 驱动。已有 OpenOCD 在 PATH 时，也可以从项目根目录执行这一行：

```powershell
openocd -f 925/stlink.cfg -c "program firmware/stm32f103rct6-demo.hex verify reset exit"
```

PowerShell 不使用 Bash 的行尾 `\` 换行；不要把 `-c` 单独作为另一条命令。这里统一提供单行命令，规则参考 [Microsoft PowerShell 文档](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_parsing)。OpenOCD 参数参考 [官方 Flash Programming](https://openocd.org/doc/html/Flash-Programming.html)。

源码自行编译需要 ARM GNU 工具链（`arm-none-eabi-gcc`、`arm-none-eabi-objcopy`）和 make，执行 `make -C 925 BUILD=build-rct6-demo DEMO=1` 后使用 `925/build-rct6-demo/925.hex`。使用预编译 HEX 时不需要这一步。源码改变后必须重新编译；预编译固件不会自动更新。

`925/cmsis-dap.cfg` 仅供 CMSIS-DAP 下载器使用。现在选择 ST-Link，使用 `925/stlink.cfg` 或 STM32CubeProgrammer 的 ST-LINK / SWD 接口。

若能发现 ST-Link 但连接不到 STM32，检查独立供电、共地、SWDIO/SWCLK 接线、目标电压检测和 BOOT0，必要时连接 NRST 并降低 SWD 频率。电脑上的文件与配置检查不等于已经完成实板下载。

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
openocd -f 925/stlink.cfg -c "program 925/build-rct6/925.hex verify reset exit"
```

正常固件默认不生成采集数据，未接入采集逻辑时，RK 一直等待属于预期行为。电脑上的构建和协议测试通过不能代替上述实板验收。
