"""Regenerate the original app icon. Pillow is only needed for this optional step."""
from pathlib import Path
from PIL import Image, ImageDraw

image = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((4, 4, 252, 252), radius=62, fill="#101A2A")
draw.arc((56, 65, 200, 209), start=-62, end=263, fill="#A6CAEF", width=13)
draw.line((128, 99, 128, 141, 159, 158), fill="#F4F7FD", width=13, joint="curve")
draw.ellipse((122, 93, 134, 105), fill="#F4F7FD")
draw.ellipse((153, 152, 165, 164), fill="#F4F7FD")
draw.rounded_rectangle((111, 33, 145, 46), radius=6, fill="#A6CAEF")
destination = Path(__file__).resolve().parents[1] / "resources" / "app.ico"
image.save(destination, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print(destination)
