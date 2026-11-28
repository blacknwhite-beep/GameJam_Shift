Add-Type -AssemblyName System.Drawing

$RawDir = "C:\UnrealProjects\GameJam_Shift_2026\Scripts\InventoryIcons\raw"
$OutDir = "C:\UnrealProjects\GameJam_Shift_2026\Scripts\InventoryIcons"

$CSharp = @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class IconProcessor
{
    public static void Process(string srcPath, string dstPath, int targetW, int targetH)
    {
        using (var srcBmp = new Bitmap(srcPath))
        {
            int w = srcBmp.Width;
            int h = srcBmp.Height;
            using (var argb = new Bitmap(w, h, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(argb))
                {
                    g.DrawImage(srcBmp, 0, 0, w, h);
                }

                var rect = new Rectangle(0, 0, w, h);
                var data = argb.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
                int stride = data.Stride;
                byte[] buf = new byte[stride * h];
                Marshal.Copy(data.Scan0, buf, 0, buf.Length);

                const int threshHard = 10;
                const int threshSoft = 22;

                bool[] visited = new bool[w * h];
                bool[] isBg = new bool[w * h];
                var q = new Queue<int>();

                Action<int, int> tryPush = (x, y) =>
                {
                    int idx = y * w + x;
                    if (visited[idx]) return;
                    int off = y * stride + x * 4;
                    int lum = Math.Max(buf[off + 2], Math.Max(buf[off + 1], buf[off]));
                    if (lum <= threshHard)
                    {
                        visited[idx] = true;
                        isBg[idx] = true;
                        q.Enqueue(idx);
                    }
                };

                for (int x = 0; x < w; x++) { tryPush(x, 0); tryPush(x, h - 1); }
                for (int y = 0; y < h; y++) { tryPush(0, y); tryPush(w - 1, y); }

                while (q.Count > 0)
                {
                    int cur = q.Dequeue();
                    int cx = cur % w;
                    int cy = cur / w;
                    if (cx > 0) tryPush(cx - 1, cy);
                    if (cx + 1 < w) tryPush(cx + 1, cy);
                    if (cy > 0) tryPush(cx, cy - 1);
                    if (cy + 1 < h) tryPush(cx, cy + 1);
                }

                int minX = w, minY = h, maxX = -1, maxY = -1;
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int idx = y * w + x;
                        int off = y * stride + x * 4;
                        if (isBg[idx])
                        {
                            buf[off + 3] = 0;
                        }
                        else
                        {
                            int lum = Math.Max(buf[off + 2], Math.Max(buf[off + 1], buf[off]));
                            byte alpha = 255;
                            if (lum < threshSoft)
                            {
                                bool nearBg = false;
                                for (int dy = -2; dy <= 2 && !nearBg; dy++)
                                {
                                    int ny = y + dy;
                                    if (ny < 0 || ny >= h) continue;
                                    for (int dx = -2; dx <= 2; dx++)
                                    {
                                        int nx = x + dx;
                                        if (nx < 0 || nx >= w) continue;
                                        if (isBg[ny * w + nx]) { nearBg = true; break; }
                                    }
                                }
                                if (nearBg)
                                {
                                    double t = (lum - threshHard) / (double)(threshSoft - threshHard);
                                    alpha = (byte)Math.Max(0, Math.Min(255, (int)(t * 255.0)));
                                }
                            }
                            buf[off + 3] = alpha;
                            if (alpha > 20)
                            {
                                if (x < minX) minX = x;
                                if (x > maxX) maxX = x;
                                if (y < minY) minY = y;
                                if (y > maxY) maxY = y;
                            }
                        }
                    }
                }

                Marshal.Copy(buf, 0, data.Scan0, buf.Length);
                argb.UnlockBits(data);

                if (maxX < minX || maxY < minY)
                {
                    minX = 0; minY = 0; maxX = w - 1; maxY = h - 1;
                }

                int cw = maxX - minX + 1;
                int ch = maxY - minY + 1;
                double pad = 0.06;
                double maxW = targetW * (1.0 - 2.0 * pad);
                double maxH = targetH * (1.0 - 2.0 * pad);
                double scale = Math.Min(maxW / cw, maxH / ch);
                int drawW = Math.Max(1, (int)Math.Round(cw * scale));
                int drawH = Math.Max(1, (int)Math.Round(ch * scale));
                int drawX = (targetW - drawW) / 2;
                int drawY = (targetH - drawH) / 2;

                using (var canvas = new Bitmap(targetW, targetH, PixelFormat.Format32bppArgb))
                {
                    using (var g = Graphics.FromImage(canvas))
                    {
                        g.Clear(Color.Transparent);
                        g.CompositingQuality = CompositingQuality.HighQuality;
                        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                        g.SmoothingMode = SmoothingMode.HighQuality;
                        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                        g.DrawImage(argb, new Rectangle(drawX, drawY, drawW, drawH), new Rectangle(minX, minY, cw, ch), GraphicsUnit.Pixel);
                    }
                    canvas.Save(dstPath, ImageFormat.Png);
                }
            }
        }
    }
}
"@

Add-Type -TypeDefinition $CSharp -ReferencedAssemblies System.Drawing

$Icons = @(
    @("icon_ammo_9mm.jpg",      "T_Icon_Ammo_9mm.png",      256, 256),
    @("icon_ammo_12g.jpg",      "T_Icon_Ammo_12G.png",      256, 256),
    @("icon_ammo_556.jpg",      "T_Icon_Ammo_556.png",      256, 256),
    @("icon_medkit.jpg",        "T_Icon_Medkit.png",        256, 512),
    @("icon_landmine.jpg",      "T_Icon_Landmine.png",      256, 256),
    @("icon_farmplot.jpg",      "T_Icon_FarmPlot.png",      384, 256),
    @("icon_weapon_pistol.jpg", "T_Icon_Weapon_Pistol.png", 512, 256),
    @("icon_weapon_smg.jpg",    "T_Icon_Weapon_SMG.png",    512, 256),
    @("icon_weapon_rifle.jpg",  "T_Icon_Weapon_Rifle.png",  384, 128)
)

foreach ($entry in $Icons) {
    $src = Join-Path $RawDir $entry[0]
    $dst = Join-Path $OutDir $entry[1]
    [IconProcessor]::Process($src, $dst, $entry[2], $entry[3])
    Write-Output "OK $($entry[1]) -> $($entry[2])x$($entry[3])"
}
