# Better Transition · 更好的过渡

一个 KWin / Plasma 6 桌面特效（C++ 插件）：**窗口被提升（raise）时，遮挡它的窗口先渐隐、停留片刻，再以原有顺序渐显回被提升窗口的下方**；如果被提升窗口几乎被完全遮挡，或者前方挡着一个巨大窗口，则改为对**被提升窗口自己**做缩放进入 / 淡入。

| | |
| --- | --- |
| 插件 id | `bettertransition` |
| 显示名称 | 更好的过渡 / Better Transition |
| 适用 | KWin / Plasma **6.7+**（Wayland、X11 均可，X11 未专门测试） |
| 许可 | GPL-2.0-or-later |

> 特效**不会**修改窗口的层叠顺序，只在动画期间临时改变绘制顺序、不透明度与缩放。

---

## 效果展示

> 演示图片 / GIF 待补充：把文件放进 `docs/images/`，然后取消下面注释即可（文件名可按实际调整）。

<!--
### 普通提升：遮挡窗口渐隐 → 停留 → 在其后方渐显
![普通提升](docs/images/normal-raise.gif)

### 巨大遮挡窗口：被提升窗口在提升那一帧淡入
![巨大遮挡窗口](docs/images/large-coverer.gif)

### 几乎被完全遮挡：被提升窗口缩放进入
![缩放进入](docs/images/scale-in.gif)
-->

| 场景 | 预览 |
| --- | --- |
| 普通提升：遮挡窗口渐隐 → 停留 → 在其后方渐显 | 📷 待补充 `docs/images/normal-raise.gif` |
| 巨大遮挡窗口（占屏 ≥75% 且比 W 大）→ 对 W 做“停留 + 渐显” | 📷 待补充 `docs/images/large-coverer.gif` |
| 几乎被完全遮挡（遮挡率 >90%）→ 对 W 做缩放进入 | 📷 待补充 `docs/images/scale-in.gif` |

---

## 效果说明

对每次提升的窗口 `W`，按**先后顺序**判断（命中即停）：

1. **从最小化恢复** → 不做任何动画。
2. **几乎被完全遮挡（先判断）**：遮挡率 > `ScaleInThreshold`（默认 90%）→ 只对 `W` 做缩放进入：从 `ScaleInStartScale`（默认 90%）用 `QEasingCurve::OutCubic`（单调、无回弹/过冲）围绕中心放大到 100%。
3. **前方有一个巨大窗口**：否则若最大的遮挡窗口面积 ≥ 其所在屏幕的 `LargeCovererScreenRatio`（默认 75%）**且**比 `W` 更大 → 遮挡窗口完全不动，只对 `W` 做“停留 → 渐显”：`W` 在提升的那一帧即为最低不透明度，停留后再淡入。
4. **否则**：渐隐遮挡窗口 → 停留 → 它们在 `W` 之后渐显（保持原有相对顺序）。

```
时序（W = 被提升的窗口，A/B = 原本遮挡 W 的窗口）

  普通情况：A 渐隐 → 停留 → A 在 W 之后渐显
  提升前            fade-out            hold（停留）        fade-in
  ┌──────┐          ┌──────┐            ┌──────┐           ┌──────┐
  │  A   │ 遮挡 W    │  A   │  半透明     │  A   │  全透明    │  W   │  A 在 W 之后渐显
  ├──────┤          ├──────┤            ├──────┤           ├──────┤
  │  W   │          │  W   │  逐渐可见    │  W   │            │  A   │
  └──────┘          └──────┘            └──────┘           └──────┘

  几乎被完全遮挡（>90%）：W 从 90% 平滑放大到 100%，遮挡窗口不动
  ┌────┐   ┌─────┐   ┌──────┐   ┌──────┐
  │ W  │ → │  W  │ → │  W   │ → │  W   │
  └────┘   └─────┘   └──────┘   └──────┘

  巨大遮挡窗口（占屏 ≥75% 且比 W 大）：遮挡窗口不动，W 提升那一帧即为最低不透明度
   提升第 0 帧        Hold 停留           FadeIn 渐显
  ┌──────┐          ┌──────┐           ┌──────┐
  │  ·   │          │  ·   │           │  W   │
  └──────┘          └──────┘           └──────┘
    不可见             不可见            淡入到不透明
```

- 默认：遮挡窗口渐隐 150 ms → 停留 700 ms → 渐显 250 ms；对被提升窗口自身的显隐只用“停留 700 ms → 渐显 250 ms”（**没有渐隐段**）；缩放进入 400 ms。
- 所有时长都会再乘以“系统设置 → 桌面效果 → 动画速度”里的全局倍率。

### 行为细节

1. 监听 `EffectsHandler::stackingOrderChanged`，对比新旧层叠顺序，找出本次**向上移动**且位于最上层的那个窗口 `W`（刚打开的窗口不算，它们不在旧顺序里）。
2. **若 `W` 此前处于最小化状态**（即这次提升只是“从最小化恢复”），则不做任何动画，直接更新快照返回。判定方式：为每个窗口连接 `EffectWindow::minimizedChanged`，窗口最小化时记入集合；该窗口被提升时若命中集合则消费并跳过——最小化/恢复信号先于重新叠放发生，因此判定可靠。
3. 计算**遮挡窗口**集合：在**旧**顺序中位于 `W` 之上、可见、位于同一桌面/活动、且与 `W` 几何相交的普通窗口（模态窗口按主窗口合并几何）。
4. 计算**遮挡率** = 这些窗口与 `W` 的矩形交集之**并集**面积 ÷ `W` 面积；用 `QRegion` 求并集，重叠的遮挡窗口不会重复计算。
5. **缩放进入分支（优先）**：若遮挡率 > `ScaleInThreshold`（默认 90%），只对 `W` 做缩放进入——从 `ScaleInStartScale`（默认 90%）用 `QEasingCurve::OutCubic`（单调、无回弹/过冲）放大到 100%，围绕窗口中心（与 KWin `AnimationEffect` 的居中锚点一致）。
6. **巨大遮挡窗口分支**：否则取遮挡窗口中 `frameGeometry` 面积最大者，若其面积 ≥ 其所在 `LogicalOutput` 屏幕几何的 `LargeCovererScreenRatio`（默认 75%）**且**其面积 > `W` 面积，则只对 `W` 做“停留 → 渐显”（复用 `HoldDuration`/`FadeInDuration` 与 `FadeStrength`），遮挡窗口完全不动。
7. **渐隐遮挡窗口分支**：其余情况只保留在**新**顺序中落到 `W` 之下的遮挡窗口（`keep-above` 等仍停留在 `W` 之上的无法“渐显在 `W` 之后”，会被排除）。把它们按“由下到上”的原始顺序用 `effects->setElevatedWindow(true)` 临时提升到最上层——否则窗口提升后它们已经在 `W` 后面，渐隐就看不见了。渐隐、停留阶段保持临时提升；进入渐显阶段前解除临时提升，于是它们在 `W` **后面**渐显，且相互之间保持原有顺序。
8. 动画都在 `EffectWindow::paintWindow` 中修改 `WindowPaintData`：渐隐用 `multiplyOpacity()`，缩放进入用居中缩放（`translate` + `setXScale/setYScale`）；并在 `prePaintWindow` 中分别 `setTranslucent()` / `setTransformed()`，让合成器重绘受影响的区域。

### 为什么“对被提升窗口的显隐”没有渐隐段

特效只能在 `stackingOrderChanged` 里得知提升，而该信号发出时提升**已经提交**，下一帧 `W` 就以 100% 不透明度画在最上层；若此时再播放渐隐，就会先看到“跳到最前”然后才开始动画。KWin 没有向特效暴露“即将提升”的钩子，所以在特效层能做的就是让 `W` 在提升那一帧直接处于最低不透明度——跳变被“不可见”吞掉，动画本身就是提升的显现过程。想要它更快出现，把 `HoldDuration` 调小即可（该分支不使用 `FadeOutDuration`）。

---

## 安装

### 1. 依赖

Debian / Ubuntu：

```bash
sudo apt install cmake g++ extra-cmake-modules kwin-dev \
    qt6-base-dev qt6-base-dev-tools libkf6config-dev libkf6configwidgets-dev \
    libkf6coreaddons-dev libkf6i18n-dev libkf6kcmutils-dev
```

其它发行版安装对应的 `kwin-dev`（KWin 头文件）、Qt 6 Base、KDE Frameworks 6（Config / ConfigWidgets / CoreAddons / I18n / KCMUtils）与 `extra-cmake-modules` 即可。

### 2. 构建并安装

一键：

```bash
./install.sh            # 构建 + 安装到 /usr
./install.sh --enable   # 安装后顺便写入 kwinrc 并让 KWin 立即加载（有风险，见下）
```

手动：

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

安装后的文件（与系统自带特效、其它第三方特效放在一起）：

```
/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/bettertransition.so
/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/configs/kwin_bettertransition_config.so
```

> Debian/KDE 会采用 Qt 的插件路径方案，因此目录是 `.../qt6/plugins/kwin/...`，
> 而不是 `/usr/lib/x86_64-linux-gnu/kwin/...`。其它发行版可能是
> `/usr/lib/qt6/plugins/kwin/...` 或 `/usr/lib64/qt6/plugins/kwin/...`，`uninstall.sh` 里都做了兜底。

### 3. 启用

新装的插件需要让 KWin 重新枚举特效：

```bash
# 1) 在 kwinrc 中打开（也可在“系统设置 → 桌面效果”里搜索“更好的过渡 / Better Transition”勾选）
kwriteconfig6 --file kwinrc --group Plugins --key bettertransitionEnabled true

# 2) 让正在运行的 KWin 重新加载特效列表
qdbus6 org.kde.KWin /KWin reconfigure
```

如果列表没刷新，注销 / 重新登录，或重启 `kwin_wayland` / `kwin_x11`。
也可以在“桌面效果”里直接勾选后点“应用”。

> `install.sh --enable` 会在**正在运行的合成器**里加载新插件；若插件有问题可能导致 KWin 崩溃，
> 请先保存工作。稳妥做法是只执行 `./install.sh`，注销重新登录后再启用。

---

## 卸载

一键：

```bash
./uninstall.sh
```

或者分步执行：

```bash
# 1) 在 kwinrc 中禁用，并让 KWin 重新加载特效列表
kwriteconfig6 --file kwinrc --group Plugins --key bettertransitionEnabled false
qdbus6 org.kde.KWin /KWin reconfigure

# 2a) 用构建目录里的 uninstall 目标删除已安装文件（推荐，基于 install_manifest.txt）
sudo cmake --build build --target uninstall

# 2b) 或者手动删除两个插件文件
sudo rm -f /usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/bettertransition.so
sudo rm -f /usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/configs/kwin_bettertransition_config.so
```

可选：清除配置组 `[Effect-bettertransition]`（不清理也不影响使用）：

```bash
for k in FadeOutDuration HoldDuration FadeInDuration FadeStrength \
         OnlyOverlapping IncludeSpecialWindows LargeCovererScreenRatio \
         ScaleInThreshold ScaleInDuration ScaleInStartScale; do
    kwriteconfig6 --file kwinrc --group Effect-bettertransition --key "$k" --delete
done
```

卸载后如果“桌面效果”列表里仍有残留条目，注销 / 重新登录即可。

---

## 配置文件介绍

配置保存在 `~/.config/kwinrc` 的 `[Effect-bettertransition]` 组里；图形界面在
“系统设置 → 桌面效果 → 更好的过渡”。设置页与运行中的特效共用同一份 KConfigXT 骨架类
（`src/bettertransition.kcfg`），所以两边始终一致。

```ini
[Effect-bettertransition]
FadeOutDuration=150
HoldDuration=700
FadeInDuration=250
FadeStrength=100
OnlyOverlapping=true
IncludeSpecialWindows=false
LargeCovererScreenRatio=75
ScaleInThreshold=90
ScaleInDuration=400
ScaleInStartScale=90
```

| 键 | 类型 | 默认 | 说明 |
| --- | --- | --- | --- |
| `FadeOutDuration` | int (ms) | `150` | 遮挡窗口的渐隐时长。**不用于**对被提升窗口自身的显隐分支。 |
| `HoldDuration` | int (ms) | `700` | 最低不透明度的停留时长；遮挡窗口分支与“对被提升窗口显隐”分支都会用。调小可让被提升窗口更快出现。 |
| `FadeInDuration` | int (ms) | `250` | 渐显时长。 |
| `FadeStrength` | int (%) | `100` | 渐隐强度，`100` = 完全透明。目标不透明度 = `1 - FadeStrength/100`，被提升窗口自身的显隐也用它。 |
| `OnlyOverlapping` | bool | `true` | 只处理与提升窗口几何相交的遮挡窗口；关掉后“位于其上方”的窗口都会参与（是否相交只影响渐隐分支的入选）。 |
| `IncludeSpecialWindows` | bool | `false` | 是否也处理弹出菜单、通知、工具提示等特殊窗口；面板（dock）、桌面、锁屏等**始终不处理**。 |
| `LargeCovererScreenRatio` | int (%) | `75` | 最大遮挡窗口面积 ≥ 其所在屏幕的该百分比**且**大于被提升窗口时，改为对被提升窗口做“停留 → 渐显”。`100` = 关闭该规则。 |
| `ScaleInThreshold` | int (%) | `90` | 被提升窗口被遮挡面积超过该百分比时，改为对它做缩放进入。`100` = 关闭缩放进入。 |
| `ScaleInDuration` | int (ms) | `400` | 缩放进入时长。 |
| `ScaleInStartScale` | int (%) | `90` | 缩放进入的起始缩放百分比，放大到 `100`。 |

另外，特效是否启用由 `[Plugins]` 组里的 `bettertransitionEnabled` 控制（由“桌面效果”界面写入）：

```ini
[Plugins]
bettertransitionEnabled=true
```

改完配置后可用 DBus 让特效立即重新读取（“桌面效果”里点“应用”等价于这一步）：

```bash
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect bettertransition
```

### 调试

```bash
# 查看当前已加载 / 可用特效
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadedEffects

# 动态加载 / 卸载（不需要重启 KWin）
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect bettertransition
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect bettertransition
```

---

## 系统要求

| 组件 | 版本 |
| --- | --- |
| KWin | **6.7 及以上**（使用了 6.7 的 `prePaintScreen(ScreenPrePaintData &)` 与 `prePaintWindow(RenderView *, …)` 绘制 API） |
| Qt | 6.6+ |
| KDE Frameworks | 6.6+（需要 ECM ≥ 5.240） |
| 合成后端 | 需要开启合成/动画（Wayland 与 X11 均可，X11 未做专门测试） |

本项目在以下环境实际编译通过：

- Debian forky/sid，KDE Plasma 6.7.4（`kwin-dev` 4:6.7.4）
- KDE Frameworks 6.28.0，Qt 6.10.2，GCC 16.2.0，CMake 4.3.4

---

## 已知限制

- 触发条件是“窗口在层叠顺序中向上移动”。把最上层窗口“下移”（Lower）在层叠顺序上等价于其余窗口上移，因此也会触发一次动画（表现为被下移的窗口渐隐后回到新的顶层窗口之后）——这与 KWin 自带 Slide Back 特效的处理方式一致。
- `FadeStrength < 100` 时，遮挡窗口并非完全透明；在解除临时提升、转入渐显的那一刻，它们与提升窗口重叠的区域会有一次轻微跳变。想要完全平滑请保持默认的 `100`。
- 遮挡率按“提升前覆盖它的窗口与它的并集交集面积 / 窗口面积”计算，是矩形级近似（不做逐像素/形状计算），且只统计同一桌面/活动上的普通窗口。
- 巨大遮挡窗口判定同样基于矩形面积：只取面积最大的**单个**遮挡窗口，与其所在输出（`LogicalOutput`）的几何比较；多屏时按各自所在屏判断。
- 对 `W` 自身的显隐从“提升那一帧即最低不透明度”开始，所以只有停留 + 渐显、没有渐隐；如果希望它更快出现，把 `HoldDuration` 调小即可。
- 新打开的窗口（首次 map）不会触发；从最小化恢复引起的提升也不会触发。
- 全局“动画速度”设为“即时”时时长会被压缩到 1 ms，等效于关闭该特效。
- 特效只在检测到提升的瞬间决定一次遮挡集合与分支；动画途中新出现的窗口不参与本次动画。
- 与其它会设置 `setElevatedWindow` 的特效（例如 Slide Back）同时使用可能互相覆盖临时提升状态，属罕见组合；与其它同时缩放窗口的特效叠加缩放也会互相影响。

---

## 目录结构

```
CMakeLists.txt                     顶层构建脚本
build.sh / install.sh / uninstall.sh   便捷脚本
docs/images/                       效果演示图片（待补充）
src/
  CMakeLists.txt                   特效插件
  main.cpp                         KWIN_EFFECT_FACTORY_SUPPORTED 入口
  bettertransition.h / bettertransition.cpp   特效实现
  bettertransition.kcfg            KConfigXT 配置定义
  bettertransitionconfig.kcfgc     生成 KWin::BetterTransitionConfig
  metadata.json                    插件元数据（名称/描述/配置模块）
  kcm/
    CMakeLists.txt                 KCM 配置模块
    bettertransition_config.{h,cpp,ui}  系统设置里的配置页
```

## 许可

GPL-2.0-or-later，见 `LICENSE`。
