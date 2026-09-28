# world.execute (me); — 终端 ASCII 动画 MV

编译一个 C 程序，运行它，终端里就会播放 Mili《world.execute (me);》的 ASCII 艺术 MV。
每一句歌词都有对应的画面，画面之间用溶解、扫描线、故障、闪白等十几种转场过渡，
底部同步显示卡拉OK歌词，关键词还会放大成巨型字体。

最终产物是**一个可执行程序**，不是视频文件——所有画面都是程序实时算出来的
（每个场景都是播放时间的纯函数），mp3 只是顺带播放，找不到播放器时静默继续。

## 快速开始

```sh
make && ./start
```

音频与歌词**与可执行文件放在同一层**：

```
mili-mv/
├── start                        ← 可执行文件（make 生成）
├── world.execute (me) ;.mp3     ← 音频（自备，不随仓库发布）
├── world.execute (me) ;.lrc     ← 歌词（自备，不随仓库发布）
├── world.execute (me) ;.yrc     ← 歌词（自备，不随仓库发布）
├── Makefile
├── src/                         ← 源码
├── tools/                       ← 辅助脚本
└── preview/                     ← 参考静帧与 GIF
```

依赖：C11 编译器、make、POSIX 终端，外加（可选）命令行 mp3 播放器。

仓库不附带音频与 `.lrc`/`.yrc`（歌词时间轴已烘焙进 `src/lyrics_data.h`，所以 clone 后可直接 `make`）。

程序按「可执行文件所在目录 → 当前目录 → 两者各自的上级目录」的顺序找 mp3，所以 `start`
旁边这首一定优先命中，不需要任何参数；整个目录可以直接拷走。仓库不附带音频文件，
请自备同名 mp3 放在这里（或 `--audio PATH` 指定、`MV_MP3` 环境变量指定）。

没有 tty 的环境（如 CI）也能用，只是看不到动画：

```sh
./start --list         # 打印 49 个场景的时间表
./start --dump 68000   # 把第 68 秒那一帧按纯文本打印出来
```

## 功能特性

- **49 个场景**，每个锚定到歌词时间轴，画面跟着歌词走
- **十几种转场**：溶解、扫描线、故障、闪白、百叶窗、燃烧等
- **逐词卡拉OK**歌词同步
- **关键词大字**自动放大
- **终端自适应**：窗口大小变化时自动重排
- **多色域支持**：truecolor / 256 / 16 / mono 自动降级
- **离线导出**：任意帧可导出为文本、PNG、GIF

## 快捷键与终端行为

| 操作 | 说明 |
| --- | --- |
| `Ctrl-C` | 退出，恢复终端原样（光标、备用屏幕、颜色） |
| 拖动终端窗口 | 收到 `SIGWINCH` 后重排并重绘 |

程序使用终端备用屏幕（alternate screen），退出后原 shell 内容还在。

## 命令行参数

```
--no-audio          默片模式（网络终端、无声卡时最稳）
--player NAME       指定播放器：ffplay | mpg123 | mpv | cvlc | afplay
--audio PATH        指定 mp3 路径（默认自动查找）
--offset MS         整体画面提前/延后毫秒，对齐声音
--start TIME        从某处开始：--start 1:23 / --start 83s / --start 83000
--fps N             帧率，默认 30
--size WxH          强制画布大小（默认问终端）
--color MODE        auto | truecolor | 256 | 16 | mono（默认 auto）
--contrast N        明暗对比曲线强度 0..1，默认 0.75
--bg RRGGBB         画布底色，默认 030409；--bg default 透出终端背景
--unicode           大标题改用方块字符（默认纯 ASCII）
--no-karaoke        不要底部歌词
--no-banner         不要放大的关键词
--no-hud            不要顶栏和底栏
--no-grain          关掉胶片颗粒
--loop              循环播放
-q, --quiet         少说话
```

### 调整对比度

画面为深色底设计，程序自动铺近黑底色。如果主体融进背景：

```sh
./start --contrast 0.9        # 更狠的明暗分离
./start --contrast 0          # 关掉曲线，看"原味"
./start --bg 000000           # 纯黑底
./start --bg default          # 终端自己的背景色
```

`--contrast` 把阴影推向黑、高光推向纯亮，只压暗稀疏字符（`.` `:` `-`）组成的中间调纹理，
密实字符（`#` `%` `@`）画的主体不动。

## 离线出图 / 出 GIF

程序可以把任意时刻的一帧导出成 `.mvf` 文本帧（每单元格一个字符 + 前景色），
配套 Python 工具按 8x16 点阵字体渲染成 PNG，再用 ImageMagick 合成 GIF：

```sh
./start --export preview/frames --export-from 0 --export-to 30000 --export-step 100 --size 80x24
python3 tools/mvpreview.py --no-export --frames preview/frames --out preview/png \
        --scale 2 --gif preview/opening.gif --sheet preview/sheet.png
make preview        # 等价的一步到位命令
```

只需 Python 3 标准库；GIF/拼图步骤调用 `magick`/`convert`（没有就跳过）。

## 画面与歌词的对齐原理

- `.yrc` 提供逐词时间（毫秒级），`.lrc` 提供整行文本，`tools/gen_lyrics.py` 合并成
  `src/lyrics_data.h`（129 行 / 422 个词，42 个关键词做大字）。
- 场景表 `src/scenes.c` 中每个场景锚定到某一行歌词的下标，时间轴天然连续，不会漂移。
- 所有特效都是播放时间的纯函数：同一时刻一定画出同一帧。转场不是硬切——进入转场窗口
  后前后两个场景都渲染，再按权重图混合，并在转场中点压一下亮度。

## 代码结构

```
src/main.c        CLI、主循环、离线模式（导出/打印/列表/自检）
src/term.c/.h     原始模式、备用屏幕、SIGWINCH、逐单元 diff 输出、导出
src/gfx.c         单元格帧缓冲 + 图元（线/圆/椭圆/精灵/横幅大字/颗粒/故障/转场混合）
src/scenes.c      49 个场景时间表、解析、特效分发、转场合成
src/fx_world.c    前半段特效 1-22（开机、护盾、积木、数据流、星球、代码、点、圆、正弦…）
src/fx_love.c     后半段特效 23-44（猫、神、性别、形变、钟、锁链、恍惚、执行风暴…）
src/lyrics.c      歌词双行显示、逐词卡拉OK、关键词大字、HUD
src/audio.c       找 mp3、挑播放器、fork/exec、退出时清理
src/sprites.c     18 个手工 ASCII 精灵（茄子、番茄、猫、神、心、锁链、警告…）
src/font_data.h   烘焙好的 8x16 点阵字体（大字和 PNG 预览共用）
src/lyrics_data.h 烘焙好的歌词时间轴
tools/gen_font.py    从系统 TTF 生成 font_data.h
tools/gen_lyrics.py  从 .lrc/.yrc 生成 lyrics_data.h
tools/mvpreview.py   .mvf -> PNG/GIF
tools/ptycheck.py    在伪终端里跑一遍，检查终端是否被正确恢复
```

## 重新生成素材

仓库已带好 `font_data.h` 和 `lyrics_data.h`，平时不需要动：

```sh
make font     # 需要 ImageMagick + Liberation Mono（或改 tools/gen_font.py 字体路径）
make data     # 从同层的 .lrc / .yrc 生成（这两个文件需自备）
```

`make data` 在 LRC 与 YRC 时间差超过 300 ms 时给出提示，并以 YRC 为准。

## 已知取舍

- 音频同步是"同时启动"级别：程序用单调时钟推进画面，先把播放器拉起来（默认等
  180 ms）再开始计时。若想更精确，用 `--offset` 微调。
- 终端单元格是 8x16 像素（宽高比 1:2），所有圆形按 `ry = rx * 0.5` 修正，
  大字把点阵字体采样到正方形单元格块里，保证字母不被拉长。
- 颜色按 `COLORTERM`/`TERM` 自动降级到 256/16/单色；`--color mono` 只靠字符密度表现明暗。

祝你播放愉快。
