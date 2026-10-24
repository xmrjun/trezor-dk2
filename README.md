# Trezor 固件 · STM32U5G9J-DK2 移植版（Safe 7 界面 + 中文）

把 Trezor 官方开源固件（2.13.1）移植到 ST 开发板 **STM32U5G9J-DK2**：Safe 7 界面竖屏显示、GT911 触摸、全中文界面，可通过 Trezor Connect 连接 MetaMask 等钱包。

👉 **复现步骤、改动说明、踩坑记录见 [dk2/README.md](dk2/README.md)**

> ⚠️ 仅供学习测试：使用 Trezor 公开开发密钥、芯片未锁（RDP Level 0）、没有安全芯片，**不要放真钱**。

Port of the Trezor firmware to the STM32U5G9J-DK2 (eckhart UI in portrait, GT911 touch, Chinese UI) — see [dk2/README.md](dk2/README.md). For testing only. Original README below.

---

# Trezor Firmware

![img](https://repository-images.githubusercontent.com/180590388/968e6880-6538-11e9-9da6-4aef78157e94)

## Repository Structure

* **[`ci`](ci/)**: Ancillary files, data, and scripts for the CI pipeline
* **[`common/defs`](common/defs/)**: JSON coin definitions and support tables
* **[`common/protob`](common/protob/)**: Common protobuf definitions for the Trezor protocol
* **[`common/tools`](common/tools/)**: Tools for managing coin definitions and related data
* **[`core`](core/)**: Trezor Core, a firmware implementation for Trezor T, Trezor Safe 3, Trezor Safe 5, and Trezor Safe 7
* **[`crypto`](crypto/)**: Stand-alone cryptography library used by both Trezor Core and the Trezor One firmware
* **[`docs`](docs/)**: Assorted documentation
* **[`legacy`](legacy/)**: Trezor One firmware implementation
* **[`python`](python/)**: Python [client library](https://pypi.org/project/trezor) and the `trezorctl` command
* **[`storage`](storage/)**: NORCOW storage implementation used by both Trezor Core and the Trezor One firmware
* **[`tests`](tests/)**: Firmware unit test suite
* **[`tools`](tools/)**: Miscellaneous build and helper scripts
* **[`vendor`](vendor/)**: Submodules for external dependencies

## Contribute

See [CONTRIBUTING.md](docs/misc/contributing.md).

Using [Conventional Commits](COMMITS.md) is strongly recommended and might be enforced in the future.

Also, please have a look at the docs, either in the `docs` folder or at [docs.trezor.io](https://docs.trezor.io) before contributing. The [misc](docs/misc/index.md) chapter should be read in particular, as it contains useful assorted knowledge.

## Security Vulnerability Disclosure

Please do NOT create publicly viewable issues for suspected security vulnerabilities. See the [Security tab](https://github.com/trezor/trezor-firmware/security/policy) for reporting instructions.

## Documentation

See the `docs` folder or visit [docs.trezor.io](https://docs.trezor.io).
