GFXfont headers copied from the [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) `Fonts/` folder
(converted GNU FreeFont, see that repository for licensing). The only change is the include: `<gfxfont.h>` from
GFX Library for Arduino instead of `<Adafruit_GFX.h>`, so they work with Arduino_GFX without pulling in Adafruit GFX.

More sizes/faces: copy another header from that folder, or generate one with Adafruit's `fontconvert`.
Each font header must be included from one .cpp only (see `src/display/Fonts.cpp`), since they define the data.
