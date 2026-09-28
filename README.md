# world.execute (me); — 终端 ASCII 动画 MV

把一个 C 程序编译出来，运行它，就会在你的终端里播放 Mili《world.execute (me);》的
ASCII 艺术 MV：每一句歌词都有对应的画面，画面之间用溶解、扫描线、故障、闪白等十几种
转场过渡，底部同步显示卡拉OK歌词，画面上还会把歌词里的关键词放大成巨型字体。

最终产物是**一个可执行程序**，不是视频文件——所有画面都是程序实时算出来的（每个场景
都是播放时间的纯函数），mp3 只是顺带播放，找不到播放器时它会静默继续跑。

```
$ make && ./start
```

```
 world.execute (me);            beat  . . @ .                              1:08
                                     =======
                   .             ^^^^^    ^^^^^=
                                ^=    ^  ^     ^= .
   .                           ==               ==
                              ==                 ==
                              =                   =
                              ==                 ==
                               ==   ~~~~v~~~~   ==
      .                         ==~~         ~~==
                                  ===      =====
                                     =======
   happiness 100% .             --[ RUN ]-------
                                  SATISFACTION
                    .      .If I can make you happy
 1:08   [=============================================---------------]     3:32
```

## 1. 编译与运行

依赖很少：一个 C11 编译器、make、POSIX 终端，外加（可选）一个命令行 mp3 播放器。

```sh
make                # 生成 ./start
make check          # 自检：时间轴是否连续、44 个特效能否在任意尺寸下渲染
./start                # 播放（自动找歌、自动找播放器；找不到就当默片）
make run            # 同上
```

没有 tty 的环境（比如 CI）也能用，只是看不到动画：

```sh
./start --list         # 打印 49 个场景的时间表
./start --dump 68000   # 把第 68 秒那一帧按纯文本打印出来
```

## 2. 快捷键

| 操作 | 说明 |
| --- | --- |
| `Ctrl-C` | 退出，并且一定会把终端恢复原样（光标、备用屏幕、颜色） |
| 直接拖动终端窗口大小 | 程序收到 `SIGWINCH` 后会重排画面并重绘 |

程序使用终端的备用屏幕（alternate screen）：退出后你原来的 shell 内容还在。

## 3. 常用参数

```
--no-audio          默片模式（网络终端、没有声卡时最稳）
--player NAME       指定播放器：ffplay | mpg123 | mpv | cvlc | afplay
--audio PATH        指定 mp3 路径（默认在可执行文件附近自动找）
--offset MS         整体画面提前/延后若干毫秒，用来对齐你听到的声音
--start TIME        从某处开始：--start 1:23 / --start 83s / --start 83000
--fps N             帧率，默认 30
--size WxH          强制画布大小（默认问终端）
--color MODE        auto | truecolor | 256 | 16 | mono（默认 auto）
--contrast N        明暗对比曲线强度 0..1，默认 0.75；嫌灰就调大，嫌刺眼调小
--bg RRGGBB         画布底色，默认 030409（近黑）；--bg default 则透出终端自己的背景
--unicode           大标题改用方块字符，边缘更实（默认纯 ASCII）
--no-karaoke        不要底部歌词
--no-banner         不要放大的关键词
--no-hud            不要顶栏（标题、节拍灯）和底栏（时钟、进度）
--no-grain          关掉胶片颗粒
--loop              循环播放
-q, --quiet         少说话
```

例：在窄终端里省点事，并把手感对齐耳机：

```sh
./start --no-audio --color 256 --size 80x24
./start --start 1:03 --offset -80
```

### 看着不够清楚时

画面是为**深色底**设计的，程序会自己铺一层近黑底色，所以浅色主题的终端也不会糊。
如果还是觉得主体融进背景：

```sh
./start --contrast 0.9        # 更狠的明暗分离：背景压下去，主体顶上来
./start --contrast 0          # 关掉曲线，看看"原味"有多灰
./start --bg 000000           # 换纯黑底
./start --bg 101018           # 稍亮一点的底板
./start --bg default          # 用终端自己的背景色
```

`--contrast` 做三件事：把阴影推向黑、把高光推向纯亮，并且**只压暗由稀疏字符
（`.` `:` `-`）组成的中间调纹理，密实字符（`#` `%` `@`）画的主体不动**——这是
ASCII 里"纹理"和"结构"的区别。程序里的精灵另有一层椭圆"聚光"暗底，主体周围
的噪点会更暗，所以主体能立起来。

## 4. 离线出图 / 出 GIF

程序可以把任意时刻“本来会画出来”的一帧导出成极小的文本帧（`.mvf`：每个单元格一个
字符 + 一个前景色），配套的 Python 工具会把它按**同一个** 8x16 点阵字体渲染成 PNG，
再用 ImageMagick 合成 GIF：

```sh
./start --export preview/frames --export-from 0 --export-to 30000 --export-step 100 --size 80x24
python3 tools/mvpreview.py --no-export --frames preview/frames --out preview/png \
        --scale 2 --gif preview/opening.gif --sheet preview/sheet.png
make preview        # 等价的一步到位命令
```

只需要 Python 3 标准库；GIF/拼图那一步会调用 `magick`/`convert`（没有就跳过）。

## 5. 画面是怎么和歌词对齐的

* `world.execute (me) ;.yrc` 提供逐词时间（毫秒级），`world.execute (me) ;.lrc` 提供干净
  的整行文本；`tools/gen_lyrics.py` 把两者合并成 `src/lyrics_data.h`
  （129 行 / 422 个词，其中 42 个词会被当成关键词做大字）。
* 场景表 `src/scenes.c` 里每个场景都**锚定到某一行歌词的下标**（可加一个前导提前量），
  于是“画面跟着歌词走”是结构上的保证；而且第 N 个场景的结束时间就是第 N+1 个场景的开始
  时间，时间轴天然连续，不会漂移，也不需要手抄毫秒数。
* 所有特效都是**播放时间的纯函数**：同一时刻一定画出同一帧。转场不是硬切——进入转场窗口
  后会把前后两个场景都渲染出来，再按权重图混合（溶解 / 左右上下擦除 / 圆形开合 / 故障 /
  闪白闪黑 / 百叶窗 / 燃烧），并在转场中点压一下亮度，观感更连贯。

## 6. 代码结构

```
src/main.c        CLI、主循环、离线模式（导出/打印/列表/自检）
src/term.c/.h     原始模式、备用屏幕、SIGWINCH、逐单元 diff 输出、导出
src/gfx.c         单元格帧缓冲 + 图元（线/圆/椭圆/精灵/横幅大字/颗粒/故障/转场混合）
src/scenes.c      49 个场景的时间表、场景解析、特效分发、转场合成
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

## 7. 重新生成素材

仓库里已经带好了 `font_data.h` 和 `lyrics_data.h`，平时不需要动。要改的话：

```sh
make font     # 需要 ImageMagick + Liberation Mono（或改 tools/gen_font.py 里的字体路径）
make data     # 需要上层目录里的 .lrc / .yrc
```

`make data` 会在 LRC 与 YRC 时间差超过 300 ms 时给出提示，并以 YRC（逐词时间）为准，
这样即使两份文件有一两句对不上，也不会影响画面。

## 8. 已知取舍

* 音频同步是“同时启动”级别的：程序用单调时钟推进画面，并先把播放器拉起来（默认等
  180 ms）再开始计时，避免把播放器的启动开销算进去。若想更精确，用 `--offset` 微调。
* 终端单元格是 8x16 像素（宽高比 1:2），所以程序里所有圆形都按 `ry = rx * 0.5` 修正，
  大字则是把点阵字体采样到正方形的单元格块里，保证字母不会被拉长。
* 颜色按 `COLORTERM`/`TERM` 自动降级到 256/16/单色；`--color mono` 只靠字符密度表现明暗。

祝你播放愉快。