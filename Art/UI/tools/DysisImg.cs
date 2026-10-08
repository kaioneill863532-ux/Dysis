// 界面素材小工具（PowerShell 里 Add-Type -Path 本文件 -ReferencedAssemblies System.Drawing 后调用）
//   LogoToAlpha：黑底发光图 → 透明底 PNG；Bounds：不透明范围；Match：在示意图里找素材摆放的位置。
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class DysisImg {
    public static byte[] Load(string path, out int w, out int h) {
        using (var src = new Bitmap(path)) {
            w = src.Width; h = src.Height;
            using (var b = new Bitmap(w, h, PixelFormat.Format32bppArgb)) {
                using (var g = Graphics.FromImage(b)) { g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy; g.DrawImage(src, 0, 0, w, h); }
                var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                var buf = new byte[w * h * 4];
                for (int y = 0; y < h; y++) Marshal.Copy(IntPtr.Add(d.Scan0, y * d.Stride), buf, y * w * 4, w * 4);
                b.UnlockBits(d); return buf;
            }
        }
    }
    public static void Save(byte[] buf, int w, int h, string path) {
        using (var b = new Bitmap(w, h, PixelFormat.Format32bppArgb)) {
            var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            for (int y = 0; y < h; y++) Marshal.Copy(buf, y * w * 4, IntPtr.Add(d.Scan0, y * d.Stride), w * 4);
            b.UnlockBits(d); b.Save(path, ImageFormat.Png);
        }
    }
    public static string LogoToAlpha(string src, string dst, int floor) {
        int w, h; var p = Load(src, out w, out h);
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int i = 0; i < w * h; i++) {
            int b = p[i * 4], g = p[i * 4 + 1], r = p[i * 4 + 2];
            int m = Math.Max(r, Math.Max(g, b));
            double a = m <= floor ? 0 : Math.Min(1.0, (m - floor) / (double)(255 - floor) * 1.15);
            if (a <= 0) { p[i * 4] = p[i * 4 + 1] = p[i * 4 + 2] = p[i * 4 + 3] = 0; continue; }
            double k = 255.0 / m;
            p[i * 4] = (byte)Math.Min(255, b * k); p[i * 4 + 1] = (byte)Math.Min(255, g * k); p[i * 4 + 2] = (byte)Math.Min(255, r * k); p[i * 4 + 3] = (byte)(a * 255);
            if (a > 0.3) { int x = i % w, y = i / w; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        }
        Save(p, w, h, dst);
        return w + "x" + h + " 内容范围 x " + x0 + "-" + x1 + ", y " + y0 + "-" + y1;
    }
    public static string Bounds(string path, int thr) {
        int w, h; var p = Load(path, out w, out h);
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (p[(y * w + x) * 4 + 3] > thr) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        return w + "x" + h + " 不透明范围 x " + x0 + "-" + x1 + ", y " + y0 + "-" + y1;
    }
    static byte[] mockBuf; static int mockW, mockH; static string mockPath;
    // 在示意图里找素材的位置：在 (gx±range, gy±range) 里按 step 找残差最小的偏移（只在素材 alpha≥minA 的像素上比颜色，每 sub 个像素取一个）。
    public static string Match(string mock, string elem, int gx, int gy, int range, int step, int sub, int minA) {
        if (mockPath != mock) { mockBuf = Load(mock, out mockW, out mockH); mockPath = mock; }
        int ew, eh; var e = Load(elem, out ew, out eh); var m = mockBuf; int mw = mockW, mh = mockH;
        double best = 1e18; int bx = 0, by = 0;
        for (int oy = gy - range; oy <= gy + range; oy += step) for (int ox = gx - range; ox <= gx + range; ox += step) {
            double sum = 0; int n = 0;
            for (int y = 0; y < eh; y += sub) { int my = oy + y; if (my < 0 || my >= mh) continue;
                for (int x = 0; x < ew; x += sub) { int mx = ox + x; if (mx < 0 || mx >= mw) continue;
                    int ei = (y * ew + x) * 4; if (e[ei + 3] < minA) continue;
                    int mi = (my * mw + mx) * 4;
                    sum += Math.Abs(m[mi] - e[ei]) + Math.Abs(m[mi + 1] - e[ei + 1]) + Math.Abs(m[mi + 2] - e[ei + 2]); n += 3; } }
            if (n < 600) continue; double r = sum / n;
            if (r < best) { best = r; bx = ox; by = oy; }
        }
        return bx + "," + by + "," + best.ToString("0.0");
    }
    // 线稿类素材（半透明的亮线）：找“素材有线的地方示意图更亮、没线的地方不亮”差值最大的偏移。
    public static string MatchLine(string mock, string elem, int gx, int gy, int range, int step, int sub, int minA) {
        if (mockPath != mock) { mockBuf = Load(mock, out mockW, out mockH); mockPath = mock; }
        int ew, eh; var e = Load(elem, out ew, out eh); var m = mockBuf; int mw = mockW, mh = mockH;
        double best = -1e18; int bx = 0, by = 0;
        for (int oy = gy - range; oy <= gy + range; oy += step) for (int ox = gx - range; ox <= gx + range; ox += step) {
            double on = 0, off = 0; int n1 = 0, n0 = 0;
            for (int y = 0; y < eh; y += sub) { int my = oy + y; if (my < 0 || my >= mh) continue;
                for (int x = 0; x < ew; x += sub) { int mx = ox + x; if (mx < 0 || mx >= mw) continue;
                    int a = e[(y * ew + x) * 4 + 3]; int mi = (my * mw + mx) * 4;
                    double lum = 0.11 * m[mi] + 0.59 * m[mi + 1] + 0.30 * m[mi + 2];
                    if (a >= minA) { on += lum; n1++; } else if (a < 8) { off += lum; n0++; } } }
            if (n1 < 200 || n0 < 200) continue; double r = on / n1 - off / n0;
            if (r > best) { best = r; bx = ox; by = oy; }
        }
        return bx + "," + by + "," + best.ToString("0.0");
    }
}
