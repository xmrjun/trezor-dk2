# Trezor 固件移植到 STM32U5G9J-DK2（Safe 7 界面 + 中文）

把 Trezor 官方固件（2.13.1，基于提交 `6f320a4`）移植到 ST 的开发板 **STM32U5G9J-DK2**：

- 5 寸 800×480 RGB 屏竖着用，跑 **Trezor Safe 7 的界面**（eckhart），带设备上的设置菜单
- GT911 电容触摸
- **简体中文界面**（944 条翻译，汉字点阵编进固件）
- 通过 USB 连接 Trezor Suite、MetaMask（Trezor Connect）等

> ⚠️ **仅供学习和测试，不要放真钱。** 这是开发版：使用 Trezor 公开的开发密钥签名、芯片 RDP Level 0（调试器可读出全部 Flash）、没有安全芯片。任何能接触板子的人都能读出钱包种子。只用测试网和测试助记词。

English summary: port of Trezor firmware to the STM32U5G9J-DK2 (D002 model, new board `dk2`) with the eckhart (Safe 7) UI rotated to portrait, GT911 touch and a Chinese translation whose CJK glyphs are compiled into the firmware. Development keys, unlocked chip — for testing only.

## 为什么是 DK2

Trezor 官方的 D002 配置是给 **STM32U5G9J-DK1**（DSI 屏 + Sitronix 触摸）写的。DK1 已经停产，DK2 用同一颗芯片，但屏幕（RGB 并口 RK050HR18）和触摸（GT911）都不同，原版固件刷进去只会黑屏或报 "Touch screen panel was not loaded properly"。

## 改了什么

| 部分 | 文件 |
|---|---|
| DK2 板级配置（新板 `-b dk2`） | `core/embed/models/D002/boards/dk2.toml`、`stm32u5g9j-dk2.h` |
| RGB 并口屏驱动（跳过 DSI，LTDC 直驱），竖屏旋转 + 1.25 倍放大 | `core/embed/io/display/ltdc_dsi/panels/stm32u5g9j-dk2/`、`display_driver.c`、`display_fb.c` |
| GT911 触摸驱动（坐标随旋转/放大换算） | `core/embed/io/touch/gt911/gt911.c` |
| "固定 USB 供电" 电源管理（DK2 无电池/PMIC） | `core/embed/io/power_manager/fixed/` |
| Safe 7 界面在无蓝牙/电源键/RGB 灯设备上编译 | `rust/src/ui/layout_eckhart/...`、`rust/src/bootloader/mod.rs`、`projects/bootloader/main.c` |
| 设备菜单不再写死只给 T3W1 | `core/src/apps/homescreen/*.py`、`core/embed/upymod/build.rs` |
| 内置中文字库 + 中文断行 | `rust/src/ui/display/cjk.rs`、`cjk/*.bin`、`rust/src/ui/component/text/layout.rs` |
| 中文翻译和工具 | `dk2/zh/` |

## 需要的东西

- STM32U5G9J-DK2 一块，两根能传数据的 USB-C 线
- macOS 或 Linux，[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
- 编译工具（版本见 `flake.nix`，不用 Nix 也行）：
  - ARM GNU Toolchain 13.3（`arm-none-eabi-gcc`）
  - Rust：在仓库根目录 `rustup override set nightly-2026-03-16`，`rustup target add thumbv8m.main-none-eabihf --toolchain nightly-2026-03-16`
  - Python 3.12 + [uv](https://docs.astral.sh/uv/)：`uv sync`
  - macOS 需要 Xcode 命令行工具（提供 libclang）

## 编译

```bash
git submodule update --init --recursive
uv sync
export PATH="<arm-gnu-toolchain>/bin:$PATH"
dk2/build.sh            # 生成 core/build/artifacts/D002/{boardloader,bootloader,firmware}.bin
```

## 刷机

1. **板子设置**：BOOT0 拨片在 `0 Internal Flash`；JP1（Board Power）跳线在 `STLK` 或 `USB`。
2. **CN8（ST-LINK）接电脑。** Apple 芯片的 Mac 上 ST-LINK 直插偶尔会 USB 超时，经 Hub 更稳。
3. **打开 TrustZone**（只需一次，RDP 保持 Level 0，随时可以关回去）：

   ```bash
   STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob TZEN=1
   STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob SECBOOTADD0=0x180080   # 0x0C004000
   ```

4. **刷入**：`dk2/flash.sh`（整片擦除后写入三部分）。以后只更新固件用 `dk2/flash.sh firmware`（保留钱包和语言包）。
5. 第一次启动时 boardloader 会自动设置安全水位线和 HDP 隐藏区（Trezor 开发版正常行为）。

## 中文

```bash
cd dk2/zh
# 字体：Noto Sans SC（SIL OFL），把 NotoSansSC-Regular.otf、NotoSansSC-Medium.otf 放进 dk2/zh/fonts/
#   https://github.com/notofonts/noto-cjk/tree/main/Sans/SubsetOTF/SC
python zh_tool.py json                       # 合并翻译 -> zh.json
python gen_cjk_font.py zh.json ../../core/embed/rust/src/ui/display/cjk   # 只在改了翻译时需要
python zh_tool.py blob                       # 生成语言包（开发密钥签名）
trezorctl set-language translation-D002-zh-CN-2.13.1.bin   # 在设备上确认
```

仓库里已经带了生成好的 `cjk/*.bin`（568 个字，三种字号约 600 KB）和语言包 `dk2/zh/translation-D002-zh-CN-2.13.1.bin`，不改翻译的话直接 `trezorctl set-language` 就行。汉字点阵编进固件而不是放在语言包里，是因为语言包的字库区只有 16 位偏移、语言包空间只有 128 KB，放不下。

## 使用

- 只用 CN2（Application USB）一根线：JP1 跳线插在 **USB** 档。
- **设了 PIN 之后，开机要先在屏幕上输 PIN 解锁，USB 才会打开**，解锁前电脑看不到设备。
- 同一时间只能有一个程序连设备。装了 OneKey Bridge 的电脑，它会抢占这台设备（USB ID 相同），先退出它。
- 24 个单词：用 `trezorctl device setup --backup-type bip39 --strength 256` 创建，或在 Suite 里选。
- 第 25 个单词（密码短语）：`trezorctl set passphrase on`。

## 踩过的坑

- **刷更小的固件前要擦掉整个固件区**，否则 bootloader 校验失败报 "FIRMWARE CORRUPTED"，还会清空钱包。`dk2/flash.sh firmware` 已处理。
- **运行时别用 STM32CubeProgrammer 的 HOTPLUG 模式反复读芯片**，会把 USB/I2C 中断搞乱，看起来像触摸和 USB 失灵。
- 旋转拷贝时如果用一个跨两块显存的 MPU 区，在 bootloader 的内存布局下会和别的区重叠导致死机（lockup），所以拷贝期间临时关闭 MPU。
- Safe 7 界面默认假设有蓝牙、电源键、THP，菜单代码里有多处 `assert`/写死型号，见 `device_menu.py`。

## 下一步

外接 Infineon OPTIGA Trust M V3 安全芯片（I2C1 PG13/PG14，RST PB11）、换成自己的签名密钥、锁 RDP Level 1，才能考虑真正使用。

## 许可

与 Trezor 固件相同，GPLv3。Noto Sans SC 字体为 SIL Open Font License 1.1。
