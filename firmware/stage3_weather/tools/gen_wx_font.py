#!/usr/bin/env python3
"""
生成天气区用的迷你字库 wx_font.h。

只包含 29 个字符：今天明后 0-9 / - 湿度%离线 周日一二三四五六
用 macOS 自带的ヒラギノ丸ゴ（Hiragino Maru Gothic ProN，圆角日文字体）
在 14px 下渲染，然后按 alpha≥128 二值化成 1-bit 掩码。
不做灰度抗锯齿：14px 下灰边在屏上会像"阴影"，硬边更清楚。

用法：
    python3 tools/gen_wx_font.py > wx_font.h
（字体缺失时换 FONT 路径；需要 Pillow）
"""
from PIL import Image, ImageDraw, ImageFont

FONT = "/System/Library/Fonts/ヒラギノ丸ゴ ProN W4.ttc"
PX = 14
CELL_W, CELL_H = 14, 15       # 基线 -12 .. +2，整数像素
BASELINE_X, BASELINE_Y = 8, 20  # 画布内基线位置

# 天只存一次，标签里复用（今天 / 明天 / 后天）
# 末尾追加顶栏用的 湿 度 % 离 线，以及星期 周 日 一 二 三 四 五 六
# （中文用显式索引，不走 wxGlyphIndex）
CHARS = "今天明后" + "0123456789" + "/-" + "湿度%离线" + "周日一二三四五六"


def mask_of(ch):
    f = ImageFont.truetype(FONT, PX)
    img = Image.new("L", (40, 40), 0)
    d = ImageDraw.Draw(img)
    d.text((BASELINE_X, BASELINE_Y), ch, font=f, fill=255, anchor="ls")
    crop = img.crop((BASELINE_X, BASELINE_Y - 12,
                     BASELINE_X + CELL_W, BASELINE_Y + 3))
    adv = int(round(f.getlength(ch)))
    # 二值化：只要覆盖超过一半就算笔画，避免灰边像阴影
    return adv, [1 if v >= 128 else 0 for v in crop.tobytes()]


def main():
    print("// 自动生成，勿手改：tools/gen_wx_font.py")
    print("// 14px ヒラギノ丸ゴ ProN W4，1-bit 硬边，"
          "仅含 今天明后 0-9 / - 湿度%离线 周日一二三四五六")
    print("#pragma once")
    print("#include <stdint.h>")
    print()
    print("#define WX_FONT_W %d" % CELL_W)
    print("#define WX_FONT_H %d" % CELL_H)
    print()
    print("struct WxGlyph { uint8_t adv; uint8_t m[WX_FONT_W * WX_FONT_H]; };")
    print()
    print("static const WxGlyph WX_GLYPHS[] = {")
    for ch in CHARS:
        adv, px = mask_of(ch)
        print("  { %d, {" % adv)
        for row in range(CELL_H):
            vals = px[row * CELL_W:(row + 1) * CELL_W]
            print("    " + ", ".join("%3d" % v for v in vals) + ",")
        print("  } },")
    print("};")
    print()
    print("// 索引：今天明后 = 0..3，'0'..'9' = 4..13，'/' = 14，'-' = 15，")
    print("//       湿 = 16，度 = 17，'%' = 18，离 = 19，线 = 20")
    print("//       周 = 21，日 = 22，一 = 23，二 = 24，三 = 25，四 = 26，")
    print("//       五 = 27，六 = 28")
    print("static inline int wxGlyphIndex(char c) {")
    print("  if (c >= '0' && c <= '9') return 4 + (c - '0');")
    print("  if (c == '/') return 14;")
    print("  if (c == '-') return 15;")
    print("  if (c == '%') return 18;")
    print("  return -1;")
    print("}")


if __name__ == "__main__":
    main()
