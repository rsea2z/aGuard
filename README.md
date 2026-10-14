# aGuard

<img src="app.ico" width="96" align="right" alt="aGuard icon">

一个 **56 KB、单文件、零依赖** 的 Windows 托盘防闲置小工具：定期检查系统键鼠空闲时间，空闲超过设定阈值时自动模拟一次 **F15 按键**，避免长时间挂机（如远程会话、长时间编译/训练观察）时因"系统无输入"而触发的各类空闲处理，例如部分 VPN / 零信任客户端（如 aTrust）的空闲注销。

防闲置的思路与 [Caffeine](https://www.zhornsoftware.co.uk/caffeine/) 一致——模拟一次几乎不被任何应用程序响应的 **F15 按键** 来重置系统空闲计时。aGuard 为从零实现的原创代码，未包含 Caffeine 的任何代码或资源（其为闭源免费软件，仅公开文档描述了该技术方案）。

> ⚠️ **使用须知**：空闲自动注销通常是单位下发的安全策略（防止离开工位后账号被冒用）。本工具不修改、不注入、不调用任何第三方客户端，仅模拟一次系统按键；但请务必先与管理员确认，在组织规定允许的前提下使用，或直接请管理员调整策略。请勿用于规避你无权规避的安全管控。本软件按 MIT 协议"按原样"提供，使用产生的一切后果由使用者自行承担。
>
> **非官方声明**：本项目为独立的开源工具，与深信服科技股份有限公司（Sangfor Technologies）无任何关联，未获其授权、认可或赞助。"Sangfor"、"深信服"、"aTrust" 是其各自权利人的商标，本文仅为说明兼容场景而提及。

---

## 它解决什么问题

许多客户端（包括部分 VPN / 零信任客户端）判断"用户是否在操作"的依据是 Windows 公开 API [`GetLastInputInfo`](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-getlastinputinfo) 返回的**系统级键鼠输入空闲时间**，而不是网络流量。因此：

- SSH 长连接、长 ping 等网络活动通常无法阻止这类"长时间未操作"的注销；
- 空闲阈值一般由管理员在服务端策略中配置，客户端本地无法修改。

aGuard 用同一个公开 API 读取空闲时间，在空闲**将要超过**你设定的阈值前模拟一次 F15，使系统空闲计时归零。

## 使用方法

1. 双击 `aGuard.exe`，托盘出现灰色盾牌图标；
2. 右键图标选择保活阈值：
   - **禁用（0）**：不做任何模拟
   - **15 / 25 / 55 分钟**：空闲超过该时间时自动保活
3. 选择会写入同目录的 `aGuard.ini`，下次启动自动生效；
4. 退出请右键 → 退出。

**阈值建议**：设为管理员策略值的 80%~95%。例如策略是 30 分钟，选 25 分钟。

### 配置文件（可选）

`aGuard.ini`（与 exe 同目录，首次修改设置后自动生成）：

```ini
[aGuard]
threshold_minutes=25   ; 空闲保活阈值（分钟），0 = 禁用；可填任意值如 10、29、59
check_seconds=60       ; 检查周期（秒），默认 60，范围 5~3600
```

### 命令行

```
aGuard.exe --print-idle    # 打印当前系统空闲秒数后退出（用于测试）
```

### 自动启动（可选）

`Win+R` 运行 `shell:startup`，把 `aGuard.exe` 的快捷方式放进去即可开机自启。

## 工作原理

```
每 check_seconds 秒:
    GetLastInputInfo(&li)
    idle = GetTickCount() - li.dwTime
    if (threshold_minutes > 0 && idle >= threshold_minutes * 60s):
        SendInput(F15 按下 + 抬起)   # 重置系统输入空闲计时
```

- F15（VK_F15, 0x7E）是扩展功能键，绝大多数应用（包括游戏）不绑定它，不会产生误操作；
- 模拟输入同时也会重置 Windows 屏保/锁屏的空闲计时，这是预期行为（不用时请选"禁用"或退出）；
- 单实例运行，重复启动会提示；Explorer 重启后托盘图标自动恢复。

## 构建编译

依赖：任意 MinGW-w64 工具链（或 MSVC）。

```bat
echo 1 ICON "app.ico" | windres -O coff -o app.res.o
gcc -O2 -s -mwindows -municode -static -o aGuard.exe aGuard.c app.res.o -lshell32
```

产物为静态链接单文件 exe，无任何运行时依赖。

MSVC 等价命令：

```
echo 1 ICON "app.ico" > app.rc && rc app.rc
cl /O2 /W4 /DUNICODE /D_UNICODE aGuard.c app.res /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib
```

图标 `app.ico` 由 `gen_icon.py`（需 Pillow）生成，一般无需重新生成。

## 致谢

- [Caffeine](https://www.zhornsoftware.co.uk/caffeine/)（Zhorn Software）——"模拟 F15 按键防闲置"这一技术方案的先行者，本项目受其启发，但为独立原创实现，未使用其任何代码。
- 图标：本项目原创绘制（`gen_icon.py` 生成，通用盾牌造型，与任何厂商图标无关），随本项目以 MIT 协议发布。

## License

[MIT](LICENSE)
