"""
生成与 404.png 风格一致的错误提示图片（下雨、暗黑背景、可爱风格）
尺寸: 1024x1024
"""
from PIL import Image, ImageDraw, ImageFont
import math
import random
import os

OUTPUT_DIR = r"G:\back\recovered\ruoyi-cpp\src\img"
os.makedirs(OUTPUT_DIR, exist_ok=True)

WIDTH, HEIGHT = 1024, 1024

# 错误码 → (主标题, 副标题)
ERROR_CODES = {
    "400": ("Bad Request", "请求参数错误"),
    "401": ("Unauthorized", "认证失败"),
    "405": ("Method Not Allowed", "方法不允许"),
    "413": ("Request Entity Too Large", "请求体过大"),
    "502": ("Bad Gateway", "网关错误"),
    "503": ("Service Unavailable", "服务不可用"),
}

# 预定义的雨滴种子（固定，保证每次运行结果一致）
random.seed(42)
RAIN_DROPS = [(random.randint(0, WIDTH), random.randint(0, HEIGHT)) for _ in range(200)]


def draw_rain(img, darkness=0.3):
    """绘制斜向雨滴"""
    draw = ImageDraw.Draw(img)
    for x, y in RAIN_DROPS:
        alpha = random.randint(80, 180)
        length = random.randint(10, 25)
        dx = random.randint(2, 5)
        dy = -length
        color = (180, 200, 220, alpha)
        draw.line([(x, y), (x + dx, y + dy)], fill=color, width=1)


def draw_bg(img):
    """绘制渐变暗色背景 + 雨"""
    draw = ImageDraw.Draw(img)
    for y in range(HEIGHT):
        ratio = y / HEIGHT
        r = int(15 + 10 * ratio)
        g = int(18 + 12 * ratio)
        b = int(28 + 20 * ratio)
        draw.line([(0, y), (WIDTH, y)], fill=(r, g, b))
    draw_rain(img)
    return draw


def wrap_text(text: str, font, max_width: int, draw) -> list:
    """简单按空格换行"""
    words = text.split()
    lines, current = [], ""
    for word in words:
        test = (current + " " + word).strip()
        if draw.textlength(test, font=font) <= max_width:
            current = test
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def create_error_image(code: str, title: str, subtitle: str, template_img: Image = None):
    """生成单个错误图片"""
    img = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 背景
    bg = Image.new("RGBA", (WIDTH, HEIGHT), (20, 22, 35, 255))
    draw_bg(bg)
    img = bg

    draw = ImageDraw.Draw(img)

    # 尝试加载字体
    font_paths = [
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
    ]
    font_large = None
    font_small = None
    for fp in font_paths:
        try:
            font_large = ImageFont.truetype(fp, 180)
            font_small = ImageFont.truetype(fp, 56)
            font_cn = ImageFont.truetype(fp, 72)
            break
        except Exception:
            continue
    if font_large is None:
        font_large = ImageFont.load_default()
        font_small = font_large
        font_cn = font_large

    # 画错误码大字
    bbox = draw.textbbox((0, 0), code, font=font_large)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = (WIDTH - text_w) // 2 - 10
    y = 320
    # 白色半透明大字
    draw.text((x, y), code, font=font_large, fill=(255, 255, 255, 220))

    # 英文标题
    en_lines = wrap_text(title, font_small, 700, draw)
    total_en_h = len(en_lines) * (56 + 10)
    start_y = y + text_h + 20
    for i, line in enumerate(en_lines):
        lw = draw.textlength(line, font=font_small)
        draw.text(((WIDTH - lw) // 2, start_y + i * 66), line, font=font_small, fill=(180, 190, 210, 255))

    # 中文副标题
    cn_y = start_y + total_en_h + 20
    cw = draw.textlength(subtitle, font=font_cn)
    draw.text(((WIDTH - cw) // 2, cn_y), subtitle, font=font_cn, fill=(140, 155, 190, 255))

    # 如果有模板图片（404.png），叠加一点原图的特征
    if template_img:
        # 在角落放一个小阴影/云团增加氛围
        for _ in range(3):
            cx = random.randint(50, WIDTH - 200)
            cy = random.randint(50, HEIGHT - 200)
            r = random.randint(80, 200)
            draw.ellipse([(cx - r, cy - r), (cx + r, cy + r)],
                         fill=(40, 50, 80, random.randint(20, 50)))

    return img


def main():
    # 加载 404.png 作为参考（保留原图）
    ref = None
    ref_path = os.path.join(OUTPUT_DIR, "404.png")
    if os.path.exists(ref_path):
        ref = Image.open(ref_path).convert("RGBA")
        print(f"参考图 404.png 加载成功: {ref.size}")

    for code, (title, subtitle) in ERROR_CODES.items():
        out_path = os.path.join(OUTPUT_DIR, f"{code}.png")
        if os.path.exists(out_path):
            print(f"[跳过] {code}.png 已存在")
            continue
        print(f"[生成] {code}.png  →  {title} / {subtitle}")
        img = create_error_image(code, title, subtitle, ref)
        # 保存为 PNG
        img.save(out_path, "PNG")
        print(f"[完成] {out_path}")

    print("\n全部完成！")


if __name__ == "__main__":
    main()
