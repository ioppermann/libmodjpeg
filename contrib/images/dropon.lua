gd = require "gd"

im = gd.createTrueColor(160, 50)

red = im:colorAllocate(255, 0, 0)
green = im:colorAllocate(0, 255, 0)
blue = im:colorAllocate(0, 0, 255)

white = im:colorAllocate(255, 255, 255)
black = im:colorAllocate(0, 0, 0)

im:filledRectangle(0, 0, 160, 50, white)

im:filledRectangle(1, 2, 48, 13, red)
im:filledRectangle(1, 19, 48, 30, green)
im:filledRectangle(1, 36, 48, 47, blue)

im:string(gd.FONT_GIANT, 61, 6, "Powered by", black)
im:string(gd.FONT_GIANT, 60, 5, "Powered by", white)
im:string(gd.FONT_GIANT, 61, 26, "libmodjpeg", black)
im:string(gd.FONT_GIANT, 60, 25, "libmodjpeg", white)

im:png("unmaskeddropon.png")
im:jpeg("dropon.jpg", 90)

im:filledRectangle(0, 0, 160, 50, black)

im:filledRectangle(1, 2, 48, 13, white)
im:filledRectangle(1, 19, 48, 30, white)
im:filledRectangle(1, 36, 48, 47, white)

gray = im:colorAllocate(64, 64, 64)

im:string(gd.FONT_GIANT, 61, 6, "Powered by", gray)
im:string(gd.FONT_GIANT, 60, 5, "Powered by", white)
im:string(gd.FONT_GIANT, 61, 26, "libmodjpeg", gray)
im:string(gd.FONT_GIANT, 60, 25, "libmodjpeg", white)

im:png("mask.png")
im:jpeg("mask.jpg", 90)

-- gm composite -compose CopyOpacity mask.png unmaskeddropon.png dropon.png
