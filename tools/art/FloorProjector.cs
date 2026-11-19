using System;
using System.Drawing;
public static class FloorProjector {
  static readonly int[] Bayer4 = new int[]{0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};
  // Returns 32*16 palette indices (256 = transparent).
  public static int[] Project(Bitmap src, int sx0, int sy0, int sw, int sh, double inset, double zoom,
                              double dither, double contrast, double sat, double bright) {
    const int TW = 32, TH = 16, SS = 6;
    int[] outp = new int[TW * TH];
    double cu0 = inset, span = (1.0 - 2*inset) / zoom;
    double ucen = 0.5, vcen = 0.5;
    for (int py = 0; py < TH; py++) for (int px = 0; px < TW; px++) {
      double cnx = (px + 0.5 - TW/2.0) / (TW/2.0), cny = (py + 0.5 - TH/2.0) / (TH/2.0);
      if (Math.Abs(cnx) + Math.Abs(cny) > 1.0) { outp[py*TW+px] = 256; continue; }
      double r = 0, g = 0, b = 0;
      for (int j = 0; j < SS; j++) for (int i = 0; i < SS; i++) {
        double nx = (px + (i + 0.5)/SS - TW/2.0) / (TW/2.0);
        double ny = (py + (j + 0.5)/SS - TH/2.0) / (TH/2.0);
        double u = (nx + ny) / 2.0 + 0.5, v = (ny - nx) / 2.0 + 0.5;   // 0..1 across the diamond
        double tu = ucen + (u - 0.5) * span, tv = vcen + (v - 0.5) * span;
        int tx = sx0 + (int)Math.Max(0, Math.Min(sw - 1, tu * sw));
        int ty = sy0 + (int)Math.Max(0, Math.Min(sh - 1, tv * sh));
        Color c = src.GetPixel(tx, ty);
        r += c.R; g += c.G; b += c.B;
      }
      double n2 = SS * SS; r /= n2; g /= n2; b /= n2;
      double lum = 0.299*r + 0.587*g + 0.114*b;
      r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
      r = (r - 128) * contrast + 128 + bright; g = (g - 128) * contrast + 128 + bright; b = (b - 128) * contrast + 128 + bright;
      if (dither > 0) { double t = (Bayer4[(py&3)*4 + (px&3)] / 15.0 - 0.5) * 48.0 * dither; r += t; g += t; b += t; }
      r = Math.Max(0, Math.Min(255, r)); g = Math.Max(0, Math.Min(255, g)); b = Math.Max(0, Math.Min(255, b));
      outp[py*TW+px] = GamePalette.Nearest(r, g, b);
    }
    return outp;
  }
}

