# Better Transition · 更好的过渡
>由DeepSeek V4.1 Flash 生成

一个 KWin / Plasma 6 桌面特效（C++ 插件）：**窗口被提升（raise）时，遮挡它的窗口先渐隐、停留片刻，再以原有顺序渐显回被提升窗口的下方**；如果被提升窗口几乎被完全遮挡，或者前方挡着一个巨大窗口，则改为对**被提升窗口自己**做缩放进入 / 淡入。

| | |
| --- | --- |
| 插件 id | `bettertransition` |
| 显示名称 | 更好的过渡 / Better Transition |
| 适用 | KWin / Plasma 6.7 Wayland |
| 许可 | GPL-2.0-or-later |

> 特效**不会**修改窗口的层叠顺序，只在动画期间临时改变绘制顺序、不透明度与缩放。

---

## 效果预览

> 演示图片 / GIF 待补充：把文件放进 `docs/images/`，然后取消下面注释即可（文件名可按实际调整）。

<!--
### 普通提升：遮挡窗口渐隐 → 停留 → 在其后方渐显
![普通提升](docs/images/normal-raise.gif)

### 巨大遮挡窗口：被提升窗口在提升那一帧淡入
![巨大遮挡窗口](docs/images/large-coverer.gif)

### 几乎被完全遮挡：被提升窗口缩放进入
![缩放进入](docs/images/scale-in.gif)
-->

---

## 效果说明

对每次提升的窗口 `W`，按**先后顺序**判断（命中即停）：

1. **从最小化恢复** → 不做任何动画。
2. **几乎被完全遮挡（先判断）**：遮挡率 > `ScaleInThreshold`（默认 90%）→ 只对 `W` 做缩放进入：从 `ScaleInStartScale`（默认 90%）用 `QEasingCurve::OutCubic`（单调、无回弹/过冲）围绕中心放大到 100%。
3. **前方有一个巨大窗口**：否则若最大的遮挡窗口面积 ≥ 其所在屏幕的 `LargeCovererScreenRatio`（默认 75%）**且**比 `W` 更大 → 遮挡窗口完全不动，只对 `W` 做“停留 → 渐显”：`W` 在提升的那一帧即为最低不透明度，停留后再淡入。
4. **否则**：渐隐遮挡窗口 → 停留 → 它们在 `W` 之后渐显（保持原有相对顺序）。

- 默认：遮挡窗口渐隐 150 ms → 停留 700 ms → 渐显 250 ms；对被提升窗口自身的显隐只用“停留 700 ms → 渐显 250 ms”（**没有渐隐段**）；缩放进入 400 ms。
- 所有时长都会再乘以“系统设置 → 桌面效果 → 动画速度”里的全局倍率。

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

## 3. 启用

安装完成后可在 设置-动效-桌面特效 页面启用

---

## 卸载

一键：

```bash
./uninstall.sh
```

或者手动删除两个插件文件
```bash
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
