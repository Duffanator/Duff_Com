Add-Type -AssemblyName System.Drawing

$csharp = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

public class DuffNewAssetGen {
    static Color Trans = Color.FromArgb(255, 255, 0, 255); // Hot Magenta key

    static void SaveBmp24(Bitmap bmp, string path) {
        int w = bmp.Width, h = bmp.Height;
        int stride = (w * 3 + 3) & ~3;
        using (var fs = new FileStream(path, FileMode.Create))
        using (var bw = new BinaryWriter(fs)) {
            int dataSize = stride * h;
            bw.Write((byte)'B'); bw.Write((byte)'M'); bw.Write(54 + dataSize); bw.Write(0); bw.Write(54);
            bw.Write(40); bw.Write(w); bw.Write(h); bw.Write((short)1); bw.Write((short)24); bw.Write(0);
            bw.Write(dataSize); bw.Write(2835); bw.Write(2835); bw.Write(0); bw.Write(0);
            for (int y = h - 1; y >= 0; y--) {
                for (int x = 0; x < w; x++) {
                    Color c = bmp.GetPixel(x, y);
                    bw.Write(c.B); bw.Write(c.G); bw.Write(c.R);
                }
                for (int p = w * 3; p < stride; p++) bw.Write((byte)0);
            }
        }
    }

    static void PSet(Bitmap b, int x, int y, Color c) {
        if (x >= 0 && x < b.Width && y >= 0 && y < b.Height) b.SetPixel(x, y, c);
    }

    static void FillRect(Bitmap b, int x, int y, int w, int h, Color c) {
        for (int py = y; py < y + h; py++)
            for (int px = x; px < x + w; px++)
                PSet(b, px, py, c);
    }

    static void CopyPixels(Bitmap src, Bitmap dst, int maxW, int maxH, Color clearCol) {
        for (int y = 0; y < dst.Height; y++)
            for (int x = 0; x < dst.Width; x++)
                dst.SetPixel(x, y, clearCol);
        int cw = Math.Min(Math.Min(src.Width, dst.Width), maxW);
        int ch = Math.Min(Math.Min(src.Height, dst.Height), maxH);
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                dst.SetPixel(x, y, src.GetPixel(x, y));
    }

    static void OutlineSprite(Bitmap b, int ox, int oy, int w, int h) {
        bool[,] mask = new bool[w, h];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                Color p = b.GetPixel(ox + x, oy + y);
                mask[x, y] = !(p.R >= 245 && p.G <= 15 && p.B >= 245);
            }
        int[] dx = new int[] { -1, 1, 0, 0 };
        int[] dy = new int[] { 0, 0, -1, 1 };
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (mask[x, y]) continue;
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h && mask[nx, ny]) {
                        PSet(b, ox + x, oy + y, Color.FromArgb(16, 18, 22));
                        break;
                    }
                }
            }
        }
    }

    public static void GenAll(string assetsDir) {
        Directory.CreateDirectory(assetsDir);
        GenModalBmp(Path.Combine(assetsDir, "MODAL.BMP"));
        GenArvTurretBmp(Path.Combine(assetsDir, "ARV_TURR.BMP"));
        ExtendFloorsBmp(Path.Combine(assetsDir, "FLOORS.BMP"));
        ExtendUnitsBmp(Path.Combine(assetsDir, "UNITS.BMP"));
        ExtendDeathBmp(Path.Combine(assetsDir, "DEATH.BMP"));
        ExtendPortraitsBmp(Path.Combine(assetsDir, "PORTRAIT.BMP"));
    }

    static void GenModalBmp(string path) {
        int mw = 236, mh = 104;
        using (Bitmap b = new Bitmap(mw, mh)) {
            for (int y = 0; y < mh; y++) {
                for (int x = 0; x < mw; x++) {
                    Color c = ((y & 1) == 0) ? Color.FromArgb(18, 30, 58) : Color.FromArgb(14, 24, 48);
                    if ((x % 16 == 0) || (y % 16 == 0)) c = Color.FromArgb(24, 42, 78);
                    int dx = x - mw / 2, dy = (y - mh / 2) * 2;
                    int r2 = dx * dx + dy * dy;
                    if (Math.Abs(r2 - 1600) < 90 || Math.Abs(r2 - 4200) < 120) {
                        c = Color.FromArgb(28, 58, 96);
                    }
                    if (x < 2 || x >= mw - 2 || y < 2 || y >= mh - 2) c = Color.FromArgb(56, 108, 172);
                    PSet(b, x, y, c);
                }
            }
            SaveBmp24(b, path);
        }
    }

    static void GenArvTurretBmp(string path) {
        int[] dx_tab = new int[] {  0,  5,  7,  5,  0, -5, -7, -5 };
        int[] dy_tab = new int[] { -5, -3,  0,  3,  5,  3,  0, -3 };
        int[] px_tab = new int[] {  2,  1,  0, -1, -2, -1,  0,  1 };
        int[] py_tab = new int[] {  0,  1,  2,  1,  0, -1, -2, -1 };

        using (Bitmap b = new Bitmap(8 * 24, 2 * 32)) {
            using (Graphics g = Graphics.FromImage(b)) g.Clear(Trans);
            for (int t = 0; t < 2; t++) {
                Color baseHi = (t == 0) ? Color.FromArgb(92, 148, 72) : Color.FromArgb(148, 162, 188);
                Color baseLo = (t == 0) ? Color.FromArgb(58, 98, 46)  : Color.FromArgb(92, 74, 144);
                Color glowC  = (t == 0) ? Color.FromArgb(72, 244, 255) : Color.FromArgb(142, 252, 56);
                Color brlC   = (t == 0) ? Color.FromArgb(168, 178, 192) : Color.FromArgb(118, 86, 172);

                for (int dir = 0; dir < 8; dir++) {
                    int ox = dir * 24, oy = t * 32;
                    for (int y = 19; y <= 25; y++) {
                        for (int x = 5; x <= 18; x++) {
                            int d2 = (x - 12) * (x - 12) + (y - 22) * (y - 22) * 3;
                            if (d2 <= 36) {
                                Color rc = (d2 >= 26) ? (((x + y) & 1) != 0 ? Color.FromArgb(238, 196, 48) : Color.FromArgb(32, 36, 42))
                                                      : Color.FromArgb(82, 88, 98);
                                PSet(b, ox + x, oy + y, rc);
                            }
                        }
                    }
                    for (int y = 11; y <= 20; y++) {
                        for (int x = 6; x <= 17; x++) {
                            int d2 = (x - 12) * (x - 12) + (y - 16) * (y - 16) * 2;
                            if (d2 <= 30) {
                                PSet(b, ox + x, oy + y, (y <= 13) ? baseHi : baseLo);
                            }
                        }
                    }
                    PSet(b, ox + 11, oy + 12, Color.FromArgb(255, 228, 64));
                    PSet(b, ox + 12, oy + 12, glowC);
                    PSet(b, ox + 11, oy + 13, glowC);
                    PSet(b, ox + 12, oy + 13, Color.White);

                    for (int s = 1; s <= 7; s++) {
                        int cx = 12 + (dx_tab[dir] * s) / 5;
                        int cy = 15 + (dy_tab[dir] * s) / 5;
                        int b1x = cx - px_tab[dir] / 2, b1y = cy - py_tab[dir] / 2;
                        int b2x = cx + px_tab[dir],     b2y = cy + py_tab[dir];
                        Color cc = (s >= 6) ? glowC : ((s == 5) ? Color.FromArgb(255, 224, 64) : brlC);
                        PSet(b, ox + b1x, oy + b1y, cc);
                        PSet(b, ox + b2x, oy + b2y, cc);
                    }
                    OutlineSprite(b, ox, oy, 24, 32);
                }
            }
            SaveBmp24(b, path);
        }
    }

    static bool InDiamond(int x, int y) {
        int dx = (x < 16) ? (15 - x) : (x - 16);
        int dy = (y < 8)  ? (7 - y)  : (y - 8);
        return (dx + dy * 2 <= 15);
    }

    static bool IsDiamondEdge(int x, int y) {
        int dx = (x < 16) ? (15 - x) : (x - 16);
        int dy = (y < 8)  ? (7 - y)  : (y - 8);
        int d = dx + dy * 2;
        return (d == 14 || d == 15);
    }

    static void ExtendFloorsBmp(string path) {
        if (!File.Exists(path)) return;
        Bitmap oldBmp;
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read)) {
            oldBmp = new Bitmap(fs);
        }
        int newW = 19 * 32;
        using (Bitmap b = new Bitmap(newW, 16)) {
            CopyPixels(oldBmp, b, 18 * 32, 16, Trans);
            oldBmp.Dispose();
            int ox = 18 * 32;
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 32; x++) {
                    if (!InDiamond(x, y)) { PSet(b, ox + x, y, Trans); continue; }
                    int dx_c = Math.Abs(x - 16), dy_c = Math.Abs(y - 8) * 2;
                    int dist = dx_c + dy_c;
                    Color c;
                    if (IsDiamondEdge(x, y))
                        c = (((x + y) & 2) != 0) ? Color.FromArgb(238, 196, 48) : Color.FromArgb(28, 32, 38);
                    else if (dist == 9 || dist == 10)
                        c = Color.FromArgb(72, 244, 255);
                    else if ((dx_c == 3 && dy_c <= 4) || (dy_c <= 1 && dx_c <= 3))
                        c = Color.FromArgb(255, 228, 64);
                    else
                        c = (((x + y) & 1) == 0) ? Color.FromArgb(62, 68, 78) : Color.FromArgb(52, 58, 68);
                    PSet(b, ox + x, y, c);
                }
            }
            SaveBmp24(b, path);
        }
    }

    static void ExtendUnitsBmp(string path) {
        if (!File.Exists(path)) return;
        Bitmap oldBmp;
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read)) {
            oldBmp = new Bitmap(fs);
        }
        int newH = 19 * 4 * 32;
        using (Bitmap b = new Bitmap(8 * 24, newH)) {
            CopyPixels(oldBmp, b, 8 * 24, 15 * 4 * 32, Trans);
            oldBmp.Dispose();

            int[] dx_tab = new int[] { 0,  5,  7,  5,  0, -5, -7, -5 };
            int[] dy_tab = new int[] { -4, -3, 0,  3,  4,  3,  0, -3 };

            for (int type = 15; type < 19; type++) {
                for (int pose = 0; pose < 4; pose++) {
                    int row = type * 4 + pose;
                    int oy = row * 32;
                    int y_off = (pose == 1) ? 3 : ((pose >= 2) ? -1 : 0);
                    int leg_l = (pose == 2) ? -2 : (pose == 3 ? 2 : 0);
                    int leg_r = (pose == 2) ?  2 : (pose == 3 ? -2 : 0);

                    for (int dir = 0; dir < 8; dir++) {
                        int ox = dir * 24;
                        bool backView = (dir == 0 || dir == 1 || dir == 7);
                        int eyeShift  = (dir == 2 || dir == 3) ? 1 : (dir == 5 || dir == 6) ? -1 : 0;

                        if (type == 15) { // UNIT_SPR_CRYPTID: Chitinous Crimson/Obsidian Bio-Predator
                            int lFootY = 26 + (pose == 2 ? -1 : 0);
                            int rFootY = 26 + (pose == 3 ? -1 : 0);
                            int lx0 = 8  + leg_l / 2;
                            int rx0 = 14 + leg_r / 2;
                            for (int y = 19 + (pose == 1 ? 2 : 0); y <= lFootY; y++) {
                                PSet(b, ox + lx0,     oy + y, Color.FromArgb(188, 42, 38));
                                PSet(b, ox + lx0 + 1, oy + y, Color.FromArgb(68, 22, 26));
                            }
                            for (int y = 19 + (pose == 1 ? 2 : 0); y <= rFootY; y++) {
                                PSet(b, ox + rx0,     oy + y, Color.FromArgb(188, 42, 38));
                                PSet(b, ox + rx0 + 1, oy + y, Color.FromArgb(68, 22, 26));
                            }
                            // White bone raptor talons on feet
                            PSet(b, ox + lx0 - 1, oy + lFootY + 1, Color.FromArgb(238, 238, 244));
                            PSet(b, ox + lx0,     oy + lFootY + 1, Color.FromArgb(158, 30, 30));
                            PSet(b, ox + rx0,     oy + rFootY + 1, Color.FromArgb(158, 30, 30));
                            PSet(b, ox + rx0 + 1, oy + rFootY + 1, Color.FromArgb(238, 238, 244));

                            // V-tapered segmented chitin thorax & shoulder spikes
                            for (int y = 10 + y_off; y < 19 + (pose == 1 ? 2 : 0); y++) {
                                int dyT = y - (10 + y_off);
                                int hw = (dyT <= 3) ? 6 : (dyT <= 6 ? 5 : 4);
                                for (int x = 12 - hw; x <= 11 + hw; x++) {
                                    bool rib = (dyT % 3 == 2 && x >= 9 && x <= 14);
                                    Color tc = rib      ? Color.FromArgb(44, 18, 24) :
                                               (x < 10) ? Color.FromArgb(218, 48, 42) :
                                               (x < 14) ? Color.FromArgb(158, 30, 30) :
                                                          Color.FromArgb(68, 22, 28);
                                    PSet(b, ox + x, oy + y, tc);
                                }
                            }
                            // Contoured horned predator crest
                            for (int y = 4 + y_off; y < 10 + y_off; y++) {
                                int hw = (y == 4 + y_off) ? 2 : (y == 5 + y_off ? 3 : 4);
                                for (int x = 12 - hw; x <= 11 + hw; x++) {
                                    bool horn = (y == 4 + y_off && (x == 10 || x == 13));
                                    PSet(b, ox + x, oy + y, horn ? Color.FromArgb(238, 238, 244) :
                                                                   (x < 12 ? Color.FromArgb(196, 40, 36) : Color.FromArgb(118, 24, 28)));
                                }
                            }
                            if (!backView) {
                                PSet(b, ox + 9  + eyeShift, oy + 7 + y_off, Color.FromArgb(142, 252, 56));
                                PSet(b, ox + 10 + eyeShift, oy + 7 + y_off, Color.FromArgb(255, 228, 64));
                                PSet(b, ox + 13 + eyeShift, oy + 7 + y_off, Color.FromArgb(255, 228, 64));
                                PSet(b, ox + 14 + eyeShift, oy + 7 + y_off, Color.FromArgb(142, 252, 56));
                            }
                            // Directional bone scythe-claws
                            for (int s = 1; s <= 5; s++) {
                                int cx1 = 8  + (dx_tab[dir] * s) / 4, cy1 = 14 + y_off + (dy_tab[dir] * s) / 4;
                                int cx2 = 15 + (dx_tab[dir] * s) / 4, cy2 = 14 + y_off + (dy_tab[dir] * s) / 4;
                                Color cc = (s >= 3) ? Color.FromArgb(238, 240, 246) : Color.FromArgb(208, 48, 42);
                                PSet(b, ox + cx1, oy + cy1, cc);
                                PSet(b, ox + cx2, oy + cy2, cc);
                            }
                        } else if (type == 16) { // UNIT_SPR_ZOMBIE: Infected Necrotic Operative/Civilian
                            int lFootY = 26 + (pose == 2 ? -1 : 0);
                            int rFootY = 26 + (pose == 3 ? -1 : 0);
                            int lx0 = 9  + leg_l / 2;
                            int rx0 = 13 + leg_r / 2;
                            for (int y = 19 + (pose == 1 ? 1 : 0); y <= lFootY; y++) {
                                PSet(b, ox + lx0,     oy + y, Color.FromArgb(66, 74, 88));
                                PSet(b, ox + lx0 + 1, oy + y, Color.FromArgb(42, 48, 58));
                            }
                            for (int y = 19 + (pose == 1 ? 1 : 0); y <= rFootY; y++) {
                                PSet(b, ox + rx0,     oy + y, Color.FromArgb(66, 74, 88));
                                PSet(b, ox + rx0 + 1, oy + y, Color.FromArgb(42, 48, 58));
                            }
                            PSet(b, ox + lx0 - 1, oy + lFootY + 1, Color.FromArgb(46, 50, 58));
                            PSet(b, ox + lx0,     oy + lFootY + 1, Color.FromArgb(62, 68, 78));
                            PSet(b, ox + rx0,     oy + rFootY + 1, Color.FromArgb(62, 68, 78));
                            PSet(b, ox + rx0 + 1, oy + rFootY + 1, Color.FromArgb(46, 50, 58));

                            // Torn blood-stained tactical vest
                            int tTop = 11 + y_off;
                            int tBot = 19 + (pose == 1 ? 1 : 0);
                            for (int y = tTop; y < tBot; y++) {
                                int dy = y - tTop;
                                int x0 = (dy >= 6) ? 8 : 7;
                                int x1 = (dy >= 6) ? 15 : 16;
                                for (int x = x0; x <= x1; x++) {
                                    bool blood = (!backView && ((x + y) % 5 == 0) && x >= 9 && x <= 14);
                                    PSet(b, ox + x, oy + y, blood ? Color.FromArgb(204, 32, 36) :
                                                                    (x < 11 ? Color.FromArgb(78, 124, 184) :
                                                                     x < 14 ? Color.FromArgb(56, 94, 148)  :
                                                                              Color.FromArgb(38, 64, 106)));
                                }
                            }
                            // Outstretched necrotic zombie arms
                            for (int s = 1; s <= 4; s++) {
                                int ax1 = 7  + (dx_tab[dir] * s) / 4, ay1 = 13 + y_off + (dy_tab[dir] * s) / 4;
                                int ax2 = 16 + (dx_tab[dir] * s) / 4, ay2 = 13 + y_off + (dy_tab[dir] * s) / 4;
                                PSet(b, ox + ax1, oy + ay1, Color.FromArgb(124, 194, 96));
                                PSet(b, ox + ax2, oy + ay2, Color.FromArgb(88, 148, 68));
                            }
                            // Contoured necrotic green head & glowing red eyes
                            for (int y = 4 + y_off; y <= 10 + y_off; y++) {
                                int x0 = (y <= 5 + y_off || y == 10 + y_off) ? 9 : 8;
                                int x1 = (y <= 5 + y_off || y == 10 + y_off) ? 14 : 15;
                                for (int x = x0; x <= x1; x++) {
                                    PSet(b, ox + x, oy + y, (x < 12) ? Color.FromArgb(124, 194, 96) : Color.FromArgb(88, 148, 68));
                                }
                            }
                            if (!backView) {
                                PSet(b, ox + 10 + eyeShift, oy + 7 + y_off, Color.FromArgb(242, 42, 38));
                                PSet(b, ox + 13 + eyeShift, oy + 7 + y_off, Color.FromArgb(242, 42, 38));
                                PSet(b, ox + 11 + eyeShift, oy + 9 + y_off, Color.FromArgb(148, 22, 26));
                            }
                        } else if (type == 17) { // UNIT_SPR_QUAD_DRONE
                            int bob = ((pose & 1) != 0 ? -1 : 0) - 4;
                            for (int s = -5; s <= 5; s++) {
                                PSet(b, ox + 12 + s, oy + 15 + bob + s / 2, Color.FromArgb(88, 96, 108));
                                PSet(b, ox + 12 + s, oy + 15 + bob - s / 2, Color.FromArgb(88, 96, 108));
                            }
                            for (int x = 3; x <= 8; x++) {
                                PSet(b, ox + x, oy + 12 + bob, ((x + pose) & 1) != 0 ? Color.FromArgb(72, 244, 255) : Color.FromArgb(192, 202, 214));
                                PSet(b, ox + x, oy + 18 + bob, ((x + pose) & 1) != 0 ? Color.FromArgb(72, 244, 255) : Color.FromArgb(192, 202, 214));
                            }
                            for (int x = 15; x <= 20; x++) {
                                PSet(b, ox + x, oy + 12 + bob, ((x + pose) & 1) != 0 ? Color.FromArgb(192, 202, 214) : Color.FromArgb(72, 244, 255));
                                PSet(b, ox + x, oy + 18 + bob, ((x + pose) & 1) != 0 ? Color.FromArgb(192, 202, 214) : Color.FromArgb(72, 244, 255));
                            }
                            FillRect(b, ox + 8, oy + 13 + bob, 8, 5, Color.FromArgb(76, 134, 64));
                            PSet(b, ox + 11, oy + 15 + bob, Color.FromArgb(255, 228, 64));
                            PSet(b, ox + 12, oy + 15 + bob, Color.FromArgb(72, 244, 255));
                            for (int s = 1; s <= 5; s++) {
                                int px = 12 + (dx_tab[dir] * s) / 4, py = 18 + bob + (dy_tab[dir] * s) / 4;
                                PSet(b, ox + px, oy + py, (s == 5) ? Color.FromArgb(255, 228, 64) : Color.FromArgb(148, 158, 172));
                            }
                        } else { // UNIT_SPR_UFO_DRONE
                            int bob = ((pose & 1) != 0 ? -1 : 0) - 5;
                            for (int y = 12 + bob; y <= 19 + bob; y++) {
                                int rx = (y == 15 + bob || y == 16 + bob) ? 8 : ((y == 14 + bob || y == 17 + bob) ? 6 : 4);
                                for (int x = 12 - rx; x <= 12 + rx; x++) {
                                    Color dc = (y == 15 + bob || y == 16 + bob)
                                        ? (((x + pose) & 1) != 0 ? Color.FromArgb(142, 252, 56) : Color.FromArgb(72, 244, 255))
                                        : ((y < 15 + bob) ? Color.FromArgb(158, 172, 196) : Color.FromArgb(108, 78, 168));
                                    PSet(b, ox + x, oy + y, dc);
                                }
                            }
                            FillRect(b, ox + 10, oy + 11 + bob, 5, 2, Color.FromArgb(72, 244, 255));
                            for (int s = 2; s <= 5; s++) {
                                int px = 12 + (dx_tab[dir] * s) / 4, py = 17 + bob + (dy_tab[dir] * s) / 4;
                                PSet(b, ox + px, oy + py, (s == 5) ? Color.FromArgb(142, 252, 56) : Color.FromArgb(128, 88, 188));
                            }
                        }
                        OutlineSprite(b, ox, oy, 24, 32);
                    }
                }
            }
            SaveBmp24(b, path);
        }
    }

    static void ExtendDeathBmp(string path) {
        if (!File.Exists(path)) return;
        Bitmap oldBmp;
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read)) {
            oldBmp = new Bitmap(fs);
        }
        int newH = 19 * 32;
        using (Bitmap b = new Bitmap(4 * 32, newH)) {
            CopyPixels(oldBmp, b, 4 * 32, 15 * 32, Trans);
            oldBmp.Dispose();

            for (int type = 15; type < 19; type++) {
                int oy = type * 32;

                for (int stg = 0; stg < 4; stg++) {
                    int ox = stg * 32;
                    Color[,] spr = new Color[32, 32];
                    for (int py = 0; py < 32; py++)
                        for (int px = 0; px < 32; px++) spr[px, py] = Trans;

                    if (type == 15) {
                        // ====================================================================
                        // CRYPTID: Chitinous Crimson/Obsidian Bio-Predator + Scythe Claws + Ichor
                        // ====================================================================
                        Color chHi  = Color.FromArgb(218, 48, 42);
                        Color chMd  = Color.FromArgb(158, 30, 30);
                        Color chDk  = Color.FromArgb(56, 22, 28);
                        Color clawC = Color.FromArgb(238, 240, 246);
                        Color clawSh= Color.FromArgb(168, 176, 190);
                        Color eyeC  = Color.FromArgb(255, 230, 55);
                        Color ichHi = Color.FromArgb(142, 252, 56);
                        Color ichDk = Color.FromArgb(58, 168, 34);

                        if (stg == 0) {
                            // Stage 0: Fatal hit! Rearing back, 4 bone scythe-claws flung wide, carapace ichor burst
                            for (int y = 18; y <= 25; y++) {
                                spr[10, y] = chMd; spr[11, y] = chDk;
                                spr[19, y] = chMd; spr[20, y] = chDk;
                            }
                            spr[9,  25] = clawC; spr[10, 25] = chHi;
                            spr[19, 25] = chHi;  spr[21, 25] = clawC;
                            // Segmented crimson/obsidian thorax
                            for (int y = 9; y <= 18; y++) {
                                for (int x = 10; x <= 20; x++) {
                                    bool rib = ((y - 9) % 3 == 0);
                                    spr[x, y] = rib ? chDk : (x < 14 ? chHi : (x < 18 ? chMd : chDk));
                                }
                            }
                            // Flung-wide bone scythe-claws
                            for (int i = 0; i < 6; i++) {
                                spr[9 - i,  11 - i / 2] = (i >= 3) ? clawC : chHi;
                                spr[9 - i,  12 - i / 2] = (i >= 3) ? clawSh : chMd;
                                spr[21 + i, 11 - i / 2] = (i >= 3) ? clawC : chMd;
                                spr[21 + i, 12 - i / 2] = (i >= 3) ? clawSh : chDk;
                            }
                            // Horned predator crest & compound eyes
                            for (int y = 3; y <= 9; y++)
                                for (int x = 11; x <= 19; x++)
                                    if (!((y == 3 || y == 9) && (x == 11 || x == 19)))
                                        spr[x, y] = (y <= 5 && (x == 12 || x == 18)) ? chHi : (x < 15 ? chMd : chDk);
                            spr[12, 6] = eyeC; spr[13, 6] = ichHi;
                            spr[17, 6] = ichHi; spr[18, 6] = eyeC;
                            // Bio-ichor chest burst
                            spr[14, 12] = Color.White; spr[15, 12] = ichHi; spr[16, 13] = ichHi;
                            spr[13, 14] = ichDk; spr[17, 11] = ichHi; spr[18, 14] = ichDk;
                        } else if (stg == 1) {
                            // Stage 1: Raptor legs buckling, dorsal carapace hunching, claws curling inward
                            for (int y = 20; y <= 25; y++) {
                                spr[11, y] = chDk; spr[12, y] = chMd;
                                spr[18, y] = chMd; spr[19, y] = chDk;
                            }
                            for (int y = 12; y <= 20; y++) {
                                for (int x = 9; x <= 20; x++) {
                                    bool rib = ((x + y) % 3 == 0);
                                    spr[x, y] = rib ? chDk : (x < 14 ? chHi : (x < 18 ? chMd : chDk));
                                }
                            }
                            for (int i = 0; i < 5; i++) {
                                spr[8 - i / 2, 15 + i] = (i >= 2) ? clawC : chMd;
                                spr[21 + i / 2, 15 + i] = (i >= 2) ? clawSh : chDk;
                            }
                            for (int y = 8; y <= 14; y++)
                                for (int x = 7; x <= 13; x++)
                                    spr[x, y] = (x < 10) ? chHi : chMd;
                            spr[8, 11] = eyeC; spr[10, 11] = eyeC;
                            spr[14, 23] = ichHi; spr[15, 24] = ichDk; spr[16, 25] = ichHi;
                        } else if (stg == 2) {
                            // Stage 2: Pitching sideways onto ground, scythe-claws & chitin plates sprawling
                            for (int y = 20; y <= 25; y++)
                                for (int x = 18; x <= 25; x++)
                                    spr[x, y] = (y <= 22) ? chMd : chDk;
                            for (int y = 17; y <= 24; y++)
                                for (int x = 10; x <= 19; x++)
                                    spr[x, y] = ((x + y) % 3 == 0) ? chDk : (y <= 20 ? chHi : chMd);
                            for (int y = 16; y <= 21; y++)
                                for (int x = 6; x <= 11; x++)
                                    spr[x, y] = (y <= 18) ? chHi : chMd;
                            for (int x = 5; x <= 9; x++) spr[x, 23] = clawC;
                            spr[14, 24] = ichHi; spr[15, 25] = ichDk;
                        } else {
                            // Stage 3: Persistent Prone Cryptid Corpse (inside 32x16 floor diamond)
                            for (int y = 19; y <= 28; y++)
                                for (int x = 10; x <= 21; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 26)
                                        spr[x, y] = ((x + y) & 1) == 0 ? ichDk : ichHi;
                            // Curled raptor hind legs
                            for (int i = 0; i < 6; i++) {
                                spr[18 + i, 22 + i / 3] = chMd;
                                spr[18 + i, 23 + i / 3] = chDk;
                                if (i < 5) spr[17 + i, 25 + i / 3] = chDk;
                            }
                            // Segmented crimson/obsidian dorsal carapace
                            for (int y = 20; y <= 25; y++) {
                                int rs = (y - 20) / 3;
                                for (int x = 10 + rs; x <= 18 + rs; x++) {
                                    bool seam = ((x - rs) % 3 == 0);
                                    spr[x, y] = seam ? chDk : (y <= 21 ? chHi : (y <= 23 ? chMd : chDk));
                                }
                            }
                            // Horned skull + dim compound eye
                            for (int y = 19; y <= 23; y++)
                                for (int x = 7; x <= 11; x++)
                                    if (!((y == 19 || y == 23) && x == 7))
                                        spr[x, y] = (y <= 20) ? chHi : chMd;
                            spr[8, 21] = eyeC;
                            // Dual bone scythe-claws resting on floor diamond
                            for (int x = 8; x <= 13; x++) spr[x, 25] = (x <= 11) ? clawC : chHi;
                            for (int x = 11; x <= 16; x++) spr[x, 27] = (x <= 14) ? clawSh : chMd;
                        }
                    } else if (type == 16) {
                        // ====================================================================
                        // ZOMBIE: Infected Necrotic Operative/Civilian + Torn Bloodied Armor
                        // ====================================================================
                        Color zkHi  = Color.FromArgb(124, 194, 96);
                        Color zkMd  = Color.FromArgb(88, 148, 68);
                        Color zkSh  = Color.FromArgb(54, 98, 44);
                        Color vsHi  = Color.FromArgb(78, 124, 184);
                        Color vsSh  = Color.FromArgb(46, 76, 122);
                        Color pntMd = Color.FromArgb(66, 74, 88);
                        Color pntSh = Color.FromArgb(42, 48, 58);
                        Color bootC = Color.FromArgb(52, 56, 64);
                        Color bldHi = Color.FromArgb(218, 30, 36);
                        Color bldDk = Color.FromArgb(132, 16, 22);
                        Color ichHi = Color.FromArgb(142, 252, 56);

                        if (stg == 0) {
                            // Stage 0: Fatal hit! Necrotic arms flung out, torn vest bursting with blood & spores
                            for (int y = 18; y <= 24; y++) {
                                spr[12, y] = pntMd; spr[13, y] = pntSh;
                                spr[17, y] = pntMd; spr[18, y] = pntSh;
                            }
                            spr[11, 25] = bootC; spr[12, 25] = bootC; spr[13, 25] = bootC;
                            spr[17, 25] = bootC; spr[18, 25] = bootC; spr[19, 25] = bootC;
                            for (int y = 10; y <= 17; y++)
                                for (int x = 11; x <= 19; x++)
                                    spr[x, y] = ((x + y) % 4 == 0) ? bldHi : (x < 15 ? vsHi : vsSh);
                            for (int i = 0; i < 4; i++) {
                                spr[10 - i, 12 - i / 2] = (i >= 2) ? zkHi : vsHi;
                                spr[20 + i, 12 + i / 2] = (i >= 2) ? zkMd : vsSh;
                            }
                            for (int y = 4; y <= 9; y++)
                                for (int x = 11; x <= 16; x++)
                                    spr[x, y] = (x < 14) ? zkHi : zkMd;
                            spr[12, 6] = bldHi; spr[15, 6] = bldHi;
                            spr[13, 8] = bldDk; spr[14, 8] = bldDk;
                            spr[14, 12] = ichHi; spr[15, 13] = bldHi; spr[16, 12] = bldDk;
                            spr[13, 14] = bldHi; spr[17, 11] = ichHi;
                        } else if (stg == 1) {
                            // Stage 1: Dropping onto knees, necrotic head slumping forward
                            for (int y = 21; y <= 25; y++)
                                for (int x = 13; x <= 21; x++)
                                    if (x <= 16 || x >= 18) spr[x, y] = (y == 25) ? bootC : pntSh;
                            for (int y = 13; y <= 21; y++)
                                for (int x = 10; x <= 18; x++)
                                    spr[x, y] = ((x + y) % 4 == 0) ? bldHi : (x < 14 ? vsHi : vsSh);
                            for (int y = 8; y <= 13; y++)
                                for (int x = 8; x <= 13; x++)
                                    spr[x, y] = (x < 11) ? zkHi : zkMd;
                            spr[9, 10] = bldHi; spr[11, 10] = bldHi;
                            spr[13, 17] = bldHi; spr[14, 18] = ichHi; spr[14, 24] = bldDk; spr[15, 25] = bldHi;
                        } else if (stg == 2) {
                            // Stage 2: Collapsing horizontally toward ground
                            for (int y = 20; y <= 25; y++)
                                for (int x = 18; x <= 25; x++)
                                    spr[x, y] = (x >= 23) ? bootC : pntSh;
                            for (int y = 18; y <= 24; y++)
                                for (int x = 11; x <= 18; x++)
                                    spr[x, y] = (y == 21 && x >= 13 && x <= 16) ? bldHi : (y <= 20 ? vsHi : vsSh);
                            for (int y = 16; y <= 21; y++)
                                for (int x = 7; x <= 11; x++)
                                    spr[x, y] = (y <= 18) ? zkHi : zkMd;
                            spr[14, 23] = bldHi; spr[15, 24] = ichHi;
                        } else {
                            // Stage 3: Persistent Prone Zombie Corpse (inside 32x16 floor diamond)
                            for (int y = 20; y <= 27; y++)
                                for (int x = 10; x <= 20; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 24)
                                        spr[x, y] = ((x + y) % 3 == 0) ? ichHi : (((x + y) & 1) == 0 ? bldDk : bldHi);
                            for (int i = 0; i < 7; i++) {
                                spr[17 + i, 22 + i / 3] = (i >= 5) ? bootC : pntMd;
                                spr[17 + i, 23 + i / 3] = (i >= 5) ? bootC : pntSh;
                                if (i < 6) spr[16 + i, 25 + i / 3] = (i >= 4) ? bootC : pntSh;
                            }
                            for (int y = 21; y <= 25; y++) {
                                int rowShift = (y - 21) / 2;
                                for (int x = 11 + rowShift; x <= 17 + rowShift; x++)
                                    spr[x, y] = ((x + y) % 3 == 0) ? bldHi : (y <= 22 ? vsHi : vsSh);
                            }
                            spr[10, 24] = zkMd; spr[9, 25] = zkHi; spr[8, 25] = zkSh;
                            for (int y = 19; y <= 23; y++)
                                for (int x = 7; x <= 11; x++)
                                    if (!((y == 19 || y == 23) && (x == 7 || x == 11)))
                                        spr[x, y] = (x <= 9 && y <= 21) ? zkHi : zkMd;
                            spr[8, 21] = bldHi;
                        }
                    } else if (type == 17) {
                        // ====================================================================
                        // QUAD-COPTER DRONE: Mid-Air Explosion -> Spiraling Crash -> Wreckage
                        // ====================================================================
                        Color odHi  = Color.FromArgb(106, 168, 72);
                        Color odMd  = Color.FromArgb(72, 124, 48);
                        Color odSh  = Color.FromArgb(44, 78, 32);
                        Color stHi  = Color.FromArgb(192, 202, 216);
                        Color stSh  = Color.FromArgb(88, 96, 108);
                        Color cyHi  = Color.FromArgb(88, 250, 255);
                        Color spYel = Color.FromArgb(255, 228, 64);
                        Color spOrg = Color.FromArgb(242, 118, 28);
                        Color scorch= Color.FromArgb(36, 40, 48);

                        if (stg == 0) {
                            // Stage 0: Mid-air electrical/fire explosion, X-frame arms & rotors flying apart!
                            for (int s = -7; s <= 7; s++) {
                                spr[16 + s, 13 + s / 2] = stSh;
                                spr[16 + s, 13 - s / 2] = stSh;
                            }
                            // 4 rotor discs (top-right rotor snapping off in sparks!)
                            for (int x = 5; x <= 11; x++) {
                                spr[x, 9]  = ((x & 1) == 0) ? cyHi : stHi;
                                spr[x, 17] = ((x & 1) == 0) ? stHi : cyHi;
                            }
                            for (int x = 21; x <= 27; x++) {
                                spr[x, 7]  = ((x & 1) == 0) ? spYel : stHi;
                                spr[x, 17] = ((x & 1) == 0) ? cyHi : stHi;
                            }
                            // Olive-drab drone fuselage + bright center explosion burst
                            for (int y = 10; y <= 16; y++)
                                for (int x = 11; x <= 20; x++)
                                    spr[x, y] = (y <= 12) ? odHi : (y <= 14 ? odMd : odSh);
                            for (int y = 11; y <= 15; y++)
                                for (int x = 13; x <= 18; x++)
                                    if ((x - 15) * (x - 15) + (y - 13) * (y - 13) <= 5)
                                        spr[x, y] = (x == 15 && y == 13) ? Color.White : (((x + y) & 1) == 0 ? spYel : spOrg);
                            // Under-slung rifle barrel
                            for (int x = 10; x <= 21; x++) spr[x, 18] = (x <= 12) ? spYel : stSh;
                        } else if (stg == 1) {
                            // Stage 1: Spiraling downward at steep angle trailing orange fire & smoke
                            for (int y = 8; y <= 14; y++)
                                for (int x = 13; x <= 19; x++)
                                    if (((x + y) & 1) == 0) spr[x, y] = (y >= 12) ? spOrg : stSh;
                            for (int s = -6; s <= 6; s++) {
                                spr[15 + s, 18 + s / 2] = stSh;
                                spr[15 + s, 18 - s / 3] = stSh;
                            }
                            for (int y = 15; y <= 21; y++)
                                for (int x = 11; x <= 19; x++)
                                    spr[x, y] = (x >= 14 && x <= 17 && y <= 18) ? spOrg : (y <= 17 ? odHi : odMd);
                            spr[15, 16] = spYel; spr[16, 17] = Color.White;
                            for (int x = 6; x <= 11; x++) spr[x, 15] = stHi;
                            for (int x = 19; x <= 24; x++) spr[x, 21] = stHi;
                        } else if (stg == 2) {
                            // Stage 2: Slamming into ground in a shower of sparks
                            for (int y = 19; y <= 25; y++)
                                for (int x = 9; x <= 23; x++)
                                    if (y >= 22 || (x >= 12 && x <= 20))
                                        spr[x, y] = ((x + y) % 4 == 0) ? spYel : (y <= 21 ? odHi : (y <= 23 ? odMd : odSh));
                            for (int x = 6; x <= 12; x++) spr[x, 24] = stHi;
                            for (int x = 19; x <= 25; x++) spr[x, 23] = stSh;
                            spr[13, 18] = spYel; spr[16, 17] = spOrg; spr[18, 19] = spYel;
                        } else {
                            // Stage 3: Persistent Prone Quad-Copter Wreckage (inside 32x16 floor diamond)
                            for (int y = 20; y <= 27; y++)
                                for (int x = 10; x <= 21; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 24)
                                        spr[x, y] = ((x + y) & 1) == 0 ? scorch : stSh;
                            // Bent X-frame struts & shattered rotor blades inside diamond
                            for (int i = -5; i <= 5; i++) {
                                spr[15 + i, 23 + i / 2] = stSh;
                                spr[15 + i, 24 - i / 2] = stHi;
                            }
                            for (int x = 8; x <= 12; x++) spr[x, 22] = ((x & 1) == 0) ? cyHi : stHi;
                            for (int x = 19; x <= 23; x++) spr[x, 25] = ((x & 1) == 0) ? stHi : cyHi;
                            // Cracked olive-drab central hull + broken cyan lens + detached rifle
                            for (int y = 21; y <= 25; y++)
                                for (int x = 12; x <= 18; x++)
                                    spr[x, y] = (x == 14 && y == 23) ? cyHi : (x == 16 && y == 22 ? spOrg : (y <= 22 ? odHi : odMd));
                            for (int x = 11; x <= 18; x++)
                                spr[x, 27] = (x == 11) ? spYel : stHi;
                        }
                    } else {
                        // ====================================================================
                        // UFO PLASMA DRONE: Elerium Core Overload -> Crash -> Saucer Wreckage
                        // ====================================================================
                        Color alHi   = Color.FromArgb(192, 206, 232);
                        Color alMd   = Color.FromArgb(142, 156, 186);
                        Color alSh   = Color.FromArgb(98, 72, 154);
                        Color cyHi   = Color.FromArgb(96, 252, 255);
                        Color cyMd   = Color.FromArgb(38, 134, 228);
                        Color elCore = Color.FromArgb(235, 255, 195);
                        Color elHi   = Color.FromArgb(142, 252, 56);
                        Color elDk   = Color.FromArgb(48, 158, 34);

                        if (stg == 0) {
                            // Stage 0: Mid-air Elerium core overload! Saucer pitching + green plasma arcs
                            for (int y = 9; y <= 17; y++) {
                                int hw = (y == 13) ? 10 : ((y == 12 || y == 14) ? 9 : ((y == 11 || y == 15) ? 7 : 4));
                                for (int x = 16 - hw; x <= 16 + hw; x++) {
                                    spr[x, y] = (y == 13) ? (((x & 1) == 0) ? elHi : cyHi) :
                                                (y < 13)  ? alHi : alSh;
                                }
                            }
                            // Erupting Elerium-115 plasma core dome
                            for (int y = 5; y <= 12; y++)
                                for (int x = 12; x <= 20; x++)
                                    if ((x - 16) * (x - 16) + (y - 9) * (y - 9) <= 10)
                                        spr[x, y] = ((x - 16) * (x - 16) + (y - 9) * (y - 9) <= 3) ? elCore :
                                                    (((x + y) & 1) == 0 ? elHi : cyHi);
                            // Radial plasma discharge sparks
                            spr[7, 7] = elHi; spr[25, 8] = cyHi; spr[9, 18] = elHi; spr[23, 18] = elHi;
                        } else if (stg == 1) {
                            // Stage 1: Dropping out of hover at 45-degree tilt, venting green plasma fire
                            for (int y = 8; y <= 14; y++)
                                for (int x = 13; x <= 19; x++)
                                    if (((x + y) & 1) == 0) spr[x, y] = (y <= 10) ? elDk : elHi;
                            for (int i = -8; i <= 8; i++) {
                                int cx = 16 + i;
                                int cy = 17 + i / 2;
                                for (int dy = -2; dy <= 2; dy++) {
                                    if (Math.Abs(i) <= 6 || Math.Abs(dy) <= 1) {
                                        spr[cx, cy + dy] = (dy == 0) ? (((i & 1) == 0) ? elHi : cyHi) :
                                                           (dy < 0 ? alHi : alSh);
                                    }
                                }
                            }
                            spr[15, 15] = elCore; spr[16, 15] = elHi; spr[17, 16] = cyHi;
                        } else if (stg == 2) {
                            // Stage 2: Impacting ground in a burst of green Elerium sparks
                            for (int y = 18; y <= 25; y++) {
                                int hw = (y == 22) ? 10 : ((y == 21 || y == 23) ? 8 : 5);
                                for (int x = 16 - hw; x <= 16 + hw; x++) {
                                    spr[x, y] = (y == 22) ? (((x & 1) == 0) ? elHi : cyMd) :
                                                (y < 22 ? alHi : alSh);
                                }
                            }
                            spr[14, 19] = elCore; spr[15, 18] = elHi; spr[17, 19] = cyHi;
                            spr[9, 19] = elHi; spr[22, 18] = elHi;
                        } else {
                            // Stage 3: Persistent Prone UFO Drone Wreckage (inside 32x16 floor diamond)
                            for (int y = 20; y <= 28; y++)
                                for (int x = 9; x <= 22; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 28)
                                        spr[x, y] = ((x + y) & 1) == 0 ? elDk : elHi;
                            // Fractured pearlescent/violet alloy saucer disc embedded on tile
                            for (int y = 20; y <= 26; y++) {
                                int hw = (y == 23) ? 8 : ((y == 22 || y == 24) ? 7 : ((y == 21 || y == 25) ? 5 : 3));
                                for (int x = 15 - hw; x <= 15 + hw; x++) {
                                    if (x == 16 && y <= 22) continue; // Cracked hull fissure
                                    spr[x, y] = (y == 23) ? (((x & 1) == 0) ? cyHi : elHi) :
                                                (y < 23)  ? ((x < 15) ? alHi : alMd) : alSh;
                                }
                            }
                            // Shattered cyan/Elerium core dome in center
                            for (int y = 21; y <= 23; y++)
                                for (int x = 13; x <= 17; x++)
                                    spr[x, y] = (x == 15 && y == 22) ? elCore : ((x + y) % 2 == 0 ? cyHi : cyMd);
                        }
                    }

                    // For Stage 3 (persistent corpse/wreckage), clip strictly inside the 32x16 floor diamond
                    if (stg == 3) {
                        for (int py = 0; py < 32; py++) {
                            for (int px = 0; px < 32; px++) {
                                if (py < 17 || py > 30 || !InDiamond(px, py - 16)) {
                                    spr[px, py] = Trans;
                                }
                            }
                        }
                    }

                    // Apply 1px dark silhouette outline around non-transparent pixels!
                    for (int py = 1; py < 31; py++) {
                        for (int px = 1; px < 31; px++) {
                            if (stg == 3 && (py < 16 || !InDiamond(px, py - 16))) continue;
                            if (spr[px, py] != Trans) {
                                PSet(b, ox + px, oy + py, spr[px, py]);
                            } else {
                                if (spr[px - 1, py] != Trans || spr[px + 1, py] != Trans ||
                                    spr[px, py - 1] != Trans || spr[px, py + 1] != Trans) {
                                    PSet(b, ox + px, oy + py, Color.FromArgb(16, 18, 22));
                                }
                            }
                        }
                    }
                }
            }
            SaveBmp24(b, path);
        }
    }

    static void ExtendPortraitsBmp(string path) {
        if (!File.Exists(path)) return;
        Bitmap oldBmp;
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read)) {
            oldBmp = new Bitmap(fs);
        }
        int newW = 14 * 32;
        using (Bitmap b = new Bitmap(newW, 36)) {
            CopyPixels(oldBmp, b, 12 * 32, 36, Color.FromArgb(16, 26, 52));
            oldBmp.Dispose();

            for (int p = 0; p < 2; p++) {
                int ox = (12 + p) * 32;
                for (int y = 0; y < 36; y++) {
                    for (int x = 0; x < 32; x++) {
                        Color bg = ((y & 1) != 0) ? Color.FromArgb(16, 26, 52) : Color.FromArgb(12, 20, 42);
                        if ((x % 8 == 0) || (y % 8 == 0)) bg = Color.FromArgb(22, 38, 72);
                        PSet(b, ox + x, y, bg);
                    }
                }
                if (p == 0) {
                    // Slot 12: ARV Quad-Copter Recon Drone w/ 4 Spinning Rotors, Optical Gimbal & Assault Rifle
                    for (int s = 0; s < 10; s++) {
                        PSet(b, ox + 5  + s, 8  + s / 2, Color.FromArgb(88, 96, 108));
                        PSet(b, ox + 26 - s, 8  + s / 2, Color.FromArgb(88, 96, 108));
                        PSet(b, ox + 4  + s, 18 - s / 2, Color.FromArgb(68, 76, 88));
                        PSet(b, ox + 27 - s, 18 - s / 2, Color.FromArgb(68, 76, 88));
                    }
                    for (int x = 2; x <= 11; x++) {
                        PSet(b, ox + x, 6, ((x & 1) != 0) ? Color.FromArgb(72, 244, 255) : Color.FromArgb(204, 214, 226));
                        PSet(b, ox + 31 - x, 7, ((x & 1) != 0) ? Color.FromArgb(204, 214, 226) : Color.FromArgb(72, 244, 255));
                    }
                    for (int x = 1; x <= 10; x++) {
                        PSet(b, ox + x, 16, ((x & 1) != 0) ? Color.FromArgb(88, 250, 255) : Color.FromArgb(182, 192, 206));
                        PSet(b, ox + 31 - x, 16, ((x & 1) != 0) ? Color.FromArgb(182, 192, 206) : Color.FromArgb(88, 250, 255));
                    }
                    FillRect(b, ox + 6,  7,  2, 3, Color.FromArgb(52, 58, 68));
                    FillRect(b, ox + 24, 7,  2, 3, Color.FromArgb(52, 58, 68));
                    FillRect(b, ox + 5,  17, 2, 3, Color.FromArgb(52, 58, 68));
                    FillRect(b, ox + 25, 17, 2, 3, Color.FromArgb(52, 58, 68));
                    PSet(b, ox + 2,  6,  Color.FromArgb(72, 244, 255));
                    PSet(b, ox + 29, 7,  Color.FromArgb(72, 244, 255));
                    PSet(b, ox + 1,  16, Color.FromArgb(242, 48, 42));
                    PSet(b, ox + 30, 16, Color.FromArgb(92, 238, 64));

                    for (int y = 3; y <= 9; y++)
                        PSet(b, ox + 16, y, (y == 3) ? Color.FromArgb(255, 228, 64) : Color.FromArgb(148, 158, 172));

                    for (int y = 10; y <= 21; y++) {
                        int hw = (y <= 12 || y >= 20) ? 5 : 7;
                        for (int x = 16 - hw; x <= 15 + hw; x++) {
                            Color fc = (y == 13) ? Color.FromArgb(238, 196, 48) :
                                       (y <= 12) ? Color.FromArgb(96, 152, 74) :
                                       (y <= 18) ? Color.FromArgb(68, 118, 52) : Color.FromArgb(46, 82, 36);
                            if (x == 16 - hw || x == 15 + hw || y == 10 || y == 21) fc = Color.FromArgb(22, 26, 32);
                            PSet(b, ox + x, y, fc);
                        }
                    }
                    for (int y = 15; y <= 19; y++)
                        for (int x = 13; x <= 18; x++)
                            PSet(b, ox + x, y, (x == 13 || x == 18 || y == 15 || y == 19) ? Color.FromArgb(32, 36, 44) : Color.FromArgb(48, 212, 242));
                    PSet(b, ox + 14, 16, Color.White);
                    PSet(b, ox + 15, 16, Color.FromArgb(96, 252, 255));
                    PSet(b, ox + 16, 17, Color.FromArgb(255, 228, 64));

                    for (int y = 22; y <= 30; y++) {
                        PSet(b, ox + 9,  y, Color.FromArgb(62, 68, 78));
                        PSet(b, ox + 22, y, Color.FromArgb(62, 68, 78));
                    }
                    FillRect(b, ox + 7,  30, 5, 1, Color.FromArgb(98, 106, 118));
                    FillRect(b, ox + 20, 30, 5, 1, Color.FromArgb(98, 106, 118));
                    FillRect(b, ox + 12, 22, 8, 5, Color.FromArgb(108, 116, 128));
                    for (int x = 4; x <= 20; x++) {
                        PSet(b, ox + x, 25, (x <= 5) ? Color.FromArgb(255, 228, 64) : Color.FromArgb(164, 174, 188));
                        PSet(b, ox + x, 26, (x <= 5) ? Color.FromArgb(212, 162, 42) : Color.FromArgb(92, 100, 112));
                    }
                    FillRect(b, ox + 15, 26, 4, 5, Color.FromArgb(54, 92, 44));
                } else {
                    // Slot 13: ARV Alloy UFO Hover-Drone w/ Elerium Rim & Heavy Plasma Emitter
                    for (int y = 6; y <= 11; y++) {
                        int hw = (y <= 7) ? 3 : (y <= 9 ? 5 : 6);
                        for (int x = 16 - hw; x <= 15 + hw; x++) {
                            Color dc = (x == 16 - hw || x == 15 + hw || y == 6) ? Color.FromArgb(22, 26, 34) :
                                       (y <= 8 && x >= 13 && x <= 16) ? Color.FromArgb(96, 252, 255) : Color.FromArgb(38, 134, 228);
                            PSet(b, ox + x, y, dc);
                        }
                    }
                    PSet(b, ox + 14, 7, Color.White);
                    PSet(b, ox + 15, 7, Color.FromArgb(96, 252, 255));

                    for (int y = 11; y <= 21; y++) {
                        int hw = (y == 15 || y == 16) ? 14 : ((y == 14 || y == 17) ? 12 : ((y == 13 || y == 18) ? 10 : 7));
                        for (int x = 16 - hw; x <= 15 + hw; x++) {
                            Color sc;
                            if (x == 16 - hw || x == 15 + hw || y == 21) sc = Color.FromArgb(20, 22, 30);
                            else if (y == 15 || y == 16) sc = (((x + y) & 1) != 0) ? Color.FromArgb(142, 252, 56) : Color.FromArgb(72, 244, 255);
                            else if (y < 15) sc = ((x % 5) == 0) ? Color.FromArgb(108, 84, 168) : Color.FromArgb(158, 174, 198);
                            else sc = Color.FromArgb(92, 68, 148);
                            PSet(b, ox + x, y, sc);
                        }
                    }
                    for (int y = 21; y <= 26; y++)
                        for (int x = 11; x <= 20; x++)
                            PSet(b, ox + x, y, (x == 11 || x == 20) ? Color.FromArgb(22, 24, 32) :
                                               (y == 23 ? Color.FromArgb(142, 252, 56) : Color.FromArgb(118, 88, 176)));
                    for (int x = 5; x <= 15; x++) {
                        PSet(b, ox + x, 25, (x <= 7) ? Color.FromArgb(142, 252, 56) : Color.FromArgb(148, 162, 188));
                        PSet(b, ox + x, 26, (x <= 7) ? Color.White : Color.FromArgb(124, 92, 182));
                        PSet(b, ox + x, 27, (x <= 7) ? Color.FromArgb(88, 228, 46) : Color.FromArgb(26, 28, 36));
                    }
                    for (int x = 10; x <= 21; x++) if ((x & 1) == 0) PSet(b, ox + x, 30, Color.FromArgb(72, 244, 255));
                    for (int x = 12; x <= 19; x++) if ((x & 1) != 0) PSet(b, ox + x, 32, Color.FromArgb(142, 252, 56));
                }
            }
            SaveBmp24(b, path);
        }
    }
}
"@

Add-Type -TypeDefinition $csharp -ReferencedAssemblies System.Drawing
[DuffNewAssetGen]::GenAll((Resolve-Path "assets").Path)
Write-Output "Generated MODAL.BMP, ARV_TURR.BMP, and extended FLOORS.BMP, UNITS.BMP, DEATH.BMP in assets/!"
