/* TPlate help text
 Copyright (C) 2026 Ave Tealeaf

This file is part of TPlate.

TPlate is free software: you can redistribute and/or modify it under the terms
 of the GNU General Public License, version 3 or later, as published by the
 Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 details.

You should have received a copy of the GNU General Public License along with
 this program. If not, see <https://www.gnu.org/licenses/>. */

char helptext[]=
"  H   - show this here Help text! (press Enter/H to get back to normal)\n"
" WASD - rotate planet\n"
"  QE  - scrub forward/backward in time\n"
"  []  - single-step time forward/backward\n"
" mouse left click - select continent\n"
"  -+  - select previous/next continent\n"
"  G   - draw ('Genesis') a new continent (enters draw mode)\n"
"  R   - split ('Rift') current continent (enters draw mode)\n"
"  M   - Move current continent (enters move mode)\n"
"  N   - reName current (enters text mode)\n"
"  C   - reColor current continent (enters palette mode)\n"
"  T   - show graph ('Timeline') of continent speeds (buggy)\n"
"  O   - write Out data (aka 'save file to disk')\n"
"  L   - Load data (aka 'load file from disk')\n"
" Esc. - Close program (asks before closing)\n"
"\n"
"Draw mode commands:\n"
" mouse left click - add new point\n"
" Backspace - delete last point (abort if none left)\n"
" Tab - abort current drawing\n"
" Enter - confirm current drawing\n"
"\n"
"Move mode commands:\n"
" WASD - rotate planet\n"
"  QE  - rotate continent widdershins/clockwise\n"
" Backspace - abort move\n"
" Enter - confirm new continent location\n"
"\n"
"Palette mode commands:\n"
" mouse - move color sliders\n"
" Enter - confirm new color\n"
"Text mode:\n"
" letters - type\n"
" Shift+letter - uppercase\n"
" Backspace - delete last letter\n"
" Enter - confirm new continent name\n"
;
