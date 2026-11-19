using System;
using System.Drawing;

public static class GamePalette {
  public static int[][] Pal = BuildPalette();   // 256 x {r8,g8,b8}

  static int[][] BuildPalette() {
    int[][] p = new int[256][];
    int[][] ramps = new int[][] {
      new int[]{63,63,63,  0, 0, 0}, new int[]{30,62,26,  3,12, 3}, new int[]{63,18,16, 12, 2, 2},
      new int[]{58,44,28, 12, 7, 3}, new int[]{28,44,63,  3, 6,15}, new int[]{24,63,63,  2,14,16},
      new int[]{63,58,14, 15,11, 2}, new int[]{63,38, 8, 16, 5, 1}, new int[]{52,26,62,  9, 3,14},
      new int[]{42,63,12,  6,14, 2}, new int[]{50,54,60,  8,10,14}, new int[]{46,50,34,  8,10, 6},
      new int[]{60,52,42, 12,10, 8}, new int[]{63,24,52, 14, 3,10}, new int[]{38,38,46,  5, 5, 8},
      new int[]{63,49,40, 14, 7, 5}
    };
    for (int r = 0; r < 16; r++)
      for (int i = 0; i < 16; i++) {
        int[] c = ramps[r];
        int r6 = c[0] + ((c[3]-c[0]) * i) / 15;
        int g6 = c[1] + ((c[4]-c[1]) * i) / 15;
        int b6 = c[2] + ((c[5]-c[2]) * i) / 15;
        p[r*16+i] = new int[]{ (r6*255)/63, (g6*255)/63, (b6*255)/63 };
      }
    return p;
  }

  public static int Nearest(double r, double g, double b) {
    int best = 0; double bd = 1e18;
    for (int i = 0; i < 255; i++) {
      double dr = r - Pal[i][0], dg = g - Pal[i][1], db = b - Pal[i][2];
      double d = dr*dr*3 + dg*dg*4 + db*db*2;   // same weights as the game's quantizer
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  }

  public static Color ToColor(int idx) {
    if (idx >= 256) return Color.FromArgb(255, 0, 255);
    return Color.FromArgb(Pal[idx][0], Pal[idx][1], Pal[idx][2]);
  }
}

