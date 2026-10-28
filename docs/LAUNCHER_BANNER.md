# Optional launcher banner

The launcher draws its own header art (starfield, planet, gold moon). To use your own picture instead, put a PNG (or JPG) named `banner.png` in a `launcher` folder next to `PLAYTHISGAME.exe`, i.e. `launcher\banner.png`. It is scaled to cover the header strip (about 759 x 150, centred, edges cropped), so a wide image of roughly 1520 x 300 looks sharpest.

If the file is missing or cannot be read, the drawn banner is used silently. Do not commit banner art you do not own: the repo ships none.

`PLAYTHISGAME.exe --shot out.png --page 0|1|2` renders a launcher page to a PNG without opening a window (handy for checking the look).

Extra `--shot` flags: `--scale 1.5` (render at 150% DPI), `--open N` (show dropdown N, 0-4, open), `--root <dir>` (render that folder's state).
