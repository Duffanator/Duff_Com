using System;
using System.Drawing;

/// <summary>
/// Projects a flat front-on wall panel texture onto the engine's isometric wall sprite (16 x 32).
/// Geometry (matches isomap.c: wall sprite drawn at tile_y - LEVEL_H, LEVEL_H = 24):
///   West wall (NW diamond edge): bottom edge runs from (0,32) up to (16,24); top edge is 24 px above it.
///   North wall (NE diamond edge): bottom edge runs from (0,24) down to (16,32).
/// Texture u runs along the wall (left to right), v from the top (0) to the bottom (1).
/// Source pixels that are magenta (#FF00FF) or transparent become holes (transparent sprite pixels).
/// </summary>
public static class WallProjector {
  static readonly int[] Bayer4 = new int[]{0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};
  const int W = 16, H = 32, WALL_HEIGHT = 24, SS = 4;

  static bool IsHole(Color c) {
    if (c.A < 128) return true;
    int r = c.R, g = c.G, b = c.B;
    return (r - g > 55) && (b - g > 45) && Math.Abs(r - b) < 120;   // loose: also catches JPEG-blended magenta fringe
  }

  // Returns W*H palette indices (256 = transparent)
  public static int[] Project(Bitmap src, int sx0, int sy0, int sw, int sh, double inset, bool north,
                              double shade, double contrast, double sat, double bright, double dither) {
    int[] outp = new int[W * H];
    for (int py = 0; py < H; py++) for (int px = 0; px < W; px++) {
      int inside = 0, solid = 0; double r = 0, g = 0, b = 0;
      for (int j = 0; j < SS; j++) for (int i = 0; i < SS; i++) {
        double x = px + (i + 0.5) / SS, y = py + (j + 0.5) / SS;
        double yb = north ? 24 + 0.5 * x : 32 - 0.5 * x;     // bottom edge of the wall at this column
        double yt = yb - WALL_HEIGHT;
        if (y < yt || y > yb) continue;
        inside++;
        double u = x / W, v = (y - yt) / WALL_HEIGHT;
        double tu = inset + u * (1 - 2 * inset), tv = inset + v * (1 - 2 * inset);
        int tx = sx0 + (int)Math.Max(0, Math.Min(sw - 1, tu * sw));
        int ty = sy0 + (int)Math.Max(0, Math.Min(sh - 1, tv * sh));
        Color c = src.GetPixel(tx, ty);
        if (IsHole(c)) continue;
        solid++; r += c.R; g += c.G; b += c.B;
      }
      if (inside < SS * SS / 2 || solid * 2 < inside) { outp[py * W + px] = 256; continue; }
      r /= solid; g /= solid; b /= solid;
      double lum = 0.299 * r + 0.587 * g + 0.114 * b;
      r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
      r = ((r - 128) * contrast + 128 + bright) * shade; g = ((g - 128) * contrast + 128 + bright) * shade; b = ((b - 128) * contrast + 128 + bright) * shade;
      if (dither > 0) { double t = (Bayer4[(py & 3) * 4 + (px & 3)] / 15.0 - 0.5) * 48.0 * dither; r += t; g += t; b += t; }
      r = Math.Max(0, Math.Min(255, r)); g = Math.Max(0, Math.Min(255, g)); b = Math.Max(0, Math.Min(255, b));
      outp[py * W + px] = GamePalette.Nearest(r, g, b);
    }
    return outp;
  }
}

