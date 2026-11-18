ANS 1964 user score
===================

user.ansscore is a demo score. Replace it with your own; the plugin reads it when
Score = "User Score" is selected.

Format (little-endian):
  offset 0   : ASCII "ANS1"
  offset 4   : u16 version = 1
  offset 6   : u16 width in pixels
  offset 8   : u16 height = 720
  offset 10  : u16 pixels per mm (10 is a good default)
  offset 12  : u32 number of pixel bytes (width * 720)
  offset 16  : width*720 bytes, 8-bit grayscale

Row 0 is the highest pitch; row 719 is the lowest (about 21 Hz).
White (255) = open mastic / full level. Black (0) = closed.
Horizontal position is time; a wider/brighter mark is louder and, when it spans
several rows, becomes a cluster or noise.

Duration at full speed (24 mm/s) = width / (pixels_per_mm * 24) seconds.
At Speed = 12 the same score takes twice as long.

Convert a PNG/JPEG sonogram with the tool in the project folder:

  python3 tools/png_to_ansscore.py my_image.png Content/Scores/user.ansscore --px-per-mm 10

(Pillow is required: python3 -m pip install pillow.)


On the MPC, the plugin's EDIT sub-tab (press SCORE twice) can draw/erase this image
in memory. Those edits are saved in the MPC project state when you save the project.
Use the desktop editor (tools/ans_score_editor.py) when you want to draw with a large
canvas, import a PNG, or push a new file to the device.
