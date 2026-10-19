## 概述

这是 [media-kit](https://github.com/media-kit/media-kit) 的一个分支。

1. 来自 @0Chencc 的启发式广告屏蔽功能，在 `PlayerConfigure` 中启用可自动跳过 HLS 视频流中插入的 TS 广告片段。

2. Linux 平台捆绑预构建 libmpv2.so 以摆脱对系统 mpv 的依赖。

3. windows 平台原生支持 D3D11 渲染器，支持零拷贝硬件加速渲染，并摆脱对 ANGLE 的依赖。

4. Linux 平台使用模拟 vulkan 交换链的三重缓冲实现以尽量避免 Linux 平台糟糕的 openGL 驱动导致的黑屏或闪烁行为。

5. 合并来自 [avbuild](https://github.com/wang-bin/avbuild) 的 ffmpeg 树外补丁。可以播放原版 [media-kit](https://github.com/media-kit/media-kit) 无法播放, 但 [video_player](https://pub.dev/packages/video_player) 可以播放的非标准视频流。

6. 更新的 mpv 版本并优化二进制大小。

## 使用

需要 Dart 3.10 / Flutter 3.38 或更新版本。仅需核心包和可选的视频渲染包：

```yaml
dependencies:
  media_kit:
    git:
      url: https://github.com/Predidit/media-kit.git
      ref: main
      path: ./media_kit
  media_kit_video:
    git:
      url: https://github.com/Predidit/media-kit.git
      ref: main
      path: ./media_kit_video
```

两个包应使用相同的 Git 分支或提交；发布应用时可将 `ref` 固定到具体提交。
核心包的 Dart 构建钩子统一下载、校验、缓存和打包当前目标平台的 mpv、FFmpeg 及附属库。
音频应用只需 `media_kit`，默认使用同一套音视频二进制。

参见 [原生构建与迁移说明](media_kit/doc/native_assets.md) 和 [事件循环生命周期](media_kit/doc/native_event_loop.md)。
