param([switch]$ObjectsOnly)
Add-Type -AssemblyName System.Drawing

$csharp = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

public class DuffArtGen {
    static Color Trans = Color.FromArgb(255, 255, 0, 255); // Hot Magenta key

    // Save 24-bit uncompressed BMP so DOS bmp_loader reads it directly
    static void SaveBmp24(Bitmap bmp, string path) {
        using (Bitmap b24 = new Bitmap(bmp.Width, bmp.Height, PixelFormat.Format24bppRgb)) {
            using (Graphics g = Graphics.FromImage(b24)) {
                g.Clear(Trans);
                g.DrawImage(bmp, 0, 0, bmp.Width, bmp.Height);
            }
            b24.Save(path, ImageFormat.Bmp);
        }
    }

    static bool InDiamond(int x, int y) {
        int dx = (x < 16) ? (15 - x) : (x - 16);
        int dy = (y < 8)  ? (7 - y)  : (y - 8);
        return (dx + dy * 2 <= 15);
    }

    static void PSet(Bitmap b, int x, int y, Color c) {
        if (x >= 0 && x < b.Width && y >= 0 && y < b.Height) b.SetPixel(x, y, c);
    }

    // Draw a true 3D isometric box centered around (cx, baseY)
    // rx = extent down-right (+x,+y), lx = extent down-left (-x,+y), h = vertical height
    static void DrawIsoBox(Bitmap b, int ox, int cx, int baseY, int rx, int lx, int h,
                           Color topCol, Color leftCol, Color rightCol, bool outline) {
        // Left face (spans cx - lx .. cx)
        for (int i = 0; i <= lx; i++) {
            int x = ox + cx - i;
            int yBot = baseY - (i / 2);
            int yTop = yBot - h;
            for (int y = yTop; y <= yBot; y++) {
                bool edge = outline && (i == lx || y == yTop || y == yBot);
                PSet(b, x, y, edge ? Color.FromArgb(18, 20, 24) : leftCol);
            }
        }
        // Right face (spans cx .. cx + rx)
        for (int i = 0; i <= rx; i++) {
            int x = ox + cx + i;
            int yBot = baseY - (i / 2);
            int yTop = yBot - h;
            for (int y = yTop; y <= yBot; y++) {
                bool edge = outline && (i == rx || y == yTop || y == yBot || i == 0);
                PSet(b, x, y, edge ? Color.FromArgb(18, 20, 24) : rightCol);
            }
        }
        // Top diamond face (solid 2:1 isometric fill with zero integer-division gaps)
        int topCenterY = baseY - h;
        for (int u = 0; u <= lx; u++) {
            for (int v = 0; v <= rx; v++) {
                int x = ox + cx - u + v;
                int y = topCenterY - ((u + v) / 2);
                bool edge = outline && (u == lx || v == rx);
                Color tc = edge ? Color.FromArgb(24, 28, 32) : topCol;
                PSet(b, x, y, tc);
                if (u < lx && v < rx) PSet(b, x, y + 1, topCol);
            }
        }
    }

    public static void GenerateAll(string assetsDir) {
        Directory.CreateDirectory(assetsDir);
        GenFloors(Path.Combine(assetsDir, "FLOORS.BMP"));
        GenWalls(Path.Combine(assetsDir, "WALLS_W.BMP"), Path.Combine(assetsDir, "WALLS_N.BMP"));
        GenObjects(Path.Combine(assetsDir, "OBJECTS.BMP"));
        GenUnits(Path.Combine(assetsDir, "UNITS.BMP"));
        GenDeaths(Path.Combine(assetsDir, "DEATH.BMP"));
        GenFx(Path.Combine(assetsDir, "FX.BMP"));
        GenPortraits(Path.Combine(assetsDir, "PORTRAIT.BMP"));
        GenRadar(Path.Combine(assetsDir, "RADAR.BMP"));
    }

    static Color UfoHullShade(int z, double light, int arcHash, bool damaged, int x, int y) {
        // Vertical convex C-curve saucer profile (z = 0 bottom underbelly .. 23 upper dome rim)
        // light in [0.0 .. 1.0]: 1.0 = sunlit West-facing normal, 0.25 = shaded North-facing normal
        double L = 0.62 + 0.38 * light;
        Color c;
        if (z == 23) {
            int r = (int)(210 * L), g = (int)(222 * L), b = (int)(248 * L);
            return Color.FromArgb(Math.Min(255, r), Math.Min(255, g), Math.Min(255, b));
        }
        if (z >= 11 && z <= 13) {
            // Recessed saucer equator channel + glowing Neon Cyan / Elerium plasma field trim!
            if (z == 12) {
                if (damaged && x >= 5 && x <= 10) {
                    return (x == 7 || x == 8) ? Color.FromArgb(255, 228, 55) : Color.FromArgb(34, 24, 48);
                }
                bool node = (arcHash % 6 == 0);
                return node ? Color.FromArgb(168, 252, 56) : Color.FromArgb(76, 248, 252);
            }
            return Color.FromArgb((int)(46 * L), (int)(40 * L), (int)(86 * L));
        }
        if (z >= 14) {
            // Upper convex saucer dome slope: pearlescent blue-silver -> lavender-titanium
            double t = (z - 14) / 8.0; // 0 at equator, 1 at top rim
            double r = (138 + 68 * t) * L;
            double g = (128 + 84 * t) * L;
            double b = (188 + 58 * t) * L;
            if (arcHash == 0) { r *= 0.76; g *= 0.76; b *= 0.82; } // Curved C-shaped hull rib seam!
            c = Color.FromArgb(Math.Min(255, (int)r), Math.Min(255, (int)g), Math.Min(255, (int)b));
        } else {
            // Lower tucked saucer underbelly: curves sharply into deep alien violet-indigo shadow!
            double t = z / 10.0; // 0 at base, 1 just below equator
            double curve = t * t;
            double r = (36 + 96 * curve) * L;
            double g = (18 + 78 * curve) * L;
            double b = (56 + 122 * curve) * L;
            if (arcHash == 0) { r *= 0.78; g *= 0.78; b *= 0.84; }
            c = Color.FromArgb(Math.Min(255, (int)r), Math.Min(255, (int)g), Math.Min(255, (int)b));
        }
        if (damaged) {
            if (x >= 5 && x <= 10 && z >= 9 && z <= 15) {
                bool spark = (x == 8 && z == 12) || (x == 6 && z == 11);
                return spark ? Color.FromArgb(165, 255, 60) : Color.FromArgb(28, 20, 38);
            }
            if (x + z == 19 || x - z == -5 || (x >= 4 && x <= 11 && z >= 8 && z <= 16 && ((x + z) & 1) == 0)) {
                return Color.FromArgb(48, 34, 66);
            }
        }
        return c;
    }

    static Color UfoDeckPixel(int x, int y) {
        int u = (x - 16) + (y - 8) * 2 + 64;
        int v = (x - 16) - (y - 8) * 2 + 64;
        int mu = u % 12, mv = v % 12;
        if (mu == 0 || mv == 0) {
            // Recessed glowing cyan alien power conduit grid!
            bool pulse = ((u + v) % 24 == 0);
            return pulse ? Color.FromArgb(72, 236, 248) : Color.FromArgb(28, 124, 148);
        }
        if (mu == 1 || mv == 1) {
            return Color.FromArgb(118, 102, 158); // Beveled alloy plate highlight
        }
        int n = ((x * 19 + y * 31) >> 1) & 3;
        return Color.FromArgb(62 + n * 4, 46 + n * 3, 94 + n * 5);
    }

    static Color UfoRoofPixel(int x, int y, double rCorner, bool isCorner) {
        if (isCorner && rCorner > 15.25) return Trans;
        if (isCorner && rCorner >= 12.8) {
            // Continuous rounded saucer upper rim + glowing cyan trim line!
            if (rCorner >= 14.3) return Color.FromArgb(196, 210, 240);
            if (rCorner >= 13.4) return Color.FromArgb(72, 242, 250);
            return Color.FromArgb(96, 78, 138);
        }
        int u = (x - 16) + (y - 8) * 2 + 64;
        int v = (x - 16) - (y - 8) * 2 + 64;
        bool seam = (u % 16 == 0) || (v % 16 == 0);
        bool node = (u % 16 == 8) && (v % 16 == 8);
        if (node) return Color.FromArgb(88, 248, 252);
        if (seam) return Color.FromArgb(98, 86, 138);
        int dist = Math.Abs(x - 16) + Math.Abs(y - 8) * 2;
        int lift = Math.Max(0, 12 - dist) * 2;
        return Color.FromArgb(146 + lift, 142 + lift, 194 + lift);
    }

    static void GenFloors(string path) {
        // 16 floor tiles x 32x16 = 512x16 (includes UFO interior deck & rounded saucer dome roof caps!)
        using (Bitmap bmp = new Bitmap(16 * 32, 16)) {
            using (Graphics g = Graphics.FromImage(bmp)) g.Clear(Trans);
            Random rng = new Random(1994);

            for (int f = 1; f < 16; f++) {
                int ox = f * 32;
                for (int y = 0; y < 16; y++) {
                    for (int x = 0; x < 32; x++) {
                        if (!InDiamond(x, y)) continue;
                        int n = (((x * 37 + y * 57) ^ ((x + 5) * (y + 11) * 19)) >> 1) & 15;
                        Color c = Color.Gray;

                        switch (f) {
                            case 1: // FLOOR_GRASS: Rich seamless PA meadow grass with blades & clover
                                if (n == 0 || n == 7) c = Color.FromArgb(88, 178, 62);
                                else if (n < 5)       c = Color.FromArgb(66, 148, 48);
                                else if (n < 11)      c = Color.FromArgb(54, 128, 38);
                                else if (n == 14)     c = Color.FromArgb(104, 192, 72);
                                else                  c = Color.FromArgb(44, 108, 32);
                                break;

                            case 2: // FLOOR_ASPHALT: Seamless worn Route 309 highway tarmac
                            case 3: // FLOOR_ASPHALT_LINE: Double yellow highway stripe
                                int wear = Math.Abs((x - 16) + (y - 8) * 2);
                                int shade = 58 + (n & 7) - (wear < 8 ? 6 : 0);
                                c = Color.FromArgb(shade, shade, shade + 8);
                                if (f == 3) {
                                    int centerDist = (x - 16) + (y - 8) * 2;
                                    if ((centerDist == -2 || centerDist == -1 || centerDist == 2 || centerDist == 3) && ((x - y) & 7) != 0) {
                                        c = Color.FromArgb(245, 210, 42); // Crisp double-yellow PA highway line!
                                    }
                                }
                                break;

                            case 4: // FLOOR_SIDEWALK: Isometric concrete flagstones with beveled joints
                                int iu = (x - 16) + (y - 8) * 2 + 32;
                                int iv = (x - 16) - (y - 8) * 2 + 32;
                                if ((iu % 16 == 0) || (iv % 16 == 0)) c = Color.FromArgb(115, 112, 106);
                                else if ((iu % 16 == 1) || (iv % 16 == 1)) c = Color.FromArgb(208, 202, 190);
                                else {
                                    int v = 172 + (n & 7);
                                    c = Color.FromArgb(v + 6, v + 2, v - 4);
                                }
                                break;

                            case 5: // FLOOR_DINER_TILE: Classic glossy black & cream checkerboard with sheen
                                int du = (x - 16) + (y - 8) * 2 + 64;
                                int dv = (x - 16) - (y - 8) * 2 + 64;
                                bool white = (((du >> 3) + (dv >> 3)) & 1) == 0;
                                bool sheen = ((x + y * 2) % 11 == 0);
                                if (white) c = sheen ? Color.FromArgb(252, 252, 255) : Color.FromArgb(224, 228, 232);
                                else       c = sheen ? Color.FromArgb(62, 78, 92)    : Color.FromArgb(28, 32, 40);
                                break;

                            case 6: // FLOOR_ARCADE_CARPET: 90s blacklight arcade carpet with neon flecks!
                                int cn = Math.Abs((x * 41 + y * 67) ^ ((x + 3) * (y + 13) * 23)) % 41;
                                c = ((x + y) & 1) == 0 ? Color.FromArgb(46, 22, 68) : Color.FromArgb(36, 18, 56);
                                if (cn == 1)       c = Color.FromArgb(40, 235, 245); // Neon cyan triangle
                                else if (cn == 11) c = Color.FromArgb(245, 130, 35); // Neon orange confetti
                                else if (cn == 23) c = Color.FromArgb(235, 65, 200); // Neon magenta fleck
                                else if (cn == 34) c = Color.FromArgb(245, 225, 50); // Neon yellow star
                                break;

                            case 7: // FLOOR_WOOD: Warm oak floorboards with wood grain & nail heads
                                int wu = (x - 16) + (y - 8) * 2 + 64;
                                int wv = (x - 16) - (y - 8) * 2 + 64;
                                if (wu % 8 == 0) c = Color.FromArgb(82, 52, 26); // Plank gap
                                else {
                                    int grain = ((wv * 3 + wu) & 7) * 3;
                                    c = Color.FromArgb(158 + grain, 108 + grain / 2, 58);
                                    if ((wv % 16 == 2) && (wu % 8 == 2 || wu % 8 == 6))
                                        c = Color.FromArgb(62, 40, 20); // Nail head
                                }
                                break;

                            case 8: // FLOOR_ROOF_TAR: Gravel tar roof with seam lines
                                int rv = 78 + (n & 7) * 3;
                                if (((x + y * 2) % 15) == 0) rv -= 22;
                                c = Color.FromArgb(rv, rv + 2, rv + 6);
                                break;

                            case 9: // FLOOR_SCORCHED: Burned crater earth with glowing Elerium embers
                                int dist = Math.Abs(x - 16) + Math.Abs(y - 8) * 2;
                                int sv = dist < 10 ? 26 : 44 + (n & 7);
                                c = Color.FromArgb(sv + 6, sv, sv - 2);
                                if (dist < 10 && n == 3) c = Color.FromArgb(140, 245, 45); // Glowing green Elerium!
                                break;

                            case 10: // FLOOR_UFO_DECK: Interlocking alien violet-alloy deck w/ cyan conduits
                                c = UfoDeckPixel(x, y);
                                break;

                            case 11: // FLOOR_UFO_ROOF: Domed alien saucer upper hull plating
                                c = UfoRoofPixel(x, y, 0.0, false);
                                break;

                            case 12: // FLOOR_UFO_ROOF_N: Rounded saucer roof North corner cap
                            case 13: // FLOOR_UFO_ROOF_E: Rounded saucer roof East corner cap
                            case 14: // FLOOR_UFO_ROOF_S: Rounded saucer roof South corner cap
                            case 15: // FLOOR_UFO_ROOF_W: Rounded saucer roof West corner cap
                                double uf = (x + 2.0 * y - 15.0) * 0.5;
                                double vf = (2.0 * y - x + 16.0) * 0.5;
                                double pu = (f == 12 || f == 15) ? 15.0 : 0.0;
                                double pv = (f == 12 || f == 13) ? 15.0 : 0.0;
                                double rc = Math.Sqrt((uf - pu) * (uf - pu) + (vf - pv) * (vf - pv));
                                c = UfoRoofPixel(x, y, rc, true);
                                break;
                        }
                        if (c != Trans) PSet(bmp, ox + x, y, c);
                    }
                }
            }
            SaveBmp24(bmp, path);
        }
    }

    static void GenWalls(string westPath, string northPath) {
        // 21 wall types x 16x32 = 336x32 (includes UFO convex saucer hull & alien oval airlocks!)
        using (Bitmap bmpW = new Bitmap(21 * 16, 32))
        using (Bitmap bmpN = new Bitmap(21 * 16, 32)) {
            using (Graphics g = Graphics.FromImage(bmpW)) g.Clear(Trans);
            using (Graphics g = Graphics.FromImage(bmpN)) g.Clear(Trans);

            for (int w = 1; w < 21; w++) {
                int ox = w * 16;
                for (int x = 0; x < 16; x++) {
                    int topW = (15 - x) / 2;
                    int topN = x / 2;
                    bool isFence = (w == 9 || w == 16);
                    bool isUfo   = (w >= 17 && w <= 20);
                    int wallH = isFence ? 13 : 24;
                    int yStart = isFence ? (24 - 13) : 0;

                    for (int y = 0; y < wallH; y++) {
                        int wyW = topW + yStart + y;
                        int wyN = topN + yStart + y;
                        Color cw = Trans, cn = Trans;

                        switch (w) {
                            case 1:  // WALL_BRICK
                            case 2:  // WALL_BRICK_WIN
                            case 10: // WALL_BRICK_BROKEN (shattered window opening)
                            case 12: // WALL_BRICK_DMG (cracked & cratered masonry)
                                bool hMortar = (y % 4 == 0);
                                bool vMortar = ((x + (y / 4) * 3) % 6 == 0);
                                if (hMortar || vMortar) {
                                    cw = Color.FromArgb(168, 160, 150);
                                    cn = Color.FromArgb(120, 114, 108);
                                } else {
                                    int bv = ((x * 5 + y * 3) & 3) * 8;
                                    cw = Color.FromArgb(186 + bv, 62 + bv / 3, 48 + bv / 3);
                                    cn = Color.FromArgb(136 + bv, 42 + bv / 3, 34 + bv / 3);
                                }
                                if ((w == 2 || w == 10) && x >= 3 && x <= 12 && y >= 5 && y <= 16) {
                                    if (y == 16) { // Limestone window sill!
                                        cw = Color.FromArgb(220, 215, 200);
                                        cn = Color.FromArgb(170, 165, 152);
                                    } else if (x == 3 || x == 12 || y == 5 || (w == 2 && x == 7)) { // White muntin frame
                                        cw = Color.FromArgb(230, 232, 235);
                                        cn = Color.FromArgb(175, 180, 188);
                                    } else if (w == 10) { // Shattered glass jagged shards + open hole!
                                        bool shard = ((x == 4 || x == 11) && ((x + y) & 1) == 0) ||
                                                     ((y == 6 || y == 15) && ((x * 3 + y) % 3 == 0));
                                        cw = shard ? Color.FromArgb(185, 248, 255) : Trans;
                                        cn = shard ? Color.FromArgb(135, 205, 225) : Trans;
                                    } else { // Reflective window glass
                                        bool glint = ((x + y) % 5 == 0);
                                        cw = glint ? Color.FromArgb(165, 240, 250) : Color.FromArgb(52, 98, 138);
                                        cn = glint ? Color.FromArgb(125, 200, 220) : Color.FromArgb(36, 72, 106);
                                    }
                                } else if (w == 12) {
                                    if (y < 4 && x >= 5 && x <= 10 && (y + (x & 1) < 3)) {
                                        cw = Trans; cn = Trans; // Blown-out top brickwork notch
                                    } else if (x >= 7 && x <= 9 && y >= 10 && y <= 13) {
                                        cw = Trans; cn = Trans; // Shell hole right through bricks!
                                    } else if (x + y == 16 || x + y == 17 || x - y == -5 ||
                                               (x >= 6 && x <= 10 && y >= 9 && y <= 14)) {
                                        cw = Color.FromArgb(46, 38, 36); // Soot-darkened blast cracks
                                        cn = Color.FromArgb(32, 26, 26);
                                    }
                                }
                                break;

                            case 3:  // WALL_DINER_CHROME
                            case 4:  // WALL_DINER_GLASS
                            case 11: // WALL_DINER_BROKEN (shattered diner glass & broken neon)
                            case 13: // WALL_DINER_DMG (buckled & scorched chrome siding)
                                if (y == 1 || y == 2) { // Glowing Neon Cyan & Pink Diner Roof Tube!
                                    if ((w == 11 || w == 13) && x >= 5 && x <= 11) {
                                        cw = (x == 8 && y == 1) ? Color.FromArgb(255, 230, 60) : Color.FromArgb(48, 52, 60);
                                        cn = (x == 8 && y == 1) ? Color.FromArgb(220, 180, 40) : Color.FromArgb(34, 38, 44);
                                    } else {
                                        cw = (y == 1) ? Color.FromArgb(80, 250, 255) : Color.FromArgb(245, 70, 185);
                                        cn = (y == 1) ? Color.FromArgb(55, 210, 225) : Color.FromArgb(195, 50, 148);
                                    }
                                } else if (y <= 4 || y >= 19) { // Fluted Brushed Stainless Steel
                                    int sv = (y & 1) == 0 ? 215 : 162;
                                    cw = Color.FromArgb(sv, sv + 6, sv + 14);
                                    cn = Color.FromArgb(sv - 40, sv - 34, sv - 24);
                                } else if ((w == 4 || w == 11) && x >= 2 && x <= 13 && y >= 6 && y <= 17) {
                                    if (x == 2 || x == 13 || y == 6 || y == 17) {
                                        cw = Color.FromArgb(228, 232, 240);
                                        cn = Color.FromArgb(172, 178, 188);
                                    } else if (w == 11) {
                                        bool shard = ((x == 3 || x == 12) && ((x + y) & 1) == 0) ||
                                                     ((y == 7 || y == 16) && ((x * 5 + y) % 3 == 0));
                                        cw = shard ? Color.FromArgb(175, 248, 255) : Trans;
                                        cn = shard ? Color.FromArgb(135, 210, 225) : Trans;
                                    } else {
                                        bool glint = ((x + y) % 4 == 0);
                                        cw = glint ? Color.FromArgb(170, 248, 255) : Color.FromArgb(48, 128, 152);
                                        cn = glint ? Color.FromArgb(135, 210, 225) : Color.FromArgb(36, 98, 120);
                                    }
                                } else { // Retro Red Porcelain Enamel Diner Cladding
                                    cw = (y % 3 == 0) ? Color.FromArgb(230, 52, 48) : Color.FromArgb(198, 36, 34);
                                    cn = (y % 3 == 0) ? Color.FromArgb(172, 36, 34) : Color.FromArgb(145, 24, 24);
                                    if (w == 13) {
                                        if (x >= 7 && x <= 9 && y >= 10 && y <= 13) {
                                            cw = Trans; cn = Trans;
                                        } else if (x - y == -4 || x + y == 19 ||
                                                   (x >= 6 && x <= 10 && y >= 9 && y <= 14)) {
                                            cw = Color.FromArgb(44, 46, 54);
                                            cn = Color.FromArgb(30, 32, 38);
                                        }
                                    }
                                }
                                break;

                            case 5:  // WALL_BARN_WOOD: Weathered PA Red Barn Planks + White Trim
                            case 14: // WALL_BARN_DMG: Splintered PA Barn Planks with Blown-Out Holes
                                if (y == 1 || y == wallH - 1 || x == 0 || x == 15) {
                                    cw = Color.FromArgb(232, 228, 218); // Classic white PA barn trim!
                                    cn = Color.FromArgb(178, 174, 165);
                                } else if (x % 4 == 0) {
                                    cw = Color.FromArgb(128, 24, 20); // Batten shadow seam
                                    cn = Color.FromArgb(92, 16, 14);
                                } else {
                                    cw = Color.FromArgb(188, 38, 32);
                                    cn = Color.FromArgb(142, 26, 22);
                                }
                                if (w == 14) {
                                    if ((y < 4 && x >= 5 && x <= 9) ||
                                        (x >= 5 && x <= 7 && y >= 9 && y <= 15) ||
                                        (x >= 10 && x <= 11 && y >= 5 && y <= 10)) {
                                        cw = Trans; cn = Trans;
                                    } else if ((x == 4 && y >= 9 && y <= 15) ||
                                               (x == 8 && y >= 9 && y <= 15) ||
                                               (y == 8 && x >= 5 && x <= 7) ||
                                               (y == 16 && x >= 5 && x <= 7)) {
                                        cw = Color.FromArgb(214, 168, 108); // Fresh raw wood splinters!
                                        cn = Color.FromArgb(165, 126, 78);
                                    }
                                }
                                break;

                            case 6:  // WALL_STEEL: Beveled Industrial Bulkhead with Hazard Conduit
                            case 15: // WALL_STEEL_DMG: Scorched Bulkhead with Ruptured Sparking Conduit
                                if (x == 0 || x == 15 || y == 2 || y == wallH - 2) {
                                    cw = Color.FromArgb(72, 82, 96);
                                    cn = Color.FromArgb(52, 60, 72);
                                } else if (y == 11 || y == 12) {
                                    cw = Color.FromArgb(135, 245, 50); // Glowing green conduit
                                    cn = Color.FromArgb(105, 205, 38);
                                } else {
                                    int mv = 126 + ((x & 3) * 8);
                                    cw = Color.FromArgb(mv - 6, mv, mv + 14);
                                    cn = Color.FromArgb(mv - 38, mv - 32, mv - 18);
                                }
                                if (w == 15) {
                                    if (x >= 6 && x <= 10 && y >= 10 && y <= 14) {
                                        bool spark = (x == 8 && y == 12) || (x == 7 && y == 11);
                                        cw = spark ? Color.FromArgb(255, 225, 55) : Color.FromArgb(34, 38, 44);
                                        cn = spark ? Color.FromArgb(235, 165, 35) : Color.FromArgb(24, 28, 34);
                                    } else if (x + y == 15 || x - y == -6) {
                                        cw = Color.FromArgb(46, 52, 60);
                                        cn = Color.FromArgb(32, 36, 44);
                                    }
                                }
                                break;

                            case 7: // WALL_DOOR_CLOSED: Heavy Tactical Sliding Door with Keypad
                                if (x <= 2 || x >= 13 || y <= 2) {
                                    cw = Color.FromArgb(135, 142, 152);
                                    cn = Color.FromArgb(98, 104, 114);
                                } else if (x == 8) {
                                    cw = Color.FromArgb(28, 32, 38); // Center sliding door seam
                                    cn = Color.FromArgb(20, 22, 28);
                                } else {
                                    cw = Color.FromArgb(74, 114, 168);
                                    cn = Color.FromArgb(54, 86, 132);
                                    if (x == 11 && (y == 11 || y == 12)) {
                                        cw = Color.FromArgb(250, 215, 45); // Lit access pad
                                        cn = Color.FromArgb(220, 185, 35);
                                    }
                                }
                                break;

                            case 8: // WALL_DOOR_OPEN: Open Doorway Frame
                                if (x <= 2 || x >= 13 || y <= 3) {
                                    cw = Color.FromArgb(135, 142, 152);
                                    cn = Color.FromArgb(98, 104, 114);
                                } else {
                                    cw = Trans; cn = Trans;
                                }
                                break;

                            case 9:  // WALL_FENCE: Isometric Wooden Picket / Rail Fence
                            case 16: // WALL_FENCE_DMG: Snapped & Splintered Fence
                                if (w == 16 && ((x >= 5 && x <= 10 && y < 7) || (x == 7 && y < 10))) {
                                    cw = Trans; cn = Trans;
                                } else if (x % 3 == 0 || y == 3 || y == 9) {
                                    cw = Color.FromArgb(196, 154, 102);
                                    cn = Color.FromArgb(148, 114, 72);
                                } else {
                                    cw = Trans; cn = Trans;
                                }
                                break;

                            case 17: // WALL_UFO_HULL: Rounded convex alien saucer hull!
                            case 18: // WALL_UFO_HULL_DMG: Battle-damaged alien saucer hull!
                                int z = 23 - y;
                                // Parabolic C-curve rib offset so flat segments look curved in 3D!
                                int cBowW = ((z - 12) * (z - 12)) / 36;
                                int cBowN = -cBowW;
                                int ribW = (x + cBowW + 16) % 8;
                                int ribN = (x + cBowN + 16) % 8;
                                cw = UfoHullShade(z, 1.0,  ribW, w == 18, x, y);
                                cn = UfoHullShade(z, 0.35, ribN, w == 18, x, y);
                                break;

                            case 19: // WALL_UFO_DOOR_CLOSED: Rounded oval alien iris containment airlock!
                            case 20: // WALL_UFO_DOOR_OPEN:   Open oval alien containment archway!
                                int zd = 23 - y;
                                // Elliptical portal equation centered at (7.5, 12.5), radii rx=5.2, ry=9.2
                                double dxp = (x - 7.5) / 5.2;
                                double dyp = (y - 12.5) / 9.2;
                                double ell = dxp * dxp + dyp * dyp;
                                if (ell > 1.0) {
                                    // Outer convex saucer hull surrounding the oval airlock
                                    cw = UfoHullShade(zd, 1.0,  (x + 2) % 8, false, x, y);
                                    cn = UfoHullShade(zd, 0.35, (x + 2) % 8, false, x, y);
                                } else if (ell >= 0.74) {
                                    // Glowing cyan/silver rounded oval containment ring!
                                    bool innerGlow = (ell < 0.86);
                                    cw = innerGlow ? Color.FromArgb(80, 248, 255) : Color.FromArgb(172, 184, 224);
                                    cn = innerGlow ? Color.FromArgb(52, 204, 224) : Color.FromArgb(128, 138, 182);
                                } else if (w == 20) {
                                    // Retracted iris: open curved alien portal!
                                    cw = Trans; cn = Trans;
                                } else {
                                    // Closed iris: curved interlocking violet-alloy blades + glowing core glyph!
                                    double core = dxp * dxp + dyp * dyp;
                                    if (core <= 0.10) {
                                        cw = Color.FromArgb(168, 255, 64); // Elerium iris lock glyph!
                                        cn = Color.FromArgb(136, 224, 48);
                                    } else if (core <= 0.22) {
                                        cw = Color.FromArgb(72, 244, 252);
                                        cn = Color.FromArgb(48, 198, 218);
                                    } else {
                                        bool seam = (Math.Abs(x - 7.5) < 0.6) || (Math.Abs((x - 7) + (y - 12) / 2) == 0);
                                        cw = seam ? Color.FromArgb(44, 30, 74) : Color.FromArgb(118, 86, 168);
                                        cn = seam ? Color.FromArgb(30, 20, 54) : Color.FromArgb(88, 62, 134);
                                    }
                                }
                                break;
                        }

                        // 3D sunlit wall cap at y==0 and dark ambient occlusion baseboard at y==wallH-1
                        if (!isUfo) {
                            if (y == 0 && cw != Trans) {
                                cw = Color.FromArgb(225, 225, 230);
                                cn = Color.FromArgb(185, 185, 192);
                            } else if (y == wallH - 1 && cw != Trans && !isFence) {
                                cw = Color.FromArgb(32, 32, 38);
                                cn = Color.FromArgb(22, 22, 28);
                            }
                        }

                        if (cw != Trans) PSet(bmpW, ox + x, wyW, cw);
                        if (cn != Trans) PSet(bmpN, ox + x, wyN, cn);
                    }
                }
            }
            SaveBmp24(bmpW, westPath);
            SaveBmp24(bmpN, northPath);
        }
    }

    // Material IDs for 2-Tile 3D Isometric Vehicles:
    // 1=Primary Body, 2=Secondary Accent, 3=Glass, 4=Tire, 5=Chrome/Silver,
    // 6=Headlight, 7=Red Light, 8=Blue Light, 9=Dark Trim/Grille, 10=Gold/Amber
    static byte[,,] BuildVehicleVoxels(int vehKind) {
        byte[,,] m = new byte[32, 16, 20];

        if (vehKind == 0 || vehKind == 1) {
            // 0 = Coopersburg Police Cruiser, 1 = 90s Civilian Sedan
            bool isPolice = (vehKind == 0);

            // Lower body shell (u = 2..29, v = 3..12) with sloped front hood & tapered trunk
            for (int u = 2; u <= 29; u++) {
                int topZ = (u == 2) ? 6 : (u <= 26) ? 7 : (u <= 28) ? 6 : 5;
                for (int v = 3; v <= 12; v++) {
                    for (int z = 2; z <= topZ; z++) {
                        bool doorZone = (u >= 11 && u <= 18);
                        m[u, v, z] = (isPolice && doorZone) ? (byte)2 : (byte)1;
                    }
                }
            }

            // Side rub-strip & door badge
            for (int u = 4; u <= 27; u++) {
                if (!isPolice && zCheck(3)) {
                    m[u, 3, 3] = 9;
                    m[u, 12, 3] = 9;
                }
            }
            if (isPolice) {
                m[14, 3, 5] = 10; m[15, 3, 5] = 10;
                m[14, 12, 5] = 10; m[15, 12, 5] = 10;
            }

            // Chrome front & rear bumpers + dark radiator grille + headlights & taillights
            for (int v = 3; v <= 12; v++) {
                m[2,  v, 2] = 5;
                m[29, v, 2] = 5;
                if (v >= 6 && v <= 9) {
                    m[29, v, 3] = 9;
                    m[29, v, 4] = 9;
                }
            }
            // Twin Headlights (front u=29) & Taillights (rear u=2)
            for (int v = 4; v <= 5; v++) {
                m[29, v, 4] = 6; m[29, 15 - v, 4] = 6;
                m[2,  v, 4] = 7; m[2,  15 - v, 4] = 7;
            }
            m[29, 3, 4] = 10; m[29, 12, 4] = 10; // Corner amber turn markers

            // Sloped Cabin, Panoramic Windshield, Side Windows & B-Pillar (u = 8..20, v = 4..11)
            for (int u = 8; u <= 20; u++) {
                int maxZ = (u == 8) ? 9 : (u == 9) ? 11 :
                           (u <= 17) ? 12 : (u == 18) ? 11 : (u == 19) ? 10 : 8;
                for (int v = 4; v <= 11; v++) {
                    for (int z = 8; z <= maxZ; z++) {
                        bool roofCap = (z == 12 && u >= 10 && u <= 17);
                        bool frontShield = (u >= 18 && v >= 5 && v <= 10);
                        bool rearGlass   = (u <= 9  && v >= 5 && v <= 10);
                        bool sideGlass   = ((v == 4 || v == 11) && u >= 10 && u <= 17 && u != 14 && z <= 11);
                        if (roofCap) {
                            m[u, v, z] = isPolice ? (byte)2 : (byte)1;
                        } else if (frontShield || rearGlass || sideGlass) {
                            m[u, v, z] = 3;
                        } else {
                            m[u, v, z] = isPolice ? (byte)2 : (byte)1;
                        }
                    }
                }
            }

            // Sedan tinted sunroof vs Police Cruiser Red/White/Blue Roof Lightbar!
            if (isPolice) {
                for (int u = 13; u <= 15; u++) {
                    for (int v = 4; v <= 11; v++) {
                        m[u, v, 13] = (v <= 6) ? (byte)7 : (v <= 8) ? (byte)5 : (byte)8;
                    }
                }
            } else {
                for (int u = 13; u <= 16; u++)
                    for (int v = 6; v <= 9; v++)
                        m[u, v, 12] = 9;
            }

            // Carve dark wheel arches & mount 4 rounded tires with silver hubcaps
            int[] axles = new int[] { 5, 21 };
            foreach (int au in axles) {
                for (int u = au; u <= au + 5; u++) {
                    for (int z = 2; z <= 5; z++) {
                        m[u, 3, z] = 9;
                        m[u, 12, z] = 9;
                    }
                }
                for (int u = au + 1; u <= au + 4; u++) {
                    for (int z = 0; z <= 4; z++) {
                        bool corner = ((u == au + 1 || u == au + 4) && (z == 0 || z == 4));
                        if (corner) continue;
                        bool hub = (u >= au + 2 && u <= au + 3 && z >= 1 && z <= 2);
                        byte wmat = hub ? (byte)5 : (byte)4;
                        m[u, 2, z] = wmat; m[u, 3, z] = wmat;
                        m[u, 13, z] = wmat; m[u, 12, z] = wmat;
                    }
                }
            }
        } else if (vehKind == 2) {
            // 2 = Heavy Utility / Farm Pickup Truck (with open hollow 3D cargo bed!)
            // Rear Open Cargo Bed (u = 2..13, v = 3..12, z = 2..8)
            for (int u = 2; u <= 13; u++) {
                for (int v = 3; v <= 12; v++) {
                    for (int z = 2; z <= 8; z++) {
                        bool bedWall = (u == 2 || v == 3 || v == 12);
                        if (z <= 4) {
                            m[u, v, z] = bedWall ? (byte)1 : (byte)9; // Dark recessed bed liner floor
                        } else if (bedWall) {
                            m[u, v, z] = 1;
                        }
                    }
                }
            }
            // Black steel rollbar / headache rack behind cab (u = 13)
            for (int v = 3; v <= 12; v++) {
                for (int z = 9; z <= 12; z++) {
                    if (v <= 4 || v >= 11 || z == 12) m[13, v, z] = 9;
                }
            }
            // High Truck Cab (u = 14..20, v = 3..12, z = 2..13) + Sloped Windshield (u = 21)
            for (int u = 14; u <= 21; u++) {
                int maxZ = (u == 21) ? 10 : 13;
                for (int v = 3; v <= 12; v++) {
                    for (int z = 2; z <= maxZ; z++) {
                        if (z <= 8) {
                            m[u, v, z] = 1;
                        } else if (v >= 4 && v <= 11) {
                            bool roof = (z == 13 && u <= 19);
                            bool wshield = (u >= 20 && v >= 5 && v <= 10);
                            bool sideWin = ((v == 4 || v == 11) && u >= 15 && u <= 19 && z <= 12);
                            m[u, v, z] = roof ? (byte)1 : (wshield || sideWin) ? (byte)3 : (byte)1;
                        }
                    }
                }
            }
            // Squared-off Pickup Engine Hood (u = 21..29, v = 3..12, z = 2..8)
            for (int u = 21; u <= 29; u++) {
                int topZ = (u >= 28) ? 7 : 8;
                for (int v = 3; v <= 12; v++) {
                    for (int z = 2; z <= topZ; z++) {
                        if (m[u, v, z] == 0) m[u, v, z] = 1;
                    }
                }
            }
            // Bold Chrome Bumpers, Grille, Headlights & Taillights
            for (int v = 3; v <= 12; v++) {
                m[2,  v, 2] = 5;
                m[29, v, 2] = 5;
                if (v >= 5 && v <= 10) {
                    m[29, v, 4] = 5;
                    m[29, v, 5] = 9;
                    m[29, v, 6] = 5;
                }
            }
            for (int v = 3; v <= 4; v++) {
                m[29, v, 5] = 6; m[29, 15 - v, 5] = 6;
                m[2,  v, 5] = 7; m[2,  15 - v, 5] = 7;
                m[2,  v, 6] = 7; m[2,  15 - v, 6] = 7;
            }

            // Tall 4x4 Work Tires & Wheel Wells
            int[] axles = new int[] { 5, 21 };
            foreach (int au in axles) {
                for (int u = au; u <= au + 5; u++) {
                    for (int z = 2; z <= 5; z++) {
                        m[u, 3, z] = 9;
                        m[u, 12, z] = 9;
                    }
                }
                for (int u = au + 1; u <= au + 4; u++) {
                    for (int z = 0; z <= 5; z++) {
                        bool corner = ((u == au + 1 || u == au + 4) && (z == 0 || z == 5));
                        if (corner) continue;
                        bool hub = (u >= au + 2 && u <= au + 3 && z >= 2 && z <= 3);
                        byte wmat = hub ? (byte)5 : (byte)4;
                        m[u, 2, z] = wmat; m[u, 3, z] = wmat;
                        m[u, 13, z] = wmat; m[u, 12, z] = wmat;
                    }
                }
            }
        } else {
            // 3 = Full 2-Tile Pennsylvania Farm Tractor (Green & Yellow!)
            // Rear Axle Chassis, Hitch & Operator Platform (u = 4..13, v = 4..11, z = 4..8)
            for (int v = 6; v <= 9; v++) { m[2, v, 4] = 9; m[3, v, 4] = 9; } // Drawbar hitch
            for (int u = 4; u <= 13; u++) {
                for (int v = 4; v <= 11; v++) {
                    for (int z = 4; z <= 8; z++) m[u, v, z] = 1;
                }
            }
            // Flared Green Rear Fenders curving over the giant rear wheels
            for (int u = 4; u <= 12; u++) {
                for (int v = 2; v <= 4; v++) { m[u, v, 9] = 1; m[u, 15 - v, 9] = 1; }
                if (u >= 5 && u <= 11) {
                    for (int v = 2; v <= 4; v++) { m[u, v, 10] = 1; m[u, 15 - v, 10] = 1; }
                }
            }
            // Operator Seat, Steering Column, Yellow ROPS Cab Frame & Green Canopy Roof
            for (int u = 6; u <= 9; u++)
                for (int v = 6; v <= 9; v++)
                    for (int z = 9; z <= (u <= 7 ? 12 : 10); z++) m[u, v, z] = 9;
            for (int z = 9; z <= 12; z++) { m[11, 7, z] = 9; m[11, 8, z] = 9; }
            for (int z = 9; z <= 15; z++) {
                m[5,  4, z] = 2; m[5,  11, z] = 2;
                m[12, 4, z] = 2; m[12, 11, z] = 2;
            }
            for (int u = 5; u <= 12; u++)
                for (int v = 4; v <= 11; v++)
                    m[u, v, 16] = 1; // Green canopy roof

            // Long Narrow Green Engine Hood (u = 13..28, v = 5..10, z = 5..11) + Yellow Stripe!
            for (int u = 13; u <= 28; u++) {
                int topZ = (u == 28) ? 10 : 11;
                for (int v = 5; v <= 10; v++) {
                    for (int z = 5; z <= topZ; z++) {
                        bool stripe = (z == 9 && (v == 5 || v == 10) && u >= 14 && u <= 27);
                        bool louver = (z >= 7 && z <= 8 && (v == 5 || v == 10) && (u == 18 || u == 20 || u == 22 || u == 24));
                        m[u, v, z] = stripe ? (byte)2 : louver ? (byte)9 : (byte)1;
                    }
                }
            }
            // Front Tractor Grille & Twin Work Headlights (u = 28)
            for (int v = 6; v <= 9; v++)
                for (int z = 6; z <= 9; z++) m[28, v, z] = 9;
            m[28, 5, 9] = 6; m[28, 10, 9] = 6;
            m[28, 5, 10] = 6; m[28, 10, 10] = 6;

            // Tall Vertical Black Exhaust Stack + Muffler above Hood!
            for (int z = 11; z <= 17; z++) {
                m[22, 6, z] = 9;
                if (z >= 13 && z <= 15) m[23, 6, z] = 9;
            }

            // Front Axle Beam (u = 25..26, v = 3..12, z = 2..3)
            for (int u = 25; u <= 26; u++)
                for (int v = 4; v <= 11; v++)
                    m[u, v, 3] = 9;

            // MASSIVE Treaded Rear Tractor Wheels (u = 3..11, z = 0..8) with Bright Yellow Hubs!
            for (int u = 3; u <= 11; u++) {
                for (int z = 0; z <= 8; z++) {
                    int du = u - 7, dz = z - 4;
                    int d2 = du * du + dz * dz;
                    if (d2 > 20) continue;
                    bool hub = (d2 <= 5);
                    byte wmat = hub ? (byte)2 : (byte)4;
                    for (int v = 2; v <= 3; v++) {
                        m[u, v, z] = wmat;
                        m[u, 15 - v, z] = wmat;
                    }
                }
            }
            // Smaller Front Steerable Tractor Wheels (u = 23..28, z = 0..5) with Yellow Hubs!
            for (int u = 23; u <= 28; u++) {
                for (int z = 0; z <= 5; z++) {
                    int du2 = u * 2 - 51, dz2 = z * 2 - 5;
                    int d2 = du2 * du2 + dz2 * dz2;
                    if (d2 > 34) continue;
                    bool hub = (d2 <= 8);
                    byte wmat = hub ? (byte)2 : (byte)4;
                    for (int v = 3; v <= 4; v++) {
                        m[u, v, z] = wmat;
                        m[u, 15 - v, z] = wmat;
                    }
                }
            }
        }

        return m;
    }

    static bool zCheck(int z) { return z >= 0; }

    static Color ShadeVehicleMat(int vehKind, byte mat, int tone) {
        // tone: 0 = Bright Top, 1 = Sunlit Left Face, 2 = Shaded Right Face
        switch (mat) {
            case 1: // Primary Body
                if (vehKind == 0) { // Police Cruiser Deep DUFF-COM Navy
                    return tone == 0 ? Color.FromArgb(64, 108, 184) :
                           tone == 1 ? Color.FromArgb(48, 84, 148)  : Color.FromArgb(34, 60, 112);
                } else if (vehKind == 1) { // Civilian Sedan Metallic Crimson/Burgundy
                    return tone == 0 ? Color.FromArgb(208, 52, 48)  :
                           tone == 1 ? Color.FromArgb(168, 36, 34)  : Color.FromArgb(124, 24, 24);
                } else if (vehKind == 2) { // Pickup Truck Rugged Ochre/Tan-Bronze
                    return tone == 0 ? Color.FromArgb(206, 158, 96) :
                           tone == 1 ? Color.FromArgb(172, 126, 70) : Color.FromArgb(132, 92, 48);
                } else { // PA Farm Tractor Classic John-Deere Green
                    return tone == 0 ? Color.FromArgb(78, 192, 54)  :
                           tone == 1 ? Color.FromArgb(56, 154, 40)  : Color.FromArgb(38, 118, 28);
                }
            case 2: // Secondary Accent (Police White Doors/Roof OR Tractor Yellow!)
                if (vehKind == 0) {
                    return tone == 0 ? Color.FromArgb(244, 246, 252) :
                           tone == 1 ? Color.FromArgb(214, 220, 232) : Color.FromArgb(174, 182, 198);
                } else {
                    return tone == 0 ? Color.FromArgb(252, 224, 48)  :
                           tone == 1 ? Color.FromArgb(228, 192, 34)  : Color.FromArgb(186, 152, 24);
                }
            case 3: // Tinted Cabin Glass
                return tone == 0 ? Color.FromArgb(145, 238, 252) :
                       tone == 1 ? Color.FromArgb(76, 188, 224)  : Color.FromArgb(46, 134, 172);
            case 4: // Rubber Tire
                return tone == 0 ? Color.FromArgb(54, 58, 66)    :
                       tone == 1 ? Color.FromArgb(38, 42, 48)    : Color.FromArgb(26, 28, 34);
            case 5: // Chrome / Silver Hubcap
                return tone == 0 ? Color.FromArgb(236, 240, 246) :
                       tone == 1 ? Color.FromArgb(186, 194, 206) : Color.FromArgb(138, 146, 160);
            case 6: // Glowing Warm Headlight
                return Color.FromArgb(255, 244, 135);
            case 7: // Glowing Red Strobe / Taillight
                return tone == 2 ? Color.FromArgb(205, 32, 32) : Color.FromArgb(252, 48, 44);
            case 8: // Glowing Electric Blue Strobe
                return tone == 2 ? Color.FromArgb(42, 108, 225) : Color.FromArgb(65, 148, 255);
            case 9: // Dark Grille / Bed Liner / Exhaust / Seat
                return tone == 0 ? Color.FromArgb(74, 78, 88)    :
                       tone == 1 ? Color.FromArgb(52, 56, 64)    : Color.FromArgb(36, 38, 46);
            case 10: // Gold Star / Amber Signal
                return Color.FromArgb(250, 205, 42);
            default:
                return Color.Gray;
        }
    }

    static bool ValidVehicleCanvasPt(int cx, int cy, bool alongY) {
        if (cx < 0 || cx >= 48 || cy < 0 || cy >= 48) return false;
        if (!alongY) {
            return (cx <= 31 && cy <= 39) || (cx >= 16 && cy >= 8);
        } else {
            return (cx >= 16 && cy <= 39) || (cx <= 31 && cy >= 8);
        }
    }

    static void RenderTwoTileVehicle(Bitmap sheet, int backSlot, int frontSlot, int vehKind, bool alongY) {
        byte[,,] m = BuildVehicleVoxels(vehKind);
        Color[,] canv = new Color[48, 48];
        bool[,] filled = new bool[48, 48];
        bool[,] isShadow = new bool[48, 48];

        // 1. Soft dark ground contact shadow beneath chassis footprint
        for (int u = 3; u <= 28; u++) {
            for (int v = 3; v <= 12; v++) {
                int cx = !alongY ? (16 + u - v) : (31 - u + v);
                int cy = 24 + ((u + v) >> 1);
                if (ValidVehicleCanvasPt(cx, cy, alongY)) {
                    canv[cx, cy] = Color.FromArgb(20, 22, 28);
                    filled[cx, cy] = true;
                    isShadow[cx, cy] = true;
                }
            }
        }

        // 2. Strict Back-to-Front 3D Voxel Projection (s = u + v from 0..46, z from 0..19)
        for (int s = 0; s <= 46; s++) {
            int uMin = Math.Max(0, s - 15);
            int uMax = Math.Min(31, s);
            for (int u = uMin; u <= uMax; u++) {
                int v = s - u;
                for (int z = 0; z < 20; z++) {
                    byte mat = m[u, v, z];
                    if (mat == 0) continue;

                    bool topOpen = (z + 1 >= 20 || m[u, v, z + 1] == 0);
                    bool vOpen   = (v + 1 >= 16 || m[u, v + 1, z] == 0);
                    bool uOpen   = (u + 1 >= 32 || m[u + 1, v, z] == 0);
                    if (!topOpen && !vOpen && !uOpen) continue;

                    bool leftOpen  = !alongY ? vOpen : uOpen;
                    bool rightOpen = !alongY ? uOpen : vOpen;

                    int tone = 1;
                    if (topOpen) {
                        tone = 0;
                    } else if (leftOpen && !rightOpen) {
                        tone = 1;
                    } else if (rightOpen && !leftOpen) {
                        tone = 2;
                    } else {
                        tone = 1;
                    }

                    Color col = ShadeVehicleMat(vehKind, mat, tone);
                    // Add specular diagonal glint on glass windshields!
                    if (mat == 3 && ((u + v + z) % 5 == 0)) {
                        col = Color.FromArgb(215, 250, 255);
                    }

                    int cx = !alongY ? (16 + u - v) : (31 - u + v);
                    int cy = 24 + ((u + v) >> 1) - z;
                    if (ValidVehicleCanvasPt(cx, cy, alongY)) {
                        canv[cx, cy] = col;
                        filled[cx, cy] = true;
                        isShadow[cx, cy] = false;
                    }
                }
            }
        }

        // 3. Crisp 1px dark silhouette outline matching DUFF-COM isometric object style
        bool[,] addOutline = new bool[48, 48];
        int[] odx = new int[] { -1, 1, 0, 0 };
        int[] ody = new int[] { 0, 0, -1, 1 };
        for (int cy = 0; cy < 48; cy++) {
            for (int cx = 0; cx < 48; cx++) {
                if (filled[cx, cy] || !ValidVehicleCanvasPt(cx, cy, alongY)) continue;
                for (int d = 0; d < 4; d++) {
                    int nx = cx + odx[d], ny = cy + ody[d];
                    if (nx >= 0 && nx < 48 && ny >= 0 && ny < 48 && filled[nx, ny] && !isShadow[nx, ny]) {
                        addOutline[cx, cy] = true;
                        break;
                    }
                }
            }
        }
        for (int cy = 0; cy < 48; cy++) {
            for (int cx = 0; cx < 48; cx++) {
                if (addOutline[cx, cy]) {
                    canv[cx, cy] = Color.FromArgb(18, 20, 24);
                    filled[cx, cy] = true;
                }
            }
        }

        // 4. Disjointly slice the 48x48 vehicle across Back Tile (32x40) & Front Tile (32x40)
        for (int cy = 0; cy < 48; cy++) {
            for (int cx = 0; cx < 48; cx++) {
                if (!filled[cx, cy]) continue;
                if (!alongY) {
                    bool toBack = (cx < 16) || (cy < 8) || (cx < 24 && cy <= 39);
                    if (toBack) {
                        PSet(sheet, backSlot * 32 + cx, cy, canv[cx, cy]);
                    } else {
                        PSet(sheet, frontSlot * 32 + (cx - 16), cy - 8, canv[cx, cy]);
                    }
                } else {
                    bool toBack = (cx >= 32) || (cy < 8) || (cx >= 24 && cy <= 39);
                    if (toBack) {
                        PSet(sheet, backSlot * 32 + (cx - 16), cy, canv[cx, cy]);
                    } else {
                        PSet(sheet, frontSlot * 32 + cx, cy - 8, canv[cx, cy]);
                    }
                }
            }
        }
    }

    static void RenderUfoCorner(Bitmap sheet, int slot, int cornerDir) {
        // cornerDir: 0 = North (x0,y0), 1 = East (x1,y0), 2 = South (x1,y1), 3 = West (x0,y1)
        int ox = slot * 32;
        double pu = (cornerDir == 0 || cornerDir == 3) ? 15.0 : 0.0;
        double pv = (cornerDir == 0 || cornerDir == 1) ? 15.0 : 0.0;

        // 1. Paint interior alien alloy deck plating inside the rounded corner arc (exterior stays Trans!)
        for (int fy = 0; fy < 16; fy++) {
            for (int fx = 0; fx < 32; fx++) {
                if (!InDiamond(fx, fy)) continue;
                double uf = (fx + 2.0 * fy - 15.0) * 0.5;
                double vf = (2.0 * fy - fx + 16.0) * 0.5;
                double rc = Math.Sqrt((uf - pu) * (uf - pu) + (vf - pv) * (vf - pv));
                if (rc <= 14.2) {
                    PSet(sheet, ox + fx, 24 + fy, UfoDeckPixel(fx, fy));
                }
            }
        }

        // 2. Render 2x-oversampled 3D quarter-circle convex saucer hull shell (su, sv in 0..31, z in 0..23)
        for (int s = 0; s <= 62; s++) {
            int suMin = Math.Max(0, s - 31);
            int suMax = Math.Min(31, s);
            for (int su = suMin; su <= suMax; su++) {
                int sv = s - su;
                double u = su * 0.48387; // 0.0 .. 15.0
                double v = sv * 0.48387; // 0.0 .. 15.0
                double du = Math.Abs(u - pu);
                double dv = Math.Abs(v - pv);
                double r  = Math.Sqrt(du * du + dv * dv);
                if (r < 11.4 || r > 15.35) continue;

                double theta = Math.Atan2(dv, du) / (Math.PI * 0.5); // 0.0 at |dv|=0 (West seam), 1.0 at |du|=0 (North seam)
                double light = 1.0 - 0.65 * theta;
                if (cornerDir == 2) {
                    // Add specular nose highlight on front-facing South saucer curve
                    double apex = 1.0 - Math.Abs(theta - 0.5) * 2.0;
                    light = Math.Min(1.08, light + 0.16 * apex);
                }

                int arcIdx = (int)Math.Round(theta * 16.0);
                int cx = (31 + su - sv) >> 1;

                for (int z = 0; z < 24; z++) {
                    int cy = 23 + ((su + sv) >> 2) - z;
                    if (cx < 0 || cx >= 32 || cy < 0 || cy >= 40) continue;

                    Color col;
                    if (z == 23) {
                        // Top curved saucer bevel + cyan rim accent matching FLOOR_UFO_ROOF_*
                        if (r >= 14.2)      col = Color.FromArgb(198, 212, 244);
                        else if (r >= 13.1) col = Color.FromArgb(76, 244, 252);
                        else                col = Color.FromArgb(124, 110, 172);
                    } else if (r < 12.3 && cornerDir == 0) {
                        // Interior concave bulkhead wall visible when UFO roof is cut away
                        int cBow = ((z - 12) * (z - 12)) / 36;
                        col = UfoHullShade(z, 0.55, (arcIdx + cBow + 16) % 8, false, arcIdx, 23 - z);
                    } else {
                        int cBow = ((z - 12) * (z - 12)) / 36;
                        col = UfoHullShade(z, light, (arcIdx + cBow + 16) % 8, false, arcIdx, 23 - z);
                    }
                    PSet(sheet, ox + cx, cy, col);
                }
            }
        }
    }

    public static void GenObjects(string path) {
        // 37 objects x 32x40 = 1184x40 (includes 2-tile vehicles, 4 curved UFO hull corners & 5 UFO stations!)
        using (Bitmap bmp = new Bitmap(37 * 32, 40)) {
            using (Graphics g = Graphics.FromImage(bmp)) g.Clear(Trans);

            // 1. OBJ_ARCADE_CAB (ox = 32): True 3D Isometric 90s Arcade Cabinet!
            {
                int ox = 1 * 32;
                DrawIsoBox(bmp, ox, 16, 35, 7, 7, 15,
                           Color.FromArgb(88, 42, 138), Color.FromArgb(68, 30, 112), Color.FromArgb(48, 20, 84), true);
                DrawIsoBox(bmp, ox, 16, 20, 6, 6, 10,
                           Color.FromArgb(248, 215, 45), Color.FromArgb(82, 38, 130), Color.FromArgb(52, 24, 92), true);
                // Glowing recessed Cyan CRT Monitor + Trackball control deck!
                for (int y = 12; y <= 18; y++)
                    for (int x = 12; x <= 19; x++)
                        PSet(bmp, ox + x, y, ((x + y) & 1) == 0 ? Color.FromArgb(65, 245, 255) : Color.FromArgb(35, 175, 215));
                // Red control panel lip + white trackball + yellow/red arcade buttons!
                for (int x = 10; x <= 21; x++) PSet(bmp, ox + x, 19, Color.FromArgb(220, 42, 38));
                PSet(bmp, ox + 14, 19, Color.White); // Trackball!
                PSet(bmp, ox + 17, 19, Color.FromArgb(250, 220, 40));
                PSet(bmp, ox + 19, 19, Color.FromArgb(60, 245, 80));
                // Glowing orange dual coin slots
                PSet(bmp, ox + 14, 27, Color.FromArgb(250, 140, 30));
                PSet(bmp, ox + 17, 27, Color.FromArgb(250, 140, 30));
            }

            // 2. OBJ_DINER_BOOTH (ox = 64): 3D Isometric Red Vinyl Diner Booth & Formica Table
            {
                int ox = 2 * 32;
                DrawIsoBox(bmp, ox, 10, 34, 4, 5, 9,
                           Color.FromArgb(228, 52, 46), Color.FromArgb(192, 34, 30), Color.FromArgb(145, 24, 22), true);
                DrawIsoBox(bmp, ox, 22, 34, 5, 4, 9,
                           Color.FromArgb(228, 52, 46), Color.FromArgb(192, 34, 30), Color.FromArgb(145, 24, 22), true);
                DrawIsoBox(bmp, ox, 16, 31, 5, 5, 3,
                           Color.FromArgb(242, 244, 248), Color.FromArgb(185, 192, 202), Color.FromArgb(145, 152, 162), true);
                PSet(bmp, ox + 15, 26, Color.FromArgb(230, 40, 35)); // Ketchup bottle!
                PSet(bmp, ox + 17, 26, Color.FromArgb(248, 215, 35)); // Mustard bottle!
            }

            // 3. OBJ_JUKEBOX (ox = 96): 3D Glowing Wurlitzer Jukebox
            {
                int ox = 3 * 32;
                DrawIsoBox(bmp, ox, 16, 35, 6, 6, 18,
                           Color.FromArgb(250, 210, 48), Color.FromArgb(145, 92, 44), Color.FromArgb(105, 64, 28), true);
                for (int y = 16; y <= 32; y++) {
                    PSet(bmp, ox + 11, y, (y & 2) == 0 ? Color.FromArgb(255, 140, 35) : Color.FromArgb(250, 225, 50));
                    PSet(bmp, ox + 21, y, (y & 2) == 0 ? Color.FromArgb(65, 245, 255) : Color.FromArgb(240, 70, 205));
                }
                for (int y = 17; y <= 22; y++)
                    for (int x = 13; x <= 19; x++)
                        PSet(bmp, ox + x, y, Color.FromArgb(80, 245, 250));
            }

            // 4. OBJ_CORN (ox = 128): Lush Tall Pennsylvania Cornstalks with Golden Ears
            {
                int ox = 4 * 32;
                int[] stalks = new int[] { 8, 13, 18, 23 };
                foreach (int sx in stalks) {
                    for (int y = 9; y <= 34; y++) {
                        int sway = ((y / 5) + sx) & 1;
                        PSet(bmp, ox + sx + sway, y, Color.FromArgb(46, 128, 34));
                        PSet(bmp, ox + sx + sway + 1, y, Color.FromArgb(78, 174, 52));
                        if (y < 13) PSet(bmp, ox + sx + sway, y, Color.FromArgb(238, 208, 72)); // Golden tassel
                        if (y % 5 == 0 && y > 13 && y < 31) {
                            // Arching corn leaves
                            PSet(bmp, ox + sx - 2, y + 1, Color.FromArgb(62, 154, 42));
                            PSet(bmp, ox + sx - 1, y,     Color.FromArgb(86, 182, 58));
                            PSet(bmp, ox + sx + 2, y,     Color.FromArgb(62, 154, 42));
                            PSet(bmp, ox + sx + 3, y + 1, Color.FromArgb(42, 112, 30));
                        }
                        if (y == 20 || y == 21) {
                            PSet(bmp, ox + sx + 1, y, Color.FromArgb(248, 215, 45)); // Golden ear of PA sweet corn!
                        }
                    }
                }
            }

            // 5. OBJ_STREETLAMP (ox = 160): Fluted Cast-Iron Streetlamp with Glowing Cobra Head
            {
                int ox = 5 * 32;
                DrawIsoBox(bmp, ox, 15, 35, 3, 3, 4,
                           Color.FromArgb(110, 116, 126), Color.FromArgb(78, 84, 94), Color.FromArgb(52, 56, 64), true);
                for (int y = 6; y <= 31; y++) {
                    PSet(bmp, ox + 14, y, Color.FromArgb(42, 46, 54));
                    PSet(bmp, ox + 15, y, Color.FromArgb(148, 156, 168));
                    PSet(bmp, ox + 16, y, Color.FromArgb(78, 84, 94));
                }
                for (int x = 15; x <= 23; x++) PSet(bmp, ox + x, 6, Color.FromArgb(138, 146, 158));
                for (int x = 20; x <= 25; x++) {
                    PSet(bmp, ox + x, 7, Color.FromArgb(252, 230, 85));
                    PSet(bmp, ox + x, 8, Color.FromArgb(255, 252, 210));
                }
            }

            // 6 & 14..16: Full 2-Tile Coopersburg Police Cruiser (+X: 6 & 14, +Y: 15 & 16)
            RenderTwoTileVehicle(bmp, 6,  14, 0, false);
            RenderTwoTileVehicle(bmp, 15, 16, 0, true);

            // 7. OBJ_UFO_CORE (ox = 224): Rounded Futuristic Alien Elerium-115 Power Reactor!
            {
                int ox = 7 * 32;
                // Rounded octagonal violet-titanium base ring + cyan magnetic conduit
                for (int y = 26; y <= 36; y++) {
                    for (int x = 6; x <= 25; x++) {
                        double dx = (x - 15.5) / 9.2;
                        double dy = (y - 31.0) / 4.8;
                        double d2 = dx * dx + dy * dy;
                        if (d2 <= 1.0) {
                            Color bc = d2 > 0.78 ? Color.FromArgb(24, 18, 38) :
                                       (y == 29 || y == 30) ? Color.FromArgb(72, 244, 252) :
                                       (x < 16) ? Color.FromArgb(146, 118, 198) : Color.FromArgb(88, 64, 138);
                            PSet(bmp, ox + x, y, bc);
                        }
                    }
                }
                // Blazing Elerium-115 Plasma Containment Sphere + magnetic field ribs
                for (int y = 10; y <= 28; y++) {
                    for (int x = 8; x <= 23; x++) {
                        int d2 = (x - 16) * (x - 16) + (y - 19) * (y - 19);
                        if (d2 <= 49) {
                            bool magRing = (y == 19 && (x <= 10 || x >= 21));
                            Color ec = magRing ? Color.FromArgb(80, 248, 255)  :
                                       d2 < 7  ? Color.FromArgb(248, 255, 224) :
                                       d2 < 18 ? Color.FromArgb(168, 255, 58)  :
                                       d2 < 34 ? Color.FromArgb(92, 214, 34)   : Color.FromArgb(34, 98, 28);
                            PSet(bmp, ox + x, y, ec);
                        }
                    }
                }
                // Domed alien alloy upper containment crown + cyan apex emitter
                for (int y = 7; y <= 13; y++) {
                    int hw = (y <= 8) ? 3 : (y <= 10) ? 5 : 6;
                    for (int x = 16 - hw; x <= 15 + hw; x++) {
                        Color cc = (y == 8 && x >= 14 && x <= 17) ? Color.FromArgb(80, 248, 255) :
                                   (x < 16) ? Color.FromArgb(182, 196, 232) : Color.FromArgb(108, 92, 158);
                        PSet(bmp, ox + x, y, cc);
                    }
                }
            }

            // 8. OBJ_CRATE (ox = 256): 3D Isometric Military/Salvage Crate
            {
                int ox = 8 * 32;
                DrawIsoBox(bmp, ox, 16, 35, 7, 7, 10,
                           Color.FromArgb(196, 152, 92), Color.FromArgb(162, 118, 66), Color.FromArgb(124, 88, 46), true);
                for (int i = 0; i < 6; i++) PSet(bmp, ox + 10 + i, 26 + i / 2, Color.FromArgb(98, 68, 34));
            }

            // 9. OBJ_STAIRS_UP (ox = 288): True 3D Isometric 6-Step Staircase with Handrail
            {
                int ox = 9 * 32;
                for (int s = 0; s < 6; s++) {
                    DrawIsoBox(bmp, ox, 12 + s * 2, 35 - s * 4, 2, 7, 4,
                               Color.FromArgb(196, 188, 174), Color.FromArgb(142, 134, 122), Color.FromArgb(104, 98, 88), true);
                }
                // Steel Handrail
                for (int s = 0; s < 12; s++) {
                    PSet(bmp, ox + 6 + s, 24 - s, Color.FromArgb(80, 235, 248));
                }
            }

            // 10. OBJ_RUBBLE (ox = 320): 3D Broken Brick/Concrete Debris & Scorch
            {
                int ox = 10 * 32;
                DrawIsoBox(bmp, ox, 12, 34, 4, 4, 3,
                           Color.FromArgb(145, 62, 52), Color.FromArgb(112, 46, 38), Color.FromArgb(84, 34, 28), true);
                DrawIsoBox(bmp, ox, 19, 35, 4, 3, 4,
                           Color.FromArgb(148, 148, 154), Color.FromArgb(108, 108, 114), Color.FromArgb(76, 76, 82), true);
                DrawIsoBox(bmp, ox, 15, 31, 3, 3, 2,
                           Color.FromArgb(96, 96, 102), Color.FromArgb(68, 68, 74), Color.FromArgb(48, 48, 52), true);
            }

            // 11. OBJ_FLARE (ox = 352): Glowing Night Electro-Flare with Radial Ground Halo
            {
                int ox = 11 * 32;
                for (int y = 26; y <= 35; y++) {
                    for (int x = 8; x <= 24; x++) {
                        int d = Math.Abs(x - 16) + Math.Abs(y - 31) * 2;
                        if (d <= 8) PSet(bmp, ox + x, y, d < 3 ? Color.White : d < 6 ? Color.FromArgb(255, 235, 60) : Color.FromArgb(235, 135, 25));
                    }
                }
            }

            // 12. OBJ_BARREL (ox = 384): 3D Shaded Cylindrical Hazard Fuel Drum
            {
                int ox = 12 * 32;
                for (int y = 17; y <= 34; y++) {
                    for (int x = 10; x <= 21; x++) {
                        bool border = (x == 10 || x == 21 || y == 17 || y == 34);
                        bool band = (y >= 23 && y <= 26);
                        int cyl = 225 - Math.Abs(x - 14) * 16;
                        if (cyl < 90) cyl = 90;
                        Color dc = border ? Color.FromArgb(24, 20, 20) :
                                   band   ? Color.FromArgb(cyl, (cyl * 8) / 10, 24) :
                                            Color.FromArgb(cyl, cyl / 5, cyl / 5);
                        PSet(bmp, ox + x, y, dc);
                    }
                }
                // Top isometric lid ellipse
                for (int x = 11; x <= 20; x++) {
                    PSet(bmp, ox + x, 16, Color.FromArgb(235, 72, 62));
                    PSet(bmp, ox + x, 17, Color.FromArgb(185, 42, 36));
                }
            }

            // 13 & 17..19: Full 2-Tile Pennsylvania Farm Tractor (+X: 13 & 17, +Y: 18 & 19)
            RenderTwoTileVehicle(bmp, 13, 17, 3, false);
            RenderTwoTileVehicle(bmp, 18, 19, 3, true);

            // 20..23: Full 2-Tile 90s Civilian Sedan (+X: 20 & 21, +Y: 22 & 23)
            RenderTwoTileVehicle(bmp, 20, 21, 1, false);
            RenderTwoTileVehicle(bmp, 22, 23, 1, true);

            // 24..27: Full 2-Tile Utility / Farm Pickup Truck (+X: 24 & 25, +Y: 26 & 27)
            RenderTwoTileVehicle(bmp, 24, 25, 2, false);
            RenderTwoTileVehicle(bmp, 26, 27, 2, true);

            // 28..31: 3D Curved Alien Saucer Hull Corner Fairings (N, E, S, W)!
            RenderUfoCorner(bmp, 28, 0); // OBJ_UFO_CORNER_N
            RenderUfoCorner(bmp, 29, 1); // OBJ_UFO_CORNER_E
            RenderUfoCorner(bmp, 30, 2); // OBJ_UFO_CORNER_S
            RenderUfoCorner(bmp, 31, 3); // OBJ_UFO_CORNER_W

            // 32. OBJ_UFO_NAV: Alien Navigation Console w/ Floating 3D Holographic Star-Chart!
            {
                int ox = 32 * 32;
                // Curved horseshoe violet-titanium bridge console pedestal (y = 22..36)
                for (int y = 22; y <= 36; y++) {
                    for (int x = 5; x <= 26; x++) {
                        double dx = (x - 15.5) / 10.2;
                        double dy = (y - 29.5) / 6.2;
                        double r2 = dx * dx + dy * dy;
                        if (r2 > 1.0) continue;
                        if (y <= 27) {
                            // Sloped illuminated alien touch-glyph control surface + central holo-projector lens
                            double lens = (x - 15.5) * (x - 15.5) + (y - 24.5) * (y - 24.5) * 3.0;
                            Color tc = lens <= 6.5 ? Color.FromArgb(165, 255, 255) :
                                       lens <= 15.0 ? Color.FromArgb(52, 214, 238) :
                                       ((x + y) % 3 == 0 && r2 < 0.72) ? Color.FromArgb(245, 88, 208) :
                                       ((x * 3 + y) % 5 == 0 && r2 < 0.72) ? Color.FromArgb(162, 252, 58) :
                                       Color.FromArgb(136, 114, 188);
                            PSet(bmp, ox + x, y, tc);
                        } else {
                            // Contoured lower console skirt with glowing cyan trim band
                            bool border = (r2 > 0.82 || y == 36);
                            bool band   = (y == 31);
                            Color bc = border ? Color.FromArgb(24, 18, 38) :
                                       band   ? Color.FromArgb(72, 242, 252) :
                                       (x < 16) ? Color.FromArgb(124, 92, 174) : Color.FromArgb(76, 52, 122);
                            PSet(bmp, ox + x, y, bc);
                        }
                    }
                }
                // Floating 3D Holographic Star-Globe + Tilted Orbital Ring hovering above console (y = 9..22)!
                for (int y = 9; y <= 22; y++) {
                    for (int x = 7; x <= 24; x++) {
                        double gx = x - 15.5;
                        double gy = y - 15.5;
                        double sphere = gx * gx + gy * gy;
                        // Tilted elliptical planetary orbit ring
                        double rx = (gx * 0.92 + gy * 0.38) / 8.2;
                        double ry = (-gx * 0.38 + gy * 0.92) / 3.0;
                        double ring = rx * rx + ry * ry;
                        if (ring >= 0.76 && ring <= 1.12) {
                            PSet(bmp, ox + x, y, (x == 9 || x == 22) ? Color.FromArgb(252, 228, 64) : Color.FromArgb(88, 248, 255));
                        } else if (sphere <= 26.0) {
                            // Wireframe latitude/longitude hologram globe + glowing gold target vector!
                            bool grid = (Math.Abs(gx) < 0.8) || (Math.Abs(gy) < 0.8) || (sphere >= 19.5);
                            if (Math.Abs(gx - 1.5) < 1.1 && Math.Abs(gy + 1.0) < 1.1) {
                                PSet(bmp, ox + x, y, Color.FromArgb(255, 228, 58)); // Target world blip!
                            } else if (grid || ((x + y) & 1) == 0) {
                                PSet(bmp, ox + x, y, grid ? Color.FromArgb(96, 252, 255) : Color.FromArgb(32, 164, 198));
                            }
                        }
                    }
                }
                // Faint cyan projection beam rays from lens up into hologram
                PSet(bmp, ox + 13, 22, Color.FromArgb(56, 218, 238));
                PSet(bmp, ox + 18, 22, Color.FromArgb(56, 218, 238));
            }

            // 33. OBJ_UFO_COMPUTER: Alien Hyper-Wave Flight Computer / Crystal Mainframe Spire!
            {
                int ox = 33 * 32;
                for (int y = 8; y <= 35; y++) {
                    int hw = (y <= 10) ? 5 : (y <= 13) ? 7 : (y >= 34) ? 7 : 8;
                    for (int x = 16 - hw; x <= 15 + hw; x++) {
                        bool edge = (x == 16 - hw || x == 15 + hw || y == 8 || y == 35);
                        bool leftCrystal  = (x >= 11 && x <= 13 && y >= 13 && y <= 29);
                        bool rightCrystal = (x >= 18 && x <= 20 && y >= 13 && y <= 29);
                        bool centerCore   = (x >= 15 && x <= 16 && y >= 11 && y <= 31);
                        Color cc;
                        if (edge) {
                            cc = Color.FromArgb(24, 18, 38);
                        } else if (y <= 10) {
                            cc = (x < 16) ? Color.FromArgb(186, 198, 234) : Color.FromArgb(124, 110, 174);
                        } else if (leftCrystal) {
                            // Glowing Neon-Cyan optical crystal column
                            cc = ((y + x) % 3 == 0) ? Color.FromArgb(165, 255, 255) : Color.FromArgb(44, 198, 228);
                        } else if (rightCrystal) {
                            // Glowing Alien Magenta/Rose telemetry column
                            cc = ((y + x) % 3 == 0) ? Color.FromArgb(250, 112, 214) : Color.FromArgb(178, 48, 148);
                        } else if (centerCore && (y % 4 == 0)) {
                            cc = Color.FromArgb(168, 252, 58); // Pulsing Elerium status runes
                        } else {
                            cc = (x < 16) ? Color.FromArgb(122, 92, 172) : Color.FromArgb(74, 52, 118);
                        }
                        PSet(bmp, ox + x, y, cc);
                    }
                }
            }

            // 34. OBJ_UFO_CRYO: Alien Cryo-Stasis Bio-Chamber w/ Translucent Fluid & Specimen!
            {
                int ox = 34 * 32;
                for (int y = 7; y <= 35; y++) {
                    int hw = (y == 7 || y == 35) ? 4 : (y <= 9 || y >= 33) ? 6 : 7;
                    for (int x = 16 - hw; x <= 15 + hw; x++) {
                        bool border = (x == 16 - hw || x == 15 + hw || y == 7 || y == 35);
                        bool cap    = (y <= 11 || y >= 30);
                        Color sc;
                        if (border) {
                            sc = Color.FromArgb(22, 20, 36);
                        } else if (cap) {
                            // Contoured purple-titanium alloy cryo end-caps + gold/lime life-support LEDs
                            if (y == 32 && (x == 13 || x == 18)) sc = Color.FromArgb(168, 252, 58);
                            else if (y == 10) sc = Color.FromArgb(80, 248, 255);
                            else sc = (x < 16) ? Color.FromArgb(148, 134, 196) : Color.FromArgb(88, 70, 136);
                        } else {
                            // Cylindrical glowing Teal/Emerald stasis glass + suspended alien silhouette!
                            bool specHead = (y >= 15 && y <= 18 && x >= 14 && x <= 17);
                            bool specBody = (y >= 19 && y <= 25 && x >= 15 && x <= 16);
                            bool glint    = (x == 11 || (x == 12 && y % 3 == 0));
                            bool bubble   = ((x * 7 + y * 5) % 17 == 0);
                            if (specHead || specBody) sc = Color.FromArgb(28, 72, 78);
                            else if (glint)           sc = Color.FromArgb(196, 255, 255);
                            else if (bubble)          sc = Color.FromArgb(158, 255, 190);
                            else sc = (x < 16) ? Color.FromArgb(58, 224, 228) : Color.FromArgb(28, 152, 168);
                        }
                        PSet(bmp, ox + x, y, sc);
                    }
                }
            }

            // 35. OBJ_UFO_POD: Rounded Hexagonal Alien Artifact / Ordnance Containment Pod!
            {
                int ox = 35 * 32;
                for (int y = 19; y <= 35; y++) {
                    int hw = (y <= 21) ? 5 : (y <= 24) ? 7 : (y >= 34) ? 6 : 8;
                    for (int x = 16 - hw; x <= 15 + hw; x++) {
                        bool border = (x == 16 - hw || x == 15 + hw || y == 19 || y == 35);
                        bool topSeal = (y >= 20 && y <= 23 && x >= 13 && x <= 18);
                        bool clasp   = ((x == 11 || x == 20) && y >= 24 && y <= 32);
                        bool band    = (y == 27 || y == 28);
                        Color pc;
                        if (border) {
                            pc = Color.FromArgb(24, 18, 38);
                        } else if (topSeal) {
                            // Glowing Elerium/Gold energy seal on top lid
                            pc = (x >= 15 && x <= 16 && y >= 21 && y <= 22) ? Color.FromArgb(255, 232, 64) :
                                                                              Color.FromArgb(158, 252, 56);
                        } else if (clasp) {
                            pc = Color.FromArgb(182, 196, 228); // Heavy titanium magnetic ribs
                        } else if (band) {
                            pc = (x >= 14 && x <= 17) ? Color.FromArgb(252, 148, 38) : Color.FromArgb(68, 236, 248);
                        } else {
                            pc = (x < 16) ? Color.FromArgb(128, 92, 178) : Color.FromArgb(78, 52, 124);
                        }
                        PSet(bmp, ox + x, y, pc);
                    }
                }
            }

            // 36. OBJ_UFO_GRAVLIFT: Circular Alien Anti-Gravity Lift Pad + Rising Energy Rings!
            {
                int ox = 36 * 32;
                // Circular flush emitter pad on the UFO deck (y = 26..37)
                for (int y = 26; y <= 37; y++) {
                    for (int x = 4; x <= 27; x++) {
                        double dx = (x - 15.5) / 11.0;
                        double dy = (y - 31.5) / 5.4;
                        double r2 = dx * dx + dy * dy;
                        if (r2 > 1.0) continue;
                        Color gc = r2 > 0.80 ? Color.FromArgb(142, 128, 194) :
                                   r2 > 0.56 ? Color.FromArgb(64, 238, 252)  :
                                   r2 > 0.25 ? Color.FromArgb(92, 56, 152)   :
                                               Color.FromArgb(195, 255, 255);
                        PSet(bmp, ox + x, y, gc);
                    }
                }
                // Three translucent floating anti-gravity levitation rings rising toward z = 1!
                int[] ringY  = new int[] { 22, 15, 8 };
                double[] rxA = new double[] { 9.0, 7.8, 6.8 };
                double[] ryA = new double[] { 4.2, 3.6, 3.2 };
                for (int r = 0; r < 3; r++) {
                    int cy = ringY[r];
                    for (int y = cy - 4; y <= cy + 4; y++) {
                        for (int x = 5; x <= 26; x++) {
                            double dx = (x - 15.5) / rxA[r];
                            double dy = (y - cy) / ryA[r];
                            double d2 = dx * dx + dy * dy;
                            if (d2 >= 0.72 && d2 <= 1.12 && ((x + y + r) & 1) == 0) {
                                Color rc = (r == 1) ? Color.FromArgb(204, 108, 248) : Color.FromArgb(88, 250, 255);
                                PSet(bmp, ox + x, y, rc);
                            }
                        }
                    }
                }
            }

            SaveBmp24(bmp, path);
        }
    }

    static void GenUnits(string path) {
        // 8 cols (dir 0..7) x 36 rows (9 sprite groups * 4 poses) of 24x32 = 192 x 1152
        // 0..2: Male DUFF-COM Operatives (Visor/Brown, Blonde Flat-Top, Dark Buzzcut Vet)
        // 3..5: Female DUFF-COM Operatives (Redhead Ponytail, Blonde Shoulder, Dark Bob/Visor)
        // 6: Sectoid, 7: Floater, 8: Muton
        using (Bitmap bmp = new Bitmap(192, 1152)) {
            using (Graphics g = Graphics.FromImage(bmp)) g.Clear(Trans);

            int[] dxTab = new int[] { 0,  4,  6,  4,  0, -4, -6, -4 };
            int[] dyTab = new int[] {-4, -3,  0,  3,  4,  3,  0, -3 };

            for (int type = 0; type < 9; type++) {
                for (int pose = 0; pose < 4; pose++) {
                    int row = type * 4 + pose;
                    int oy = row * 32;
                    int yOff = (pose == 1) ? 4 : ((pose >= 2) ? -1 : 0);
                    int legL = (pose == 2) ? -2 : (pose == 3 ?  2 : 0);
                    int legR = (pose == 2) ?  2 : (pose == 3 ? -2 : 0);

                    for (int dir = 0; dir < 8; dir++) {
                        int ox = dir * 24;

                        // Cast ground shadow ellipse
                        for (int y = 26; y <= 29; y++)
                            for (int x = 6; x <= 17; x++)
                                if ((x - 12)*(x - 12) + (y - 28)*(y - 28)*4 <= 26)
                                    PSet(bmp, ox + x, oy + y, Color.FromArgb(28, 32, 38));

                        Color[,] spr = new Color[24, 32];
                        for (int py = 0; py < 32; py++)
                            for (int px = 0; px < 24; px++) spr[px, py] = Trans;

                        if (type < 6) {
                            // ================================================================
                            // 1994 X-COM PERSONAL ARMOR OPERATIVES (3 Male + 3 Female Variants)
                            // ================================================================
                            bool isFem  = (type >= 3);
                            bool backView = (dir == 0 || dir == 1 || dir == 7);

                            Color skinHi = (type == 2) ? Color.FromArgb(156, 96, 58)  :
                                           (type == 5) ? Color.FromArgb(182, 122, 82) :
                                                         Color.FromArgb(242, 188, 152);
                            Color skinSh = (type == 2) ? Color.FromArgb(116, 68, 38)  :
                                           (type == 5) ? Color.FromArgb(142, 92, 58)  :
                                                         Color.FromArgb(204, 146, 112);

                            Color hairHi, hairSh;
                            switch (type) {
                                case 0: // Male: Chestnut crew-cut + cyan visor
                                    hairHi = Color.FromArgb(124, 76, 40);
                                    hairSh = Color.FromArgb(78, 46, 24);
                                    break;
                                case 1: // Male: Classic X-COM Golden-Blonde Flat-Top
                                    hairHi = Color.FromArgb(252, 224, 88);
                                    hairSh = Color.FromArgb(198, 156, 44);
                                    break;
                                case 2: // Male: Close-cropped dark buzzcut veteran
                                    hairHi = Color.FromArgb(44, 36, 34);
                                    hairSh = Color.FromArgb(24, 20, 20);
                                    break;
                                case 3: // Female: Fiery Redhead Ponytail
                                    hairHi = Color.FromArgb(224, 78, 38);
                                    hairSh = Color.FromArgb(158, 44, 22);
                                    break;
                                case 4: // Female: Golden-Blonde Shoulder Locks
                                    hairHi = Color.FromArgb(250, 220, 96);
                                    hairSh = Color.FromArgb(204, 164, 54);
                                    break;
                                default: // Female: Raven-Black Bob + Gold Trim & Cyan Visor
                                    hairHi = Color.FromArgb(52, 62, 84);
                                    hairSh = Color.FromArgb(24, 28, 38);
                                    break;
                            }

                            // 1. Armored Legs, Titanium Knee-Pads & Combat Boots
                            int ly0 = 19 + (pose == 1 ? 2 : 0);
                            int lFootY = Math.Min(27, 26 + legL / 2);
                            int rFootY = Math.Min(27, 26 + legR / 2);
                            int lx0 = 9 + legL / 2;
                            int rx0 = 13 + legR / 2;

                            for (int y = ly0; y <= lFootY; y++) {
                                bool knee = (y == ly0 + 3 && !backView);
                                spr[lx0, y]     = knee ? Color.FromArgb(196, 212, 235) : Color.FromArgb(74, 132, 218);
                                spr[lx0 + 1, y] = knee ? Color.FromArgb(148, 168, 198) : Color.FromArgb(44, 82, 152);
                            }
                            for (int y = ly0; y <= rFootY; y++) {
                                bool knee = (y == ly0 + 3 && !backView);
                                spr[rx0, y]     = knee ? Color.FromArgb(196, 212, 235) : Color.FromArgb(74, 132, 218);
                                spr[rx0 + 1, y] = knee ? Color.FromArgb(148, 168, 198) : Color.FromArgb(44, 82, 152);
                            }
                            // Chunky dark gunmetal combat boots
                            spr[lx0 - 1, lFootY + 1] = Color.FromArgb(58, 64, 74);
                            spr[lx0,     lFootY + 1] = Color.FromArgb(78, 86, 98);
                            spr[lx0 + 1, lFootY + 1] = Color.FromArgb(42, 46, 54);
                            spr[rx0,     rFootY + 1] = Color.FromArgb(78, 86, 98);
                            spr[rx0 + 1, rFootY + 1] = Color.FromArgb(58, 64, 74);

                            // 2. Sculpted V-Tapered Personal Armor Torso & Titanium Shoulder Pauldrons
                            int tTop = 11 + yOff;
                            int tBot = 19 + (pose == 1 ? 1 : 0);
                            for (int y = tTop; y < tBot; y++) {
                                int dy = y - tTop;
                                int x0, x1;
                                if (dy <= 2) {
                                    // Rounded Shoulder Pauldrons
                                    x0 = isFem ? 7 : 6;
                                    x1 = isFem ? 16 : 17;
                                } else if (dy <= 5) {
                                    // Chestplate
                                    x0 = isFem ? 8 : 7;
                                    x1 = isFem ? 15 : 16;
                                } else {
                                    // Tapered Waist & Utility Belt
                                    x0 = isFem ? 9 : 8;
                                    x1 = isFem ? 14 : 15;
                                }

                                for (int x = x0; x <= x1; x++) {
                                    if (y == tBot - 1) {
                                        // Tactical utility belt + gold buckle
                                        spr[x, y] = (x == 11 || x == 12) ? Color.FromArgb(248, 212, 52) : Color.FromArgb(46, 52, 62);
                                    } else if (dy <= 2 && (x <= x0 + 1 || x >= x1 - 1)) {
                                        // Titanium-silver shoulder pauldron plates!
                                        spr[x, y] = (x <= x0 + 1) ? Color.FromArgb(204, 218, 238) : Color.FromArgb(128, 144, 168);
                                    } else {
                                        // Contoured Cobalt-Blue Personal Armor with sternum seam
                                        spr[x, y] = (x == 12 && !backView) ? Color.FromArgb(36, 68, 128) :
                                                    (x <= 10)              ? Color.FromArgb(92, 158, 245) :
                                                    (x >= 14)              ? Color.FromArgb(42, 82, 154)  :
                                                                             Color.FromArgb(66, 124, 212);
                                    }
                                }
                            }

                            // Collar trim / X-COM chest insignia (or backpack power unit from behind)
                            if (backView) {
                                for (int y = tTop + 1; y <= tTop + 5; y++)
                                    for (int x = 9; x <= 14; x++)
                                        spr[x, y] = (x <= 11) ? Color.FromArgb(164, 176, 194) : Color.FromArgb(108, 118, 136);
                            } else {
                                Color trim = (type == 5 || type == 1) ? Color.FromArgb(250, 215, 52) : Color.FromArgb(196, 214, 236);
                                spr[10, tTop] = trim;
                                spr[13, tTop] = trim;
                                spr[11, tTop + 2] = Color.FromArgb(250, 218, 56);
                                spr[12, tTop + 2] = Color.FromArgb(250, 218, 56);
                            }

                            // 3. Bare-Headed 1994 X-COM Face, Hairstyles & Tactical Visors!
                            int hTop = (type == 1) ? (3 + yOff) : (4 + yOff); // Blonde flat-top stands 1px proud!
                            int hBot = 10 + yOff;
                            for (int y = hTop; y <= hBot; y++) {
                                int x0 = (y <= 5 + yOff && type != 1) ? 9 : 8;
                                int x1 = (y <= 5 + yOff && type != 1) ? 14 : 15;
                                if (y >= 8 + yOff && !isFem) { x0 = 9; x1 = 14; }
                                if (y == hBot) { x0 = 10; x1 = 13; } // Sculpted jawline/chin!

                                for (int x = x0; x <= x1; x++) {
                                    bool isHair = backView || (y <= 6 + yOff) ||
                                                  (dir == 2 && x <= 9) || (dir == 6 && x >= 14) ||
                                                  (isFem && (x == x0 || x == x1) && y <= 9 + yOff);
                                    if (isHair) {
                                        spr[x, y] = (x < 12 || y <= 5 + yOff) ? hairHi : hairSh;
                                    } else {
                                        spr[x, y] = (x < 12) ? skinHi : skinSh;
                                    }
                                }
                            }

                            // Female shoulder-length locks / ponytail cascading onto collar!
                            if (isFem) {
                                if (type == 3) {
                                    // Redhead ponytail swept to side/back
                                    int px = (dir >= 1 && dir <= 3) ? 7 : 15;
                                    spr[px, 6 + yOff] = hairHi;
                                    spr[px, 7 + yOff] = hairHi;
                                    spr[px, 8 + yOff] = hairSh;
                                    spr[px, 9 + yOff] = hairSh;
                                    spr[px, 10 + yOff] = hairSh;
                                } else if (type == 4) {
                                    // Blonde shoulder locks framing both sides
                                    for (int y = 7 + yOff; y <= 11 + yOff; y++) {
                                        spr[8,  y] = hairHi;
                                        spr[15, y] = hairSh;
                                    }
                                }
                            }

                            // Eyes or Glowing Cyan Tactical Visor on front/side facings
                            if (!backView) {
                                int eyeShift = (dir == 2 || dir == 3) ? 1 : (dir == 5 || dir == 6) ? -1 : 0;
                                if (type == 0 || type == 5) {
                                    // Glowing Cyan Tactical Visor Shades!
                                    for (int vx = 9 + eyeShift; vx <= 14 + eyeShift; vx++)
                                        if (vx >= 8 && vx <= 15)
                                            spr[vx, 7 + yOff] = (vx <= 11 + eyeShift) ? Color.FromArgb(95, 252, 255) : Color.FromArgb(38, 195, 225);
                                } else {
                                    // Crisp dark eyes + brow highlight
                                    spr[10 + eyeShift, 7 + yOff] = Color.FromArgb(22, 26, 36);
                                    spr[13 + eyeShift, 7 + yOff] = Color.FromArgb(22, 26, 36);
                                }
                            }

                            // 4. Two-Handed Shaded Tactical Rifle + Armored Forearm Grip
                            for (int s = 0; s <= 6; s++) {
                                int gx = 12 + (dxTab[dir] * s) / 4;
                                int gy = 15 + yOff + (dyTab[dir] * s) / 4;
                                if (gx >= 1 && gx < 22 && gy >= 1 && gy < 30) {
                                    Color gunC = (s == 6) ? Color.FromArgb(250, 220, 65) :
                                                 (s <= 2) ? Color.FromArgb(72, 82, 96) :
                                                            Color.FromArgb(168, 180, 198);
                                    spr[gx, gy] = gunC;
                                    if (s >= 1 && s <= 4 && gy + 1 < 30)
                                        spr[gx, gy + 1] = (s == 2) ? Color.FromArgb(62, 114, 198) : Color.FromArgb(44, 50, 60);
                                }
                            }

                        } else if (type == 6) {
                            // SECTOID: Slender V-tapered Gray Alien with Pear-Shaped Cranium & Slanted Black Eyes
                            for (int y = 20 + (pose == 1 ? 2 : 0); y < 27; y++) {
                                spr[10 + legL/2, y] = Color.FromArgb(142, 148, 138);
                                spr[13 + legR/2, y] = Color.FromArgb(112, 118, 108);
                            }
                            // Slender ribcage & narrow alien waist + thin arms
                            for (int y = 13 + yOff; y < 20 + (pose == 1 ? 2 : 0); y++) {
                                int halfW = (y < 16 + yOff) ? 3 : 2;
                                for (int x = 12 - halfW; x <= 11 + halfW; x++) {
                                    spr[x, y] = (x < 12) ? Color.FromArgb(168, 174, 162) : Color.FromArgb(122, 128, 118);
                                }
                            }
                            spr[8,  15 + yOff] = Color.FromArgb(148, 154, 142);
                            spr[15, 15 + yOff] = Color.FromArgb(122, 128, 118);
                            // Pear-shaped bulbous cranium (wide brow, tapered chin)
                            for (int y = 4 + yOff; y <= 12 + yOff; y++) {
                                int dyH = y - (4 + yOff);
                                int halfW = (dyH == 0) ? 2 : (dyH <= 4) ? 4 : (dyH <= 6) ? 3 : 2;
                                for (int x = 12 - halfW; x <= 11 + halfW; x++) {
                                    spr[x, y] = (x < 12) ? Color.FromArgb(196, 202, 190) : Color.FromArgb(146, 152, 142);
                                }
                            }
                            // Large slanted obsidian wrap-around eyes (with white glint)
                            if (dir != 0) {
                                int ex = (dir >= 1 && dir <= 3) ? 1 : (dir >= 5 && dir <= 7) ? -1 : 0;
                                spr[9  + ex, 8 + yOff] = Color.FromArgb(16, 18, 24);
                                spr[10 + ex, 9 + yOff] = Color.FromArgb(16, 18, 24);
                                spr[13 + ex, 9 + yOff] = Color.FromArgb(16, 18, 24);
                                spr[14 + ex, 8 + yOff] = Color.FromArgb(16, 18, 24);
                                spr[9  + ex, 8 + yOff] = Color.FromArgb(90, 110, 130);
                            }
                            // Glowing Elerium Plasma Pistol
                            int pgx = 12 + dxTab[dir] / 2;
                            int pgy = 15 + yOff + dyTab[dir] / 2;
                            spr[pgx, pgy] = Color.FromArgb(145, 255, 55);
                            if (pgx + 1 < 22) spr[pgx + 1, pgy] = Color.FromArgb(205, 255, 120);

                        } else if (type == 7) {
                            // FLOATER: Flared Crimson Cape + Steel Collar + Cybernetic Hover Thruster & Exhaust!
                            int bob = (pose >= 2) ? -2 : 0;
                            // Tapered metal anti-grav thruster pod + bright flame jet
                            for (int y = 19 + bob; y <= 25 + bob; y++) {
                                int hw = (y >= 23 + bob) ? 2 : 3;
                                for (int x = 12 - hw; x <= 11 + hw; x++) {
                                    spr[x, y] = (y >= 24 + bob) ? Color.FromArgb(255, 225, 65) :
                                                (y == 23 + bob) ? Color.FromArgb(255, 130, 28) :
                                                (x < 12) ? Color.FromArgb(158, 166, 178) : Color.FromArgb(98, 106, 118);
                                }
                            }
                            // Sculpted Crimson Armored Cloak & Shoulder Pads + Silver Chest Harness
                            for (int y = 10 + bob; y < 19 + bob; y++) {
                                int hw = (y < 13 + bob) ? 5 : (y < 16 + bob) ? 4 : 5;
                                for (int x = 12 - hw; x <= 11 + hw; x++) {
                                    bool harness = (y >= 12 + bob && y <= 15 + bob && x >= 10 && x <= 13);
                                    spr[x, y] = harness  ? Color.FromArgb(182, 190, 202) :
                                                (x < 12) ? Color.FromArgb(224, 52, 46) : Color.FromArgb(148, 26, 26);
                                }
                            }
                            // Pale cybernetic head & glowing visor
                            for (int y = 5 + bob; y < 10 + bob; y++)
                                for (int x = 9; x <= 14; x++)
                                    if (!((y == 5 + bob) && (x == 9 || x == 14)))
                                        spr[x, y] = Color.FromArgb(204, 168, 134);
                            spr[10, 7 + bob] = Color.FromArgb(255, 225, 50);
                            spr[13, 7 + bob] = Color.FromArgb(255, 225, 50);
                            spr[16, 14 + bob] = Color.FromArgb(145, 255, 55);

                        } else {
                            // MUTON: Hulking V-Tapered Muscular Green Bio-Armor + Purple Cyber-Helmet + Heavy Plasma!
                            for (int y = 19 + (pose == 1 ? 2 : 0); y < 27; y++) {
                                spr[8  + legL/2, y] = Color.FromArgb(156, 72, 188);
                                spr[9  + legL/2, y] = Color.FromArgb(112, 44, 142);
                                spr[14 + legR/2, y] = Color.FromArgb(156, 72, 188);
                                spr[15 + legR/2, y] = Color.FromArgb(112, 44, 142);
                            }
                            // Broad shoulder pauldrons tapering to muscular waist + pectoral shading
                            for (int y = 10 + yOff; y < 19 + (pose == 1 ? 2 : 0); y++) {
                                int dyT = y - (10 + yOff);
                                int hw = (dyT <= 3) ? 6 : (dyT <= 6) ? 5 : 4;
                                for (int x = 12 - hw; x <= 11 + hw; x++) {
                                    bool belt = (y == 18 + yOff);
                                    bool pecLine = (dyT == 4 && x >= 9 && x <= 14);
                                    spr[x, y] = belt    ? Color.FromArgb(148, 66, 178) :
                                                pecLine ? Color.FromArgb(36, 108, 26) :
                                                (x < 10)? Color.FromArgb(108, 224, 74) :
                                                (x < 14)? Color.FromArgb(74, 184, 48)  :
                                                          Color.FromArgb(44, 128, 30);
                                }
                            }
                            // Contoured Purple/Magenta Helmeted Skull
                            for (int y = 4 + yOff; y < 10 + yOff; y++) {
                                int hw = (y == 4 + yOff) ? 2 : 3;
                                for (int x = 12 - hw; x <= 11 + hw; x++)
                                    spr[x, y] = (x < 12) ? Color.FromArgb(182, 88, 212) : Color.FromArgb(128, 52, 158);
                            }
                            spr[10, 7 + yOff] = Color.FromArgb(255, 230, 55);
                            spr[13, 7 + yOff] = Color.FromArgb(255, 230, 55);
                            // Two-handed Heavy Plasma Cannon across chest
                            for (int gx = 11; gx <= 18; gx++) {
                                spr[gx, 15 + yOff] = (gx >= 16) ? Color.FromArgb(155, 255, 65) : Color.FromArgb(92, 104, 118);
                            }
                        }

                        // Apply 1px dark silhouette outline around non-transparent pixels!
                        for (int py = 1; py < 31; py++) {
                            for (int px = 1; px < 23; px++) {
                                if (spr[px, py] != Trans) {
                                    PSet(bmp, ox + px, oy + py, spr[px, py]);
                                } else {
                                    if (spr[px-1, py] != Trans || spr[px+1, py] != Trans ||
                                        spr[px, py-1] != Trans || spr[px, py+1] != Trans) {
                                        PSet(bmp, ox + px, oy + py, Color.FromArgb(16, 18, 22));
                                    }
                                }
                            }
                        }
                    }
                }
            }
            SaveBmp24(bmp, path);
        }
    }

    static void GenDeaths(string path) {
        // 4 cols (stage 0..3: Agony Hit, Knee Buckle, Collapse, Persistent Prone Corpse)
        // x 9 rows (sprite_id 0..8: 6 DUFF-COM Operatives + Sectoid + Floater + Muton) of 32x32 = 128 x 288
        using (Bitmap bmp = new Bitmap(128, 288)) {
            using (Graphics g = Graphics.FromImage(bmp)) g.Clear(Trans);

            for (int type = 0; type < 9; type++) {
                int oy = type * 32;

                for (int stg = 0; stg < 4; stg++) {
                    int ox = stg * 32;
                    Color[,] spr = new Color[32, 32];
                    for (int py = 0; py < 32; py++)
                        for (int px = 0; px < 32; px++) spr[px, py] = Trans;

                    // Ground shadow on stages 0..2
                    if (stg < 3) {
                        int sy0 = (stg == 0) ? 24 : 23;
                        for (int y = sy0; y <= sy0 + 3; y++)
                            for (int x = 9; x <= 23; x++)
                                if ((x - 16) * (x - 16) + (y - (sy0 + 1)) * (y - (sy0 + 1)) * 5 <= 42)
                                    PSet(bmp, ox + x, oy + y, Color.FromArgb(28, 32, 38));
                    }

                    if (type < 6) {
                        // ====================================================================
                        // DUFF-COM PERSONAL ARMOR OPERATIVES (Matching all 6 hair/skin/visors)
                        // ====================================================================
                        Color skinHi = (type == 2) ? Color.FromArgb(156, 96, 58)  :
                                       (type == 5) ? Color.FromArgb(182, 122, 82) :
                                                     Color.FromArgb(242, 188, 152);
                        Color skinSh = (type == 2) ? Color.FromArgb(116, 68, 38)  :
                                       (type == 5) ? Color.FromArgb(142, 92, 58)  :
                                                     Color.FromArgb(204, 146, 112);
                        Color hairHi, hairSh;
                        switch (type) {
                            case 0: hairHi = Color.FromArgb(124, 76, 40);  hairSh = Color.FromArgb(78, 46, 24);   break;
                            case 1: hairHi = Color.FromArgb(252, 224, 88); hairSh = Color.FromArgb(198, 156, 44); break;
                            case 2: hairHi = Color.FromArgb(44, 36, 34);   hairSh = Color.FromArgb(24, 20, 20);   break;
                            case 3: hairHi = Color.FromArgb(224, 78, 38);  hairSh = Color.FromArgb(158, 44, 22);  break;
                            case 4: hairHi = Color.FromArgb(250, 220, 96); hairSh = Color.FromArgb(204, 164, 54); break;
                            default:hairHi = Color.FromArgb(52, 62, 84);   hairSh = Color.FromArgb(24, 28, 38);   break;
                        }
                        Color armHi = Color.FromArgb(92, 158, 245);
                        Color armMd = Color.FromArgb(66, 124, 212);
                        Color armSh = Color.FromArgb(42, 82, 154);
                        Color silv  = Color.FromArgb(204, 218, 238);
                        Color bootC = Color.FromArgb(62, 68, 78);
                        Color bldHi = Color.FromArgb(218, 30, 36);
                        Color bldDk = Color.FromArgb(132, 16, 22);

                        if (stg == 0) {
                            // Stage 0: Fatal hit! Thrown backward, arms flung wide, rifle flying, chest blood burst
                            for (int y = 18; y <= 24; y++) {
                                spr[12, y] = armMd; spr[13, y] = armSh;
                                spr[17, y] = armMd; spr[18, y] = armSh;
                            }
                            spr[11, 25] = bootC; spr[12, 25] = bootC; spr[13, 25] = bootC;
                            spr[17, 25] = bootC; spr[18, 25] = bootC; spr[19, 25] = bootC;
                            for (int y = 10; y <= 17; y++)
                                for (int x = 11; x <= 19; x++)
                                    spr[x, y] = (y <= 12 && (x <= 12 || x >= 18)) ? silv : (x < 15 ? armHi : armSh);
                            // Flung-out arms
                            for (int i = 0; i < 4; i++) {
                                spr[10 - i, 12 - i / 2] = armHi;
                                spr[20 + i, 12 + i / 2] = armMd;
                            }
                            // Thrown-back head + screaming mouth
                            for (int y = 4; y <= 9; y++)
                                for (int x = 11; x <= 16; x++)
                                    spr[x, y] = (y <= 5 || x == 11) ? hairHi : (x < 14 ? skinHi : skinSh);
                            if (type == 0 || type == 5) {
                                spr[13, 6] = Color.FromArgb(95, 252, 255);
                                spr[14, 6] = Color.FromArgb(38, 195, 225);
                            }
                            spr[14, 8] = Color.FromArgb(36, 14, 18);
                            // Flying rifle
                            for (int i = 0; i < 6; i++)
                                spr[21 + i, 15 + i / 2] = (i == 5) ? Color.FromArgb(250, 220, 65) : Color.FromArgb(168, 180, 198);
                            // Impact blood burst
                            spr[14, 12] = bldHi; spr[15, 13] = bldHi; spr[16, 12] = bldDk;
                            spr[13, 14] = bldDk; spr[17, 11] = bldHi; spr[19, 10] = bldHi;
                        } else if (stg == 1) {
                            // Stage 1: Buckling onto knees, clutching chest, pitching down-left
                            for (int y = 21; y <= 25; y++)
                                for (int x = 13; x <= 21; x++)
                                    if (x <= 16 || x >= 18) spr[x, y] = (y == 25) ? bootC : armSh;
                            for (int y = 13; y <= 21; y++)
                                for (int x = 10; x <= 18; x++)
                                    spr[x, y] = (y <= 15 && x <= 12) ? silv : (x < 14 ? armHi : armMd);
                            for (int y = 8; y <= 13; y++)
                                for (int x = 8; x <= 13; x++)
                                    spr[x, y] = (y <= 10) ? hairHi : skinSh;
                            // Clattering rifle & blood splash
                            for (int x = 19; x <= 25; x++) spr[x, 24] = Color.FromArgb(156, 168, 186);
                            spr[13, 17] = bldHi; spr[14, 18] = bldDk; spr[14, 24] = bldDk; spr[15, 25] = bldHi;
                        } else if (stg == 2) {
                            // Stage 2: Crashing horizontally toward prone
                            for (int y = 20; y <= 25; y++)
                                for (int x = 18; x <= 25; x++)
                                    spr[x, y] = (x >= 23) ? bootC : armSh;
                            for (int y = 18; y <= 24; y++)
                                for (int x = 11; x <= 18; x++)
                                    spr[x, y] = (x <= 13 && y <= 20) ? silv : (y <= 20 ? armHi : armMd);
                            for (int y = 16; y <= 21; y++)
                                for (int x = 7; x <= 11; x++)
                                    spr[x, y] = (x <= 8 || y <= 17) ? hairHi : skinHi;
                            for (int x = 11; x <= 18; x++) spr[x, 26] = Color.FromArgb(148, 160, 178);
                            spr[14, 23] = bldHi; spr[15, 24] = bldDk;
                        } else {
                            // Stage 3: Persistent Prone Operative Corpse (angled naturally inside floor diamond!)
                            for (int y = 20; y <= 27; y++)
                                for (int x = 10; x <= 20; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 24)
                                        spr[x, y] = ((x + y) & 1) == 0 ? bldDk : bldHi;
                            // Splayed armored legs & combat boots along isometric floor
                            for (int i = 0; i < 7; i++) {
                                spr[17 + i, 22 + i / 3] = (i >= 5) ? bootC : armMd;
                                spr[17 + i, 23 + i / 3] = (i >= 5) ? bootC : armSh;
                                if (i < 6) spr[16 + i, 25 + i / 3] = (i >= 4) ? bootC : armSh;
                            }
                            // Personal Armor chestplate, gold belt & titanium pauldron
                            for (int y = 21; y <= 25; y++) {
                                int rowShift = (y - 21) / 2;
                                for (int x = 11 + rowShift; x <= 17 + rowShift; x++)
                                    spr[x, y] = (x == 17 + rowShift) ? Color.FromArgb(248, 212, 52) :
                                                (x <= 12 + rowShift && y <= 23) ? silv :
                                                (y <= 22) ? armHi : (y <= 24 ? armMd : armSh);
                            }
                            // Outstretched arms (one forward-left, one up-right)
                            spr[10, 24] = armMd; spr[9, 25] = armHi; spr[8, 25] = skinSh;
                            spr[13, 19] = armMd; spr[14, 19] = skinHi;
                            // Fallen head with recognizable hair & visor!
                            for (int y = 19; y <= 23; y++)
                                for (int x = 7; x <= 11; x++)
                                    if (!((y == 19 || y == 23) && (x == 7 || x == 11)))
                                        spr[x, y] = (x <= 8 || y == 19) ? hairHi : (x == 9 && y <= 20 ? hairSh : skinHi);
                            if (type == 0 || type == 5) {
                                spr[10, 20] = Color.FromArgb(95, 252, 255);
                                spr[10, 21] = Color.FromArgb(38, 195, 225);
                            }
                            // Dropped rifle beside body
                            for (int x = 11; x <= 17; x++)
                                spr[x, 28] = (x == 11) ? Color.FromArgb(250, 220, 65) : Color.FromArgb(160, 172, 190);
                        }

                    } else if (type == 6) {
                        // ====================================================================
                        // SECTOID: Gray Alien collapse + green bio-ichor pool
                        // ====================================================================
                        Color grHi = Color.FromArgb(196, 202, 190);
                        Color grMd = Color.FromArgb(156, 162, 150);
                        Color grSh = Color.FromArgb(116, 122, 112);
                        Color ichHi = Color.FromArgb(132, 248, 56);
                        Color ichDk = Color.FromArgb(58, 168, 34);

                        if (stg == 0) {
                            for (int y = 19; y <= 25; y++) { spr[13, y] = grMd; spr[17, y] = grSh; }
                            for (int y = 12; y <= 18; y++)
                                for (int x = 13; x <= 17; x++) spr[x, y] = (x < 15) ? grMd : grSh;
                            for (int i = 0; i < 4; i++) { spr[12 - i, 13 - i / 2] = grMd; spr[18 + i, 13 + i / 2] = grSh; }
                            for (int y = 4; y <= 11; y++)
                                for (int x = 11; x <= 18; x++)
                                    if (!((y == 4 || y == 11) && (x == 11 || x == 18)))
                                        spr[x, y] = (x < 15) ? grHi : grMd;
                            spr[12, 7] = Color.FromArgb(16, 18, 24); spr[13, 8] = Color.FromArgb(16, 18, 24);
                            spr[16, 7] = Color.FromArgb(16, 18, 24); spr[15, 8] = Color.FromArgb(16, 18, 24);
                            spr[15, 14] = ichHi; spr[16, 13] = ichHi; spr[17, 15] = ichDk; spr[19, 12] = ichHi;
                        } else if (stg == 1) {
                            for (int y = 21; y <= 25; y++) { spr[14, y] = grSh; spr[15, y] = grMd; spr[18, y] = grSh; }
                            for (int y = 15; y <= 21; y++)
                                for (int x = 12; x <= 17; x++) spr[x, y] = (x < 15) ? grMd : grSh;
                            for (int y = 9; y <= 16; y++)
                                for (int x = 8; x <= 14; x++)
                                    if (!((y == 9 || y == 16) && (x == 8 || x == 14)))
                                        spr[x, y] = (x < 11) ? grHi : grMd;
                            spr[9, 13] = Color.FromArgb(16, 18, 24); spr[10, 14] = Color.FromArgb(16, 18, 24);
                            spr[14, 24] = ichDk; spr[15, 25] = ichHi;
                        } else if (stg == 2) {
                            for (int y = 21; y <= 25; y++)
                                for (int x = 13; x <= 23; x++)
                                    if (y <= 23 || x >= 19) spr[x, y] = (x < 18) ? grMd : grSh;
                            for (int y = 17; y <= 23; y++)
                                for (int x = 7; x <= 13; x++)
                                    spr[x, y] = (x < 10) ? grHi : grMd;
                            spr[8, 20] = Color.FromArgb(16, 18, 24); spr[9, 21] = Color.FromArgb(16, 18, 24);
                        } else {
                            // Stage 3: Persistent Prone Sectoid Corpse
                            for (int y = 20; y <= 27; y++)
                                for (int x = 11; x <= 20; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 22)
                                        spr[x, y] = ((x + y) & 1) == 0 ? ichDk : ichHi;
                            // Slender gray torso & sprawled thin alien limbs
                            for (int y = 22; y <= 25; y++)
                                for (int x = 12; x <= 18; x++) spr[x, y] = (y <= 23) ? grMd : grSh;
                            for (int i = 0; i < 6; i++) {
                                spr[18 + i, 22 + i / 2] = grSh;
                                spr[17 + i, 25 + i / 3] = grMd;
                            }
                            spr[11, 25] = grMd; spr[10, 26] = grHi;
                            // Oversized pale-gray cranium + dark slanted eye
                            for (int y = 19; y <= 24; y++)
                                for (int x = 6; x <= 12; x++)
                                    if (!((y == 19 || y == 24) && (x == 6 || x == 12)))
                                        spr[x, y] = (y <= 21 && x <= 10) ? grHi : grMd;
                            spr[8, 22] = Color.FromArgb(16, 18, 24);
                            spr[9, 23] = Color.FromArgb(16, 18, 24);
                            // Dropped Elerium Plasma Pistol
                            spr[13, 28] = ichHi; spr[14, 28] = Color.FromArgb(205, 255, 120);
                        }

                    } else if (type == 7) {
                        // ====================================================================
                        // FLOATER: Crimson cape + sparking hover-thruster + cybernetic corpse
                        // ====================================================================
                        Color cpHi = Color.FromArgb(224, 52, 46);
                        Color cpSh = Color.FromArgb(148, 26, 26);
                        Color stHi = Color.FromArgb(182, 190, 202);
                        Color stSh = Color.FromArgb(98, 106, 118);
                        Color cybSkin = Color.FromArgb(204, 168, 134);

                        if (stg == 0) {
                            // Thruster exploding in sparks + crimson cape & cyber-arms flaring wide
                            for (int y = 18; y <= 24; y++)
                                for (int x = 12; x <= 19; x++)
                                    spr[x, y] = ((x + y) & 1) == 0 ? Color.FromArgb(255, 228, 65) : Color.FromArgb(255, 120, 24);
                            for (int y = 9; y <= 18; y++)
                                for (int x = 10; x <= 21; x++)
                                    spr[x, y] = (x >= 13 && x <= 17 && y >= 11 && y <= 15) ? stHi : (x < 16 ? cpHi : cpSh);
                            for (int i = 0; i < 4; i++) { spr[9 - i, 11 - i / 2] = cpHi; spr[22 + i, 12 + i / 2] = stHi; }
                            for (int y = 4; y <= 9; y++)
                                for (int x = 12; x <= 17; x++) spr[x, y] = cybSkin;
                            spr[13, 6] = Color.FromArgb(255, 225, 50); spr[16, 6] = Color.FromArgb(255, 225, 50);
                        } else if (stg == 1) {
                            for (int y = 19; y <= 25; y++)
                                for (int x = 15; x <= 21; x++) spr[x, y] = (y >= 23) ? Color.FromArgb(255, 145, 32) : stSh;
                            for (int y = 11; y <= 20; y++)
                                for (int x = 10; x <= 20; x++)
                                    spr[x, y] = (x >= 13 && x <= 16 && y >= 14) ? stHi : (x < 15 ? cpHi : cpSh);
                            for (int y = 9; y <= 14; y++)
                                for (int x = 8; x <= 12; x++) spr[x, y] = cybSkin;
                        } else if (stg == 2) {
                            for (int y = 20; y <= 25; y++)
                                for (int x = 18; x <= 25; x++) spr[x, y] = (x < 22) ? stHi : stSh;
                            for (int y = 17; y <= 25; y++)
                                for (int x = 10; x <= 19; x++) spr[x, y] = (y <= 20) ? cpHi : cpSh;
                            for (int y = 17; y <= 21; y++)
                                for (int x = 7; x <= 11; x++) spr[x, y] = cybSkin;
                        } else {
                            // Stage 3: Persistent Prone Floater Corpse
                            for (int y = 20; y <= 27; y++)
                                for (int x = 11; x <= 20; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 22)
                                        spr[x, y] = Color.FromArgb(128, 22, 42);
                            // Charred anti-grav thruster pod angled SE
                            for (int y = 22; y <= 26; y++)
                                for (int x = 18; x <= 24; x++)
                                    if (y - 22 <= (x - 18) / 2 + 2)
                                        spr[x, y] = (y <= 23 && x <= 21) ? stHi : stSh;
                            // Draped crimson armored cloak & silver harness + cyber arm
                            for (int y = 20; y <= 26; y++)
                                for (int x = 10; x <= 18; x++)
                                    if (!((y == 20 && x >= 16) || (y == 26 && x <= 11)))
                                        spr[x, y] = (x >= 13 && x <= 15 && y >= 22 && y <= 24) ? stHi : (y <= 22 ? cpHi : cpSh);
                            spr[9, 25] = stHi; spr[10, 25] = cpHi;
                            // Cybernetic head
                            for (int y = 19; y <= 23; y++)
                                for (int x = 7; x <= 11; x++) spr[x, y] = cybSkin;
                            spr[8, 21] = Color.FromArgb(196, 150, 32);
                        }

                    } else {
                        // ====================================================================
                        // MUTON: Hulking Green Bio-Armor + Purple Cyber-Suit + Green Ichor
                        // ====================================================================
                        Color gnHi = Color.FromArgb(108, 224, 74);
                        Color gnMd = Color.FromArgb(74, 184, 48);
                        Color gnSh = Color.FromArgb(44, 128, 30);
                        Color prHi = Color.FromArgb(182, 88, 212);
                        Color prSh = Color.FromArgb(118, 46, 148);
                        Color ichHi = Color.FromArgb(145, 255, 65);
                        Color ichDk = Color.FromArgb(58, 168, 34);

                        if (stg == 0) {
                            for (int y = 18; y <= 25; y++) {
                                spr[11, y] = prHi; spr[12, y] = prSh;
                                spr[18, y] = prHi; spr[19, y] = prSh;
                            }
                            for (int y = 9; y <= 18; y++)
                                for (int x = 9; x <= 21; x++)
                                    spr[x, y] = (y == 18) ? prSh : (x < 13 ? gnHi : (x < 18 ? gnMd : gnSh));
                            for (int i = 0; i < 4; i++) { spr[8 - i, 11 - i / 2] = gnHi; spr[22 + i, 12 + i / 2] = gnMd; }
                            for (int y = 3; y <= 9; y++)
                                for (int x = 12; x <= 18; x++) spr[x, y] = (x < 15) ? prHi : prSh;
                            spr[13, 6] = Color.FromArgb(255, 230, 55); spr[16, 6] = Color.FromArgb(255, 230, 55);
                            for (int x = 21; x <= 27; x++) spr[x, 16] = (x >= 25) ? ichHi : Color.FromArgb(92, 104, 118);
                            spr[14, 12] = ichHi; spr[15, 13] = ichHi; spr[16, 14] = ichDk;
                        } else if (stg == 1) {
                            for (int y = 20; y <= 25; y++)
                                for (int x = 12; x <= 22; x++)
                                    if (x <= 15 || x >= 18) spr[x, y] = prSh;
                            for (int y = 12; y <= 20; y++)
                                for (int x = 9; x <= 20; x++)
                                    spr[x, y] = (x < 13) ? gnHi : (x < 17 ? gnMd : gnSh);
                            for (int y = 8; y <= 14; y++)
                                for (int x = 7; x <= 13; x++) spr[x, y] = (x < 10) ? prHi : prSh;
                            spr[15, 24] = ichDk; spr[16, 25] = ichHi;
                        } else if (stg == 2) {
                            for (int y = 20; y <= 26; y++)
                                for (int x = 18; x <= 25; x++) spr[x, y] = (y <= 22) ? prHi : prSh;
                            for (int y = 17; y <= 25; y++)
                                for (int x = 10; x <= 19; x++) spr[x, y] = (y <= 20) ? gnHi : (y <= 23 ? gnMd : gnSh);
                            for (int y = 17; y <= 22; y++)
                                for (int x = 6; x <= 11; x++) spr[x, y] = prHi;
                        } else {
                            // Stage 3: Persistent Prone Hulking Muton Corpse
                            for (int y = 19; y <= 28; y++)
                                for (int x = 10; x <= 21; x++)
                                    if ((x - 15) * (x - 15) + (y - 24) * (y - 24) * 3 <= 26)
                                        spr[x, y] = ((x + y) & 1) == 0 ? ichDk : ichHi;
                            // Thick splayed purple bio-suit legs
                            for (int i = 0; i < 7; i++) {
                                spr[18 + i, 22 + i / 3] = prHi;
                                spr[18 + i, 23 + i / 3] = prSh;
                                if (i < 6) spr[17 + i, 25 + i / 3] = prSh;
                            }
                            // Broad green bio-armor chestplate + muscular arm
                            for (int y = 20; y <= 25; y++) {
                                int rs = (y - 20) / 3;
                                for (int x = 10 + rs; x <= 18 + rs; x++)
                                    spr[x, y] = (x == 18 + rs) ? prSh : (y <= 21 ? gnHi : (y <= 23 ? gnMd : gnSh));
                            }
                            spr[9, 24] = gnMd; spr[8, 25] = gnHi;
                            // Purple helmeted skull
                            for (int y = 19; y <= 23; y++)
                                for (int x = 6; x <= 10; x++) spr[x, y] = (y <= 20) ? prHi : prSh;
                            spr[8, 21] = Color.FromArgb(255, 230, 55);
                            // Dropped Heavy Plasma Cannon
                            for (int x = 11; x <= 18; x++)
                                spr[x, 28] = (x <= 12) ? ichHi : Color.FromArgb(92, 104, 118);
                        }
                    }

                    // For Stage 3 (persistent corpse), clip pixels strictly inside the 32x16 floor diamond
                    // (leaving 1px margin for the dark silhouette outline so corpses never poke through walls!)
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
                                PSet(bmp, ox + px, oy + py, spr[px, py]);
                            } else {
                                if (spr[px - 1, py] != Trans || spr[px + 1, py] != Trans ||
                                    spr[px, py - 1] != Trans || spr[px, py + 1] != Trans) {
                                    PSet(bmp, ox + px, oy + py, Color.FromArgb(16, 18, 22));
                                }
                            }
                        }
                    }
                }
            }
            SaveBmp24(bmp, path);
        }
    }

    static void GenFx(string path) {
        // 8 cols x 8 rows of 24x24 = 192 x 192
        // Row 0: PROJ_BULLET (8 isometric screen angles)
        // Row 1: PROJ_LASER  (8 isometric screen angles)
        // Row 2: PROJ_PLASMA (8 isometric screen angles)
        // Row 3: Thrown HE Grenade [cols 0..3] & Night Electro-Flare [cols 4..7]
        // Row 4: Muzzle Flashes [cols 0..2] & Targeting Reticles [cols 3..4]
        // Row 5: SPLASH_BULLET [cols 0..3] & SPLASH_LASER [cols 4..7]
        // Row 6: SPLASH_PLASMA [cols 0..3] & SPLASH_GLASS [cols 4..7]
        // Row 7: SPLASH_HIT_HUMAN [cols 0..3] & SPLASH_HIT_ALIEN [cols 4..7]
        using (Bitmap bmp = new Bitmap(192, 192)) {
            using (Graphics g = Graphics.FromImage(bmp)) g.Clear(Trans);

            double[] uxTab = new double[] {  0.0,  0.8944,  1.0,  0.8944,  0.0, -0.8944, -1.0, -0.8944 };
            double[] uyTab = new double[] { -1.0, -0.4472,  0.0,  0.4472,  1.0,  0.4472,  0.0, -0.4472 };

            // 1. ROWS 0..2: Directional Weapon Ammunition Sprites
            for (int dir = 0; dir < 8; dir++) {
                double ux = uxTab[dir], uy = uyTab[dir];
                double nx = -uy, ny = ux;
                int ox = dir * 24;

                // Row 0: PROJ_BULLET — High-velocity copper/brass tracer bullet + supersonic wake
                {
                    int oy = 0 * 24;
                    Color[,] cell = new Color[24, 24];
                    for (int py = 0; py < 24; py++)
                        for (int px = 0; px < 24; px++) cell[px, py] = Trans;

                    for (int py = 1; py < 23; py++) {
                        for (int px = 1; px < 23; px++) {
                            double dx = px - 12.0, dy = py - 12.0;
                            double along = dx * ux + dy * uy;
                            double perp  = Math.Abs(dx * nx + dy * ny);

                            if (along >= 1.2 && along <= 5.2 && perp <= 1.25 - (along - 1.2) * 0.22) {
                                // Pointed gleaming brass/copper bullet nose + white tip glint
                                cell[px, py] = (along >= 4.0) ? Color.White :
                                               (perp <= 0.55) ? Color.FromArgb(255, 228, 84) :
                                                                Color.FromArgb(214, 146, 42);
                            } else if (along >= -2.5 && along < 1.2 && perp <= 0.95) {
                                // Hot tracer base
                                cell[px, py] = (perp <= 0.45) ? Color.FromArgb(255, 205, 68) :
                                                                Color.FromArgb(238, 118, 28);
                            } else if (along >= -7.5 && along < -2.5 && perp <= 0.72) {
                                // Fading supersonic vapor / ember streak
                                if (((px + py) & 1) == 0 || along > -5.0)
                                    cell[px, py] = (along > -4.8) ? Color.FromArgb(215, 96, 28) :
                                                                    Color.FromArgb(148, 148, 156);
                            }
                        }
                    }
                    // Dark silhouette around the metallic bullet head so it pops over bright pavement/daylight!
                    for (int py = 1; py < 23; py++) {
                        for (int px = 1; px < 23; px++) {
                            double along = (px - 12.0) * ux + (py - 12.0) * uy;
                            if (cell[px, py] != Trans) {
                                PSet(bmp, ox + px, oy + py, cell[px, py]);
                            } else if (along >= -1.0 &&
                                      (cell[px-1, py] != Trans || cell[px+1, py] != Trans ||
                                       cell[px, py-1] != Trans || cell[px, py+1] != Trans)) {
                                PSet(bmp, ox + px, oy + py, Color.FromArgb(24, 22, 20));
                            }
                        }
                    }
                }

                // Row 1: PROJ_LASER — Coopersburg Prototype coherent cyan/cobalt laser pulse bolt
                {
                    int oy = 1 * 24;
                    for (int py = 1; py < 23; py++) {
                        for (int px = 1; px < 23; px++) {
                            double dx = px - 12.0, dy = py - 12.0;
                            double along = dx * ux + dy * uy;
                            double perp  = Math.Abs(dx * nx + dy * ny);
                            // Tapered caps at front (+6.5) and tail (-6.5)
                            double maxPerp = 2.2 - (Math.Abs(along) > 4.2 ? (Math.Abs(along) - 4.2) * 0.75 : 0.0);
                            if (Math.Abs(along) <= 6.8 && maxPerp > 0 && perp <= maxPerp) {
                                Color lc = (perp <= 0.65 && Math.Abs(along) <= 5.0) ? Color.White :
                                           (perp <= 1.35 && Math.Abs(along) <= 5.8) ? Color.FromArgb(88, 250, 255) :
                                                                                      Color.FromArgb(32, 118, 236);
                                PSet(bmp, ox + px, oy + py, lc);
                            }
                        }
                    }
                }

                // Row 2: PROJ_PLASMA — Swirling Elerium-115 green plasma orb + ionized comet wake
                {
                    int oy = 2 * 24;
                    double headX = 12.0 + ux * 2.2;
                    double headY = 12.0 + uy * 2.2;
                    for (int py = 1; py < 23; py++) {
                        for (int px = 1; px < 23; px++) {
                            double dx = px - 12.0, dy = py - 12.0;
                            double along = dx * ux + dy * uy;
                            double perp  = Math.Abs(dx * nx + dy * ny);
                            double rHead = Math.Sqrt((px - headX) * (px - headX) + (py - headY) * (py - headY));

                            if (rHead <= 4.3) {
                                Color pc = (rHead <= 1.4) ? Color.FromArgb(245, 255, 210) :
                                           (rHead <= 2.5) ? Color.FromArgb(185, 255, 78)  :
                                           (rHead <= 3.5) ? Color.FromArgb(88, 238, 42)   :
                                                            Color.FromArgb(28, 128, 24);
                                PSet(bmp, ox + px, oy + py, pc);
                            } else if (along >= -8.2 && along <= 1.5) {
                                double wakeW = 2.4 * (1.0 - (1.5 - along) / 10.5);
                                if (perp <= wakeW && (((px * 3 + py * 5) & 3) != 0 || along > -3.5)) {
                                    Color wc = (perp <= wakeW * 0.45 && along > -4.5)
                                               ? Color.FromArgb(132, 252, 56)
                                               : Color.FromArgb(42, 158, 30);
                                    PSet(bmp, ox + px, oy + py, wc);
                                }
                            }
                        }
                    }
                }
            }

            // 2. ROW 3: Thrown Munitions — Spinning HE Grenade [0..3] & Sparking Electro-Flare [4..7]
            for (int f = 0; f < 4; f++) {
                // Cols 0..3: Segmented Olive-Drab Pineapple Frag Grenade
                {
                    int ox = f * 24, oy = 3 * 24;
                    for (int py = 6; py <= 18; py++) {
                        for (int px = 6; px <= 18; px++) {
                            double r = Math.Sqrt((px - 12.0) * (px - 12.0) + (py - 12.0) * (py - 12.0));
                            if (r <= 4.6) {
                                bool border = (r >= 3.7);
                                bool groove = (((px + f) % 3 == 0) || ((py + f) % 3 == 0));
                                Color gc = border ? Color.FromArgb(20, 26, 18) :
                                           groove ? Color.FromArgb(44, 78, 32) :
                                           (px < 12 && py < 12) ? Color.FromArgb(106, 168, 72) :
                                                                  Color.FromArgb(72, 124, 48);
                                PSet(bmp, ox + px, oy + py, gc);
                            }
                        }
                    }
                    // Rotating brass fuse & spoon lever
                    int[] fxOff = new int[] {  0,  4,  0, -4 };
                    int[] fyOff = new int[] { -5,  0,  5,  0 };
                    int fx = 12 + fxOff[f], fy = 12 + fyOff[f];
                    PSet(bmp, ox + fx, fy + oy, Color.FromArgb(252, 220, 64));
                    PSet(bmp, ox + (fx + 12) / 2, oy + (fy + 12) / 2, Color.FromArgb(196, 168, 48));
                }

                // Cols 4..7: Tumbling Red/Yellow Night Electro-Flare Stick with Sparks!
                {
                    int ox = (4 + f) * 24, oy = 3 * 24;
                    double ang = f * (Math.PI / 4.0);
                    double ca = Math.Cos(ang), sa = Math.Sin(ang);
                    for (int s = -5; s <= 5; s++) {
                        int px = (int)Math.Round(12.0 + ca * s);
                        int py = (int)Math.Round(12.0 + sa * s);
                        Color fc = (s >= 3) ? Color.White :
                                   (s == 2) ? Color.FromArgb(255, 228, 60) :
                                              Color.FromArgb(228, 48, 42);
                        PSet(bmp, ox + px, oy + py, fc);
                        PSet(bmp, ox + px - (int)Math.Round(sa), oy + py + (int)Math.Round(ca),
                             (s >= 3) ? Color.FromArgb(255, 190, 42) : Color.FromArgb(148, 26, 26));
                    }
                    // Ignited spark droplets around tip
                    int txp = (int)Math.Round(12.0 + ca * 6.0);
                    int typ = (int)Math.Round(12.0 + sa * 6.0);
                    PSet(bmp, ox + txp + 1, oy + typ - 1, Color.FromArgb(255, 238, 75));
                    PSet(bmp, ox + txp - 1, oy + typ + 1, Color.FromArgb(255, 138, 28));
                }
            }

            // 3. ROW 4: Weapon Muzzle Flashes [cols 0..2] & Tactical Reticles [cols 3..4]
            for (int m = 0; m < 3; m++) {
                int ox = m * 24, oy = 4 * 24;
                Color cCore = Color.White;
                Color cMid  = (m == 0) ? Color.FromArgb(255, 228, 68) :
                              (m == 1) ? Color.FromArgb(90, 250, 255) :
                                         Color.FromArgb(145, 255, 65);
                Color cOut  = (m == 0) ? Color.FromArgb(245, 118, 26) :
                              (m == 1) ? Color.FromArgb(36, 118, 236) :
                                         Color.FromArgb(42, 164, 32);

                for (int py = 2; py < 22; py++) {
                    for (int px = 2; px < 22; px++) {
                        int dx = px - 12, dy = py - 12;
                        double r = Math.Sqrt(dx * dx + dy * dy);
                        double ang = Math.Atan2(dy, dx);
                        double spikes = (m == 0) ? (5.5 + 3.2 * Math.Cos(6.0 * ang)) :
                                        (m == 1) ? (4.8 + 4.0 * Math.Cos(4.0 * ang)) :
                                                   (5.6 + 2.6 * Math.Sin(5.0 * ang));
                        if (r <= 2.2)      PSet(bmp, ox + px, oy + py, cCore);
                        else if (r <= 4.4) PSet(bmp, ox + px, oy + py, cMid);
                        else if (r <= spikes) PSet(bmp, ox + px, oy + py, cOut);
                    }
                }
            }

            // Cols 3 & 4 of Row 4: Battlefield Targeting Reticles (3 = Clear Lock-On, 4 = Blocked LOS)
            for (int rt = 0; rt < 2; rt++) {
                int ox = (3 + rt) * 24, oy = 4 * 24;
                Color[,] rcell = new Color[24, 24];
                for (int py = 0; py < 24; py++)
                    for (int px = 0; px < 24; px++) rcell[px, py] = Trans;

                Color crossC = (rt == 0) ? Color.FromArgb(248, 42, 42)  : Color.FromArgb(188, 64, 48);
                Color brkC   = (rt == 0) ? Color.FromArgb(255, 224, 64) : Color.FromArgb(196, 132, 42);

                // Outer corner rangefinder brackets (span 4..20 around center (12, 12))
                for (int s = 0; s < 3; s++) {
                    rcell[4 + s, 4]  = brkC; rcell[4, 4 + s]  = brkC;
                    rcell[20 - s, 4] = brkC; rcell[20, 4 + s] = brkC;
                    rcell[4 + s, 20] = brkC; rcell[4, 20 - s] = brkC;
                    rcell[20 - s, 20]= brkC; rcell[20, 20 - s]= brkC;
                }
                // Inner circular scope ring arcs + cardinal crosshairs
                for (int s = 3; s <= 7; s++) {
                    rcell[12 - s, 12] = crossC;
                    rcell[12 + s, 12] = crossC;
                    rcell[12, 12 - s] = crossC;
                    rcell[12, 12 + s] = crossC;
                }
                rcell[8, 7]  = crossC; rcell[7, 8]  = crossC;
                rcell[16, 7] = crossC; rcell[17, 8] = crossC;
                rcell[8, 17] = crossC; rcell[7, 16] = crossC;
                rcell[16, 17]= crossC; rcell[17, 16]= crossC;

                if (rt == 0) {
                    // Bright crisp bullseye dot right on exact hotspot (12, 12)!
                    rcell[12, 12] = Color.White;
                } else {
                    // Blocked shot 'X' pip at center
                    rcell[11, 11] = Color.FromArgb(245, 58, 48);
                    rcell[13, 13] = Color.FromArgb(245, 58, 48);
                    rcell[13, 11] = Color.FromArgb(245, 58, 48);
                    rcell[11, 13] = Color.FromArgb(245, 58, 48);
                    rcell[12, 12] = Color.FromArgb(255, 160, 48);
                }

                // Apply high-contrast dark outline around reticle so it reads over any terrain!
                for (int py = 1; py < 23; py++) {
                    for (int px = 1; px < 23; px++) {
                        if (rcell[px, py] != Trans) {
                            PSet(bmp, ox + px, oy + py, rcell[px, py]);
                        } else if (rcell[px-1, py] != Trans || rcell[px+1, py] != Trans ||
                                   rcell[px, py-1] != Trans || rcell[px, py+1] != Trans) {
                            PSet(bmp, ox + px, oy + py, Color.FromArgb(16, 18, 22));
                        }
                    }
                }
            }

            // 4. ROWS 5..7: Six 4-Frame 24x24 Impact Splash Animations!
            // 0=SPLASH_BULLET, 1=SPLASH_LASER, 2=SPLASH_PLASMA, 3=SPLASH_GLASS, 4=SPLASH_HIT_HUMAN, 5=SPLASH_HIT_ALIEN
            Random rng = new Random(199409);
            for (int spType = 0; spType < 6; spType++) {
                int baseRow = 5 + (spType / 2);
                int baseCol = (spType % 2) * 4;

                Color cCore = Color.White;
                Color cBright, cMid, cDebris;
                switch (spType) {
                    case 0: // SPLASH_BULLET: brass/orange ricochet sparks + gray/tan dust puff
                        cBright = Color.FromArgb(255, 228, 72);
                        cMid    = Color.FromArgb(242, 132, 32);
                        cDebris = Color.FromArgb(158, 152, 142);
                        break;
                    case 1: // SPLASH_LASER: electric cyan shockwave + cobalt/orange thermal sparks
                        cBright = Color.FromArgb(96, 252, 255);
                        cMid    = Color.FromArgb(38, 142, 248);
                        cDebris = Color.FromArgb(255, 184, 58);
                        break;
                    case 2: // SPLASH_PLASMA: Elerium-115 neon green plasma splash & bio-globules
                        cBright = Color.FromArgb(196, 255, 84);
                        cMid    = Color.FromArgb(88, 238, 42);
                        cDebris = Color.FromArgb(34, 142, 26);
                        break;
                    case 3: // SPLASH_GLASS: shattering cyan/white crystalline shards
                        cBright = Color.FromArgb(228, 252, 255);
                        cMid    = Color.FromArgb(92, 228, 248);
                        cDebris = Color.FromArgb(164, 196, 214);
                        break;
                    case 4: // SPLASH_HIT_HUMAN: armor spark + crimson blood spray
                        cBright = Color.FromArgb(255, 215, 75);
                        cMid    = Color.FromArgb(228, 32, 38);
                        cDebris = Color.FromArgb(142, 18, 24);
                        break;
                    default:// SPLASH_HIT_ALIEN: impact spark + glowing chartreuse alien bio-ichor
                        cBright = Color.FromArgb(225, 255, 95);
                        cMid    = Color.FromArgb(118, 242, 48);
                        cDebris = Color.FromArgb(48, 158, 34);
                        break;
                }

                // Deterministic ray angles per splash type
                double[] rayAng = new double[10];
                for (int r = 0; r < 10; r++) {
                    rayAng[r] = (r * 2.0 * Math.PI / 10.0) + (rng.NextDouble() - 0.5) * 0.35;
                }

                for (int f = 0; f < 4; f++) {
                    int ox = (baseCol + f) * 24;
                    int oy = baseRow * 24;

                    if (f == 0) {
                        // Frame 0: Sharp impact flash star + initial radial streaks
                        for (int py = 5; py <= 19; py++) {
                            for (int px = 5; px <= 19; px++) {
                                double d = Math.Sqrt((px - 12.0) * (px - 12.0) + (py - 12.0) * (py - 12.0));
                                if (d <= 2.3) PSet(bmp, ox + px, oy + py, cCore);
                                else if (d <= 4.2) PSet(bmp, ox + px, oy + py, cBright);
                            }
                        }
                        for (int r = 0; r < 8; r++) {
                            int sx = (int)Math.Round(12.0 + Math.Cos(rayAng[r]) * 5.2);
                            int sy = (int)Math.Round(12.0 + Math.Sin(rayAng[r]) * 5.2);
                            PSet(bmp, ox + sx, oy + sy, cMid);
                        }
                    } else if (f == 1) {
                        // Frame 1: Expanding shockwave ring + bright flying droplets/shards
                        for (int py = 2; py < 22; py++) {
                            for (int px = 2; px < 22; px++) {
                                double d = Math.Sqrt((px - 12.0) * (px - 12.0) + (py - 12.0) * (py - 12.0));
                                if (d >= 2.6 && d <= 5.4 && ((px + py) & 1) == 0) {
                                    PSet(bmp, ox + px, oy + py, (d <= 3.8) ? cBright : cMid);
                                }
                            }
                        }
                        for (int r = 0; r < 10; r++) {
                            for (int s = 4; s <= 7; s++) {
                                int sx = (int)Math.Round(12.0 + Math.Cos(rayAng[r]) * s);
                                int sy = (int)Math.Round(12.0 + Math.Sin(rayAng[r]) * s);
                                PSet(bmp, ox + sx, oy + sy, (s <= 5) ? cBright : cMid);
                            }
                        }
                    } else if (f == 2) {
                        // Frame 2: Wide splash burst + secondary smoke/debris/ichor globules
                        for (int py = 1; py < 23; py++) {
                            for (int px = 1; px < 23; px++) {
                                double d = Math.Sqrt((px - 12.0) * (px - 12.0) + (py - 12.0) * (py - 12.0));
                                if (d >= 4.5 && d <= 7.8 && (((px * 37 + py * 57) ^ (px * 13 + f * 19)) & 3) == 0) {
                                    PSet(bmp, ox + px, oy + py, cDebris);
                                }
                            }
                        }
                        for (int r = 0; r < 10; r++) {
                            double dist = 6.8 + (r & 1) * 2.0;
                            int sx = (int)Math.Round(12.0 + Math.Cos(rayAng[r]) * dist);
                            int sy = (int)Math.Round(12.0 + Math.Sin(rayAng[r]) * dist + 0.8);
                            PSet(bmp, ox + sx, oy + sy, (r % 3 == 0) ? cBright : cMid);
                            PSet(bmp, ox + sx + 1, oy + sy, cDebris);
                        }
                    } else {
                        // Frame 3: Dissipating outer dust/vapor motes & falling embers
                        for (int r = 0; r < 10; r++) {
                            double dist = 8.2 + (r % 3) * 1.1;
                            int sx = (int)Math.Round(12.0 + Math.Cos(rayAng[r]) * dist);
                            int sy = (int)Math.Round(12.0 + Math.Sin(rayAng[r]) * dist + 1.6);
                            PSet(bmp, ox + sx, oy + sy, (r & 1) == 0 ? cDebris : cMid);
                        }
                    }
                }
            }

            SaveBmp24(bmp, path);
        }
    }

    static void ExtractSheetGrid(Bitmap dst, string srcJpg, int dstStartSlot) {
        using (Bitmap src = new Bitmap(srcJpg)) {
            int cw = src.Width / 3;
            int ch = src.Height / 2;
            for (int r = 0; r < 2; r++) {
                for (int c = 0; c < 3; c++) {
                    int slot = dstStartSlot + r * 3 + c;
                    int ox = slot * 32;
                    // Inset slightly inside cell to skip divider lines & frame tight on head/shoulders
                    double sx0 = c * cw + cw * 0.04;
                    double sy0 = r * ch + ch * 0.02;
                    double sw  = cw * 0.92;
                    double sh  = ch * 0.95;

                    Color[,] raw = new Color[32, 36];
                    for (int py = 0; py < 36; py++) {
                        for (int px = 0; px < 32; px++) {
                            // High-quality 3x3 subpixel box filter per target pixel
                            int sumR = 0, sumG = 0, sumB = 0, cnt = 0;
                            for (int sy = 0; sy < 3; sy++) {
                                for (int sx = 0; sx < 3; sx++) {
                                    int ix = (int)(sx0 + ((px + (sx + 0.5) / 3.0) * sw) / 32.0);
                                    int iy = (int)(sy0 + ((py + (sy + 0.5) / 3.0) * sh) / 36.0);
                                    if (ix < 0) ix = 0; if (ix >= src.Width) ix = src.Width - 1;
                                    if (iy < 0) iy = 0; if (iy >= src.Height) iy = src.Height - 1;
                                    Color sc = src.GetPixel(ix, iy);
                                    sumR += sc.R; sumG += sc.G; sumB += sc.B;
                                    cnt++;
                                }
                            }
                            raw[px, py] = Color.FromArgb(sumR / cnt, sumG / cnt, sumB / cnt);
                        }
                    }

                    // Mild pixel-art unsharp mask so eyes, visors & hair pop cleanly at 32x36!
                    for (int py = 0; py < 36; py++) {
                        for (int px = 0; px < 32; px++) {
                            Color cc = raw[px, py];
                            if (px > 0 && px < 31 && py > 0 && py < 35) {
                                int avgR = (raw[px-1, py].R + raw[px+1, py].R + raw[px, py-1].R + raw[px, py+1].R) / 4;
                                int avgG = (raw[px-1, py].G + raw[px+1, py].G + raw[px, py-1].G + raw[px, py+1].G) / 4;
                                int avgB = (raw[px-1, py].B + raw[px+1, py].B + raw[px, py-1].B + raw[px, py+1].B) / 4;
                                int nr = Math.Min(254, Math.Max(0, cc.R + ((cc.R - avgR) * 2) / 5));
                                int ng = Math.Min(254, Math.Max(0, cc.G + ((cc.G - avgG) * 2) / 5));
                                int nb = Math.Min(254, Math.Max(0, cc.B + ((cc.B - avgB) * 2) / 5));
                                // Avoid accidental Hot Magenta (#FF00FF) transparency key!
                                if (nr >= 245 && ng <= 15 && nb >= 245) nr = 240;
                                cc = Color.FromArgb(nr, ng, nb);
                            }
                            PSet(dst, ox + px, py, cc);
                        }
                    }
                }
            }
        }
    }

    static void GenPortraits(string path) {
        // 12 detailed 90s tactical portraits (0..5 Male, 6..11 Female) x 32x36 = 384x36
        string toolsDir = Path.Combine(Path.GetDirectoryName(path), "..", "tools");
        string maleSrc  = Path.Combine(toolsDir, "portraits_male_src.jpg");
        string femSrc   = Path.Combine(toolsDir, "portraits_female_src.jpg");

        using (Bitmap bmp = new Bitmap(384, 36)) {
            if (File.Exists(maleSrc) && File.Exists(femSrc)) {
                ExtractSheetGrid(bmp, maleSrc, 0);
                ExtractSheetGrid(bmp, femSrc,  6);
            }
            SaveBmp24(bmp, path);
        }
    }

    public static void GenRadar(string path) {
        // 300 x 132 Full-Width Topographic CRT Map of the Lehigh Valley & Upper Bucks PA
        using (Bitmap bmp = new Bitmap(300, 132)) {
            for (int y = 0; y < 132; y++) {
                for (int x = 0; x < 300; x++) {
                    // Topographic elevation contours:
                    // 1) Blue Mountain / Kittatinny Ridge across the far North
                    double blueMtn  = Math.Exp(-Math.Pow((y - (8 + Math.Sin(x * 0.03) * 3.0)) / 7.5, 2.0));
                    // 2) South Mountain Ridge running SW (Macungie/Emmaus) -> NE (Saucon/Hellertown)
                    double southMtn = Math.Exp(-Math.Pow((y - (84 - x * 0.17)) / 13.0, 2.0));
                    // 3) Nockamixon / Richland Hills in the Southeast
                    double bucksHill = (x > 165) ? Math.Exp(-Math.Pow((y - 104) / 16.0, 2.0)) * 0.55 : 0.0;

                    double elev = Math.Min(1.15, blueMtn * 0.85 + southMtn * 0.95 + bucksHill);
                    int scan = ((y & 1) == 0) ? 3 : 0;
                    int g = 20 + (int)(elev * 36.0) + scan + (((x ^ y) & 3));
                    Color c = Color.FromArgb(6, Math.Min(92, g), 10);

                    // Subtle CRT latitude/longitude graticule every 30x22 px
                    if ((x % 30 == 0) || (y % 22 == 0)) {
                        c = Color.FromArgb(12, Math.Min(110, g + 15), 18);
                    }
                    PSet(bmp, x, y, c);
                }
            }
            using (Graphics gr = Graphics.FromImage(bmp)) {
                // Lehigh River (Whitehall -> Allentown -> Bethlehem -> Easton)
                using (Pen riverPen = new Pen(Color.FromArgb(36, 128, 198), 2f)) {
                    gr.DrawLines(riverPen, new Point[] {
                        new Point(98, 2), new Point(116, 22), new Point(124, 36),
                        new Point(156, 38), new Point(188, 32), new Point(228, 35), new Point(268, 26)
                    });
                }
                // Delaware River along the Eastern border (Easton -> Riegelsville -> Upper Bucks)
                using (Pen delPen = new Pen(Color.FromArgb(38, 134, 206), 2f)) {
                    gr.DrawLines(delPen, new Point[] {
                        new Point(276, 2), new Point(268, 26), new Point(279, 68),
                        new Point(286, 102), new Point(295, 130)
                    });
                }
                // Saucon Creek (Coopersburg -> Center Valley -> Hellertown -> Bethlehem) & Little Lehigh
                using (Pen creekPen = new Pen(Color.FromArgb(28, 98, 154), 1f)) {
                    gr.DrawLines(creekPen, new Point[] {
                        new Point(154, 80), new Point(172, 66), new Point(190, 52), new Point(190, 33)
                    });
                    gr.DrawLines(creekPen, new Point[] {
                        new Point(42, 76), new Point(88, 60), new Point(124, 38)
                    });
                }
                // Lake Nockamixon (Southeast of Quakertown)
                using (Brush lakeBr = new SolidBrush(Color.FromArgb(36, 122, 192))) {
                    gr.FillEllipse(lakeBr, 214, 104, 34, 10);
                }
                // I-476 PA Turnpike NE Extension (NW -> Quakertown) & I-78 / US-22 East-West Corridor
                using (Pen hwyPen = new Pen(Color.FromArgb(84, 152, 72), 1f)) {
                    gr.DrawLines(hwyPen, new Point[] {
                        new Point(58, 2), new Point(78, 42), new Point(100, 74),
                        new Point(132, 112), new Point(144, 130)
                    });
                    gr.DrawLines(hwyPen, new Point[] {
                        new Point(4, 46), new Point(82, 42), new Point(126, 40),
                        new Point(182, 44), new Point(228, 40), new Point(270, 30), new Point(297, 32)
                    });
                }
                // PA Route 309 North-South Arterial straight through Coopersburg HQ (158, 82)!
                using (Pen rte309 = new Pen(Color.FromArgb(204, 190, 52), 2f)) {
                    gr.DrawLines(rte309, new Point[] {
                        new Point(90, 2), new Point(122, 38), new Point(162, 62),
                        new Point(158, 82), new Point(170, 110), new Point(180, 130)
                    });
                }
                // Concentric Radar Range Rings centered on Coopersburg HQ (158, 82)
                using (Pen ringPen = new Pen(Color.FromArgb(40, 128, 50), 1f)) {
                    gr.DrawEllipse(ringPen, 158 - 34, 82 - 34, 68, 68);
                    gr.DrawEllipse(ringPen, 158 - 72, 82 - 72, 144, 144);
                    gr.DrawEllipse(ringPen, 158 - 112, 82 - 112, 224, 224);
                }
            }
            SaveBmp24(bmp, path);
        }
    }
}
"@

Add-Type -TypeDefinition $csharp -ReferencedAssemblies System.Drawing
if ($ObjectsOnly) {
    [DuffArtGen]::GenObjects((Join-Path (Resolve-Path "assets").Path "OBJECTS.BMP"))
    Write-Output "Generated assets/OBJECTS.BMP!"
} else {
    [DuffArtGen]::GenerateAll((Resolve-Path "assets").Path)
    & (Join-Path $PSScriptRoot "gen_new_assets.ps1")
    Write-Output "Generated BMP sprite sheets in assets/!"
}

