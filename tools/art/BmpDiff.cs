using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

/// <summary>Fast cell-level comparison of two 24bpp BMP files (used to prove a build only touched intended cells).</summary>
public static class BmpDiff {
  static byte[] Read(string path, out int w, out int h) {
    using (Bitmap raw = new Bitmap(new System.IO.MemoryStream(System.IO.File.ReadAllBytes(path)))) {
      w = raw.Width; h = raw.Height;
      BitmapData d = raw.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
      byte[] buf = new byte[d.Stride * h];
      System.Runtime.InteropServices.Marshal.Copy(d.Scan0, buf, 0, buf.Length);
      int stride = d.Stride; raw.UnlockBits(d);
      byte[] outb = new byte[w * h * 3];
      for (int y = 0; y < h; y++) Array.Copy(buf, y * stride, outb, y * w * 3, w * 3);
      return outb;
    }
  }
  /// <summary>Returns "cx,cy" keys (cell column,row) of every cell whose pixels differ.</summary>
  public static string[] ChangedCells(string a, string b, int cw, int ch) {
    int wa, ha, wb, hb;
    byte[] A = Read(a, out wa, out ha), B = Read(b, out wb, out hb);
    if (wa != wb || ha != hb) return new string[] { "SIZE-MISMATCH " + wa + "x" + ha + " vs " + wb + "x" + hb };
    HashSet<string> set = new HashSet<string>();
    for (int y = 0; y < ha; y++) for (int x = 0; x < wa; x++) {
      int i = (y * wa + x) * 3;
      if (A[i] != B[i] || A[i+1] != B[i+1] || A[i+2] != B[i+2]) set.Add((x / cw) + "," + (y / ch));
    }
    string[] r = new string[set.Count]; set.CopyTo(r); return r;
  }
}
