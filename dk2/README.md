# Trezor 固件移植到 STM32U5G9J-DK2（Safe 7 界面 + 中文）

把 Trezor 官方固件（2.13.1，基于提交 `6f320a4`）移植到 ST 的开发板 **STM32U5G9J-DK2**：

- 5 寸 800×480 RGB 屏竖着用，跑 **Trezor Safe 7 的界面**（eckhart），带设备上的设置菜单
- GT911 电容触摸
- **简体中文界面**（944 条翻译，汉字点阵编进固件）
- 通过 USB 连接 Trezor Suite、MetaMask（Trezor Connect）等

> ⚠️ **仅供学习和测试，只放小额。** 签名用的是**你自己生成的密钥**（不再是 Trezor 公开的开发密钥），芯片可以锁到 RDP Level 1，但板子上没有安全芯片：拿到板子、又有能力做芯片毛刺攻击的人，仍有可能读出种子。

English summary: port of Trezor firmware to the STM32U5G9J-DK2 (D002 model, new board `dk2`) with the eckhart (Safe 7) UI rotated to portrait, GT911 touch and a Chinese translation whose CJK glyphs are compiled into the firmware. Signed with your own key set instead of the public development keys, and lockable to RDP Level 1 (reversible), see `dk2/keys/README.md`. No secure element — small amounts only.

## 为什么是 DK2

Trezor 官方的 D002 配置是给 **STM32U5G9J-DK1**（DSI 屏 + Sitronix 触摸）写的。本仓库把 D002 整个型号切换成了 Safe 7 界面和 RGB565，**原版 DK1 的 `-b dk` 配置在这个分支上已经不能用**。DK1 已经停产，DK2 用同一颗芯片，但屏幕（RGB 并口 RK050HR18）和触摸（GT911）都不同，原版固件刷进去只会黑屏或报 "Touch screen panel was not loaded properly"。

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
| 自己的签名密钥（替换全部开发密钥）、生成/签名/校验工具 | `dk2/keys/`、`sec/image/`、`rust/src/{translations,definitions}/`、`trezorlib/_internal/firmware_headers.py` |
| 可逆的 RDP Level 1：boardloader 不改 RDP、关 SWD 防拆、bootloader 菜单 Factory reset 自动降级 | `sec/option_bytes/stm32u5/option_bytes.c`、`sec/tamper/stm32u5/tamper.c`、`projects/bootloader/workflow/wf_bootloader.c` |

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
# 第一次：生成自己的密钥（私钥文件，放在仓库外面，离线备份）
.venv/bin/python dk2/keys/gen_keys.py ~/keys/dk2/keys.json dk2
XW_KEYS=~/keys/dk2/keys.json dk2/build.sh   # 生成 core/build/artifacts/D002/{boardloader,bootloader,firmware}.bin
.venv/bin/python dk2/keys/verify_images.py ~/keys/dk2/keys.json   # 只用公钥核对：全是我们的签名，没有开发密钥
```

没设 `XW_KEYS` 时签名工具会直接拒绝，不会悄悄用开发密钥签。

## 刷机

1. **板子设置**：BOOT0 拨片在 `0 Internal Flash`；JP1（Board Power）跳线在 `STLK` 或 `USB`。
2. **CN8（ST-LINK）接电脑。** Apple 芯片的 Mac 上 ST-LINK 直插偶尔会 USB 超时，经 Hub 更稳。
3. **打开 TrustZone**（只需一次，RDP 保持 Level 0）。注意：TZEN 打开后**不能直接关回去**，只能在 RDP 从 Level 1 回退到 Level 0 的过程中关闭，而回退会整片擦除芯片：

   ```bash
   STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob TZEN=1
   STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob SECBOOTADD0=0x180080   # 0x0C004000
   ```

4. **刷入**：`dk2/flash.sh`（整片擦除后写入三部分）。以后只更新固件用 `dk2/flash.sh firmware`（保留钱包和语言包）。
5. 第一次启动时 boardloader 会自动设置安全水位线和 HDP 隐藏区（Trezor 开发版正常行为）。

**要锁 RDP Level 1**（调试器读不出 Flash），顺序很重要，见 `dk2/keys/README.md`：只刷 boardloader + bootloader → `-ob RDP=0xBB` → 拔掉两根线断电 → 只插 CN2，在 bootloader 里经 USB 装固件 → 语言包 → 建/恢复钱包。**钱包必须在锁住之后再建**：存储的密钥经过芯片的硬件密钥，RDP 0 和 1 下不一样，锁之前建的钱包锁之后打不开。降级（会整片擦除）：bootloader 菜单 → Factory reset，或 `STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob RDP=0xAA TZEN=0`。

## 中文

```bash
cd dk2/zh
# 字体：Noto Sans SC（SIL OFL），把 NotoSansSC-Regular.otf、NotoSansSC-Medium.otf 放进 dk2/zh/fonts/
#   https://github.com/notofonts/noto-cjk/tree/main/Sans/SubsetOTF/SC
python zh_tool.py json                       # 合并翻译 -> zh.json
python gen_cjk_font.py zh.json ../../core/embed/rust/src/ui/display/cjk   # 只在改了翻译时需要
XW_KEYS=~/keys/dk2/keys.json python zh_tool.py blob   # 生成语言包（用自己的翻译密钥签名）
trezorctl set language translation-D002-zh-CN-2.13.1.bin   # 在设备上确认
```

仓库里已经带了生成好的 `cjk/*.bin`（570 个字，三种字号约 600 KB）。仓库里的语言包是用作者的密钥签的，**你的固件只认你自己签的语言包**，所以先跑一遍 `zh_tool.py blob`。汉字点阵编进固件而不是放在语言包里，是因为语言包的字库区只有 16 位偏移、语言包空间只有 128 KB，放不下。

## 使用

- 只用 CN2（Application USB）一根线：JP1 跳线插在 **USB** 档。
- **设了 PIN 之后，开机要先在屏幕上输 PIN 解锁，USB 才会打开**，解锁前电脑看不到设备。
- 同一时间只能有一个程序连设备。装了 OneKey Bridge 的电脑，它会抢占这台设备（USB ID 相同），先退出它。
- 24 个单词：用 `trezorctl device setup --backup-type bip39 --strength 256` 创建，或在 Suite 里选。
- 第 25 个单词（密码短语）：`trezorctl set passphrase on`（Safe 7 的设备菜单里本来就没有这个开关）。
- **Trezor Suite**：固件不是官方的，"固件真实性检查" 永远不过，新版 Suite 会把收款地址的复制/验证按钮锁成灰色。在 Suite 设置 → 设备 → 高级设置里关掉 "固件真实性检查"。
- bootloader 界面要先点 **Initiate connection**，电脑才能看到它。

## 安全审查

用两个独立的审查代理（红队找漏洞、蓝队查正确性）审过一遍改动，没有发现 PIN 绕过、种子泄露或内存越界；修复了：自动锁定后黑屏卡死、菜单 "关机" 不关机且保持解锁、点屏幕黑边误触按钮、触摸读取失败被当成松手、U+FFFF 以上字符被显示成常见汉字、旋转拷贝期间整体关闭 MPU（改为单独映射目标显存）、拷贝前未等 DMA2D 完成、无蓝牙板子上的蓝牙菜单、中文标点断行、BOR 阈值（DK2 是 3.3 V）。

## 踩过的坑

- **刷更小的固件前要擦掉整个固件区**，否则 bootloader 校验失败报 "FIRMWARE CORRUPTED"，还会清空钱包。`dk2/flash.sh firmware` 已处理。
- **运行时别用 STM32CubeProgrammer 的 HOTPLUG 模式反复读芯片**，会把 USB/I2C 中断搞乱，看起来像触摸和 USB 失灵。
- 旋转拷贝时如果用一个跨两块显存的 MPU 区，在 bootloader 的内存布局下会和别的区重叠导致死机（lockup），所以加了一个单独的 MPU 模式 `MPU_MODE_FB_COPY`，拷贝期间只多映射目标显存。
- Safe 7 界面默认假设有蓝牙、电源键、THP，菜单代码里有多处 `assert`/写死型号，见 `device_menu.py`。
- **原版 PRODUCTION 编译第一次开机就把芯片永久锁成 RDP Level 2**，还会每次开机拿仓库里签名无效的 bootloader 去 "升级"。本分支直接禁止 PRODUCTION 编译。
- 原版的代币/网络定义（definitions）也认开发密钥，谁都能伪造代币信息；本分支一并换成自己的密钥。
- 用调试器锁完 RDP 1 后，必须**拔掉 CN8 和 CN2 彻底断电**一次，否则芯片认为调试器还在、禁止访问 Flash（黑屏），这时开机还会写坏钱包存储。
- 从 RDP 1 降级后 TZEN 会变回 0，刷机前要再 `-ob TZEN=1`；SECBOOTADD0 / BOOT_LOCK 会保留。

## 下一步

外接 Infineon OPTIGA Trust M V3 安全芯片（I2C1 PG13/PG14，RST PB11），PIN 次数由安全芯片限制，才能考虑放更多钱。

## 许可

与 Trezor 固件相同，GPLv3。Noto Sans SC 字体为 SIL Open Font License 1.1。
