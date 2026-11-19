using System;
using System.Drawing;
using System.Drawing.Imaging;

public static class SpritePacker {
  public static double LastScale = 0;
  static readonly int[] Bayer4 = new int[]{0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};

  static bool IsBg(Color c) {
    if (c.A < 128) return true;
    int r = c.R, g = c.G, b = c.B;
    return (r - g > 90) && (b - g > 90) && Math.Abs(r - b) < 90;
  }

  // Returns index grid (256 = transparent) of size cw x ch for every cell, concatenated.
  public static int[][] Pack(Bitmap src, int cols, int rows, int cw, int ch,
                             double fit, int bottomMargin, double dither,
                             double contrast, double sat, bool outline, bool flipx, double scaleOverride, int cellMargin) {
    int sw = src.Width / cols, sh = src.Height / rows;
    int n = cols * rows;
    int[] x0 = new int[n], x1 = new int[n], y0 = new int[n], y1 = new int[n];
    bool[][] fg = new bool[n][];
    bool[][] core = new bool[n][];
    double maxH = 1, maxW = 1;
    for (int k = 0; k < n; k++) {
      int cx = (k % cols) * sw, cy = (k / cols) * sh;
      fg[k] = new bool[sw * sh];
      int ax = sw, bx = -1, ay = sh, by = -1;
      for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        bool f = !IsBg(src.GetPixel(cx + x, cy + y)) && x >= cellMargin && y >= cellMargin && x < sw - cellMargin && y < sh - cellMargin;
        fg[k][y*sw+x] = f;
        if (f) { if (x < ax) ax = x; if (x > bx) bx = x; if (y < ay) ay = y; if (y > by) by = y; }
      }
      // "core" = foreground pixels with no background within 2px: used for colour sampling so
      // magenta-tinted JPEG edge pixels never leak into the sprite colours.
      core[k] = new bool[sw * sh];
      for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        if (!fg[k][y*sw+x]) continue;
        bool ok = true;
        for (int dy = -2; dy <= 2 && ok; dy++) for (int dx = -2; dx <= 2; dx++) {
          int xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= sw || yy >= sh || !fg[k][yy*sw+xx]) { ok = false; break; }
        }
        core[k][y*sw+x] = ok;
      }
      x0[k]=ax; x1[k]=bx; y0[k]=ay; y1[k]=by;
      if (bx >= 0) { maxH = Math.Max(maxH, by-ay+1); maxW = Math.Max(maxW, bx-ax+1); }
    }
    int outPad = outline ? 2 : 0;
    double scale = Math.Min((ch * fit) / maxH, (cw - outPad - 1) / maxW);   // dest px per source px
    if (scaleOverride > 0) scale = scaleOverride;
    LastScale = scale;
    int[][] res = new int[n][];
    for (int k = 0; k < n; k++) {
      int cx = (k % cols) * sw, cy = (k / cols) * sh;
      int[] outp = new int[cw * ch];
      for (int i = 0; i < outp.Length; i++) outp[i] = 256;
      if (x1[k] >= 0) {
        double bw = (x1[k]-x0[k]+1) * scale, bh = (y1[k]-y0[k]+1) * scale;
        double offx = (cw - bw) / 2.0;
        double offy = ch - bottomMargin - (outline ? 1 : 0) - bh;
        for (int oy = 0; oy < ch; oy++) for (int ox = 0; ox < cw; ox++) {
          double sx0 = x0[k] + (ox - offx) / scale, sx1 = x0[k] + (ox + 1 - offx) / scale;
          double sy0 = y0[k] + (oy - offy) / scale, sy1 = y0[k] + (oy + 1 - offy) / scale;
          int ix0 = (int)Math.Floor(sx0), ix1 = (int)Math.Ceiling(sx1) - 1;
          int iy0 = (int)Math.Floor(sy0), iy1 = (int)Math.Ceiling(sy1) - 1;
          double wsum = 0, fgw = 0, rs = 0, gs = 0, bs = 0;
          double cw0 = 0, cr = 0, cg = 0, cb = 0;     // core-only colour accumulators
          for (int y = iy0; y <= iy1; y++) for (int x = ix0; x <= ix1; x++) {
            double wx = Math.Min(x+1, sx1) - Math.Max(x, sx0);
            double wy = Math.Min(y+1, sy1) - Math.Max(y, sy0);
            double w = Math.Max(0, wx) * Math.Max(0, wy);
            wsum += w;
            if (x < 0 || y < 0 || x >= sw || y >= sh) continue;
            if (!fg[k][y*sw+x]) continue;
            Color c = src.GetPixel(cx + x, cy + y);
            fgw += w; rs += c.R * w; gs += c.G * w; bs += c.B * w;
            if (core[k][y*sw+x]) { cw0 += w; cr += c.R * w; cg += c.G * w; cb += c.B * w; }
          }
          if (wsum <= 0 || fgw / wsum < 0.5) continue;
          double r, g, b;
          if (cw0 > 0) { r = cr / cw0; g = cg / cw0; b = cb / cw0; }
          else {
            // edge-only pixel: borrow colour from the nearest core pixels (search outward)
            double ccx = (sx0 + sx1) / 2, ccy = (sy0 + sy1) / 2; double bestD = 1e18; Color bc = Color.Black;
            for (int yy = (int)ccy - 8; yy <= (int)ccy + 8; yy++) for (int xx = (int)ccx - 8; xx <= (int)ccx + 8; xx++) {
              if (xx < 0 || yy < 0 || xx >= sw || yy >= sh || !core[k][yy*sw+xx]) continue;
              double d = (xx-ccx)*(xx-ccx) + (yy-ccy)*(yy-ccy);
              if (d < bestD) { bestD = d; bc = src.GetPixel(cx + xx, cy + yy); }
            }
            if (bestD < 1e17) { r = bc.R; g = bc.G; b = bc.B; } else { r = rs / fgw; g = gs / fgw; b = bs / fgw; }
          }
          // contrast + saturation boost (averaging washes colors out)
          double lum = 0.299*r + 0.587*g + 0.114*b;
          r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
          r = (r - 128) * contrast + 128; g = (g - 128) * contrast + 128; b = (b - 128) * contrast + 128;
          if (dither > 0) {
            double t = (Bayer4[(oy & 3) * 4 + (ox & 3)] / 15.0 - 0.5) * 48.0 * dither;
            r += t; g += t; b += t;
          }
          r = Math.Max(0, Math.Min(255, r)); g = Math.Max(0, Math.Min(255, g)); b = Math.Max(0, Math.Min(255, b));
          outp[oy*cw+ox] = GamePalette.Nearest(r, g, b);
        }
        if (outline) {
          int[] o2 = (int[])outp.Clone();
          for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) {
            if (outp[y*cw+x] != 256) continue;
            bool adj = (x>0 && outp[y*cw+x-1]!=256) || (x<cw-1 && outp[y*cw+x+1]!=256) ||
                       (y>0 && outp[(y-1)*cw+x]!=256) || (y<ch-1 && outp[(y+1)*cw+x]!=256);
            if (adj) o2[y*cw+x] = 15;   // COL_BLACK
          }
          outp = o2;
        }
        if (flipx) {
          for (int y = 0; y < ch; y++) for (int x = 0; x < cw/2; x++) {
            int t = outp[y*cw+x]; outp[y*cw+x] = outp[y*cw+cw-1-x]; outp[y*cw+cw-1-x] = t;
          }
        }
      }
      res[k] = outp;
    }
    return res;
  }

}


