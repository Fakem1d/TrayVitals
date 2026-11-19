# Draws the TrayVitals icon: a CPU, a RAM stick and a graphics card standing on a taskbar,
# in the colors the taskbar labels use. Writes trayvitals.ico (16-256 px) and a 256 px PNG preview.
#   powershell -ExecutionPolicy Bypass -File tools\make-icon.ps1
param([string]$Out = "$PSScriptRoot\..\src\TrayVitals\trayvitals.ico", [string]$Preview = "$PSScriptRoot\icon-preview.png")
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class IconFx {
    // Puts a soft, dark outline behind everything drawn: the shapes' coverage grown by r pixels,
    // softened, at the given opacity. Keeps the icon readable on light and dark wallpapers.
    public static void Outline(Bitmap b, double r, int opacity) {
        int w = b.Width, h = b.Height;
        var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        int[] px = new int[w * h]; Marshal.Copy(d.Scan0, px, 0, px.Length);
        double[] grow = new double[w * h];
        int R = (int)Math.Ceiling(r);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            double m = 0;
            for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) {
                int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                double dist = Math.Sqrt(dx * dx + dy * dy), f = Math.Max(0, Math.Min(1, r + 0.5 - dist));
                double a = ((px[yy * w + xx] >> 24) & 255) / 255.0 * f;
                if (a > m) m = a;
            }
            grow[y * w + x] = m;
        }
        double[] soft = new double[w * h];   // a small blur, so the edge is soft
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            double s = 0; int n = 0;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                s += grow[yy * w + xx]; n++;
            }
            soft[y * w + x] = s / n;
        }
        for (int i = 0; i < px.Length; i++) {
            double fa = ((px[i] >> 24) & 255) / 255.0, ba = soft[i] * opacity / 255.0;
            double oa = fa + ba * (1 - fa);
            if (oa <= 0) { px[i] = 0; continue; }
            int[] bc = { 12, 14, 22 };   // outline color (b, g, r order below)
            int fr = (px[i] >> 16) & 255, fg = (px[i] >> 8) & 255, fb = px[i] & 255;
            int or_ = (int)Math.Round((fr * fa + bc[2] * ba * (1 - fa)) / oa);
            int og = (int)Math.Round((fg * fa + bc[1] * ba * (1 - fa)) / oa);
            int ob = (int)Math.Round((fb * fa + bc[0] * ba * (1 - fa)) / oa);
            px[i] = ((int)Math.Round(oa * 255) << 24) | (or_ << 16) | (og << 8) | ob;
        }
        Marshal.Copy(px, 0, d.Scan0, px.Length);
        b.UnlockBits(d);
    }
}
"@

function RR([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($r * 2, [Math]::Min($w, $h))
    if ($d -le 0.5) { $p.AddRectangle((New-Object Drawing.RectangleF $x, $y, $w, $h)); return $p }
    $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure(); return $p
}
function C([int]$r, [int]$g, [int]$b, [int]$a = 255) { [Drawing.Color]::FromArgb($a, $r, $g, $b) }

function Draw([int]$S) {
    $bmp = New-Object Drawing.Bitmap $S, $S, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.PixelOffsetMode = 'HighQuality'; $g.Clear([Drawing.Color]::Transparent)
    $u = $S / 64.0                          # drawn on a 64-unit grid

    # no background: the shapes stand on their own, with a soft outline added at the end



    $blue = C 80 190 255; $purple = C 185 145 255; $green = C 118 215 90

    # a heartbeat line above the taskbar ("vitals"), where there's room for it
    if ($S -ge 16) {
        $pulse = New-Object Drawing.Pen (C 255 110 130), ([float][Math]::Max(1.2, 2.2*$u))
        $pulse.LineJoin = 'Round'; $pulse.StartCap = 'Round'; $pulse.EndCap = 'Round'
        $pts = @((8,22), (21,22), (25,13), (30,31), (34,9), (38,22), (56,22)) | ForEach-Object { New-Object Drawing.PointF ($_[0]*$u), ($_[1]*$u) }
        $g.DrawLines($pulse, [Drawing.PointF[]]$pts)
    }

    # the taskbar, with the three readings sitting in its tray
    $bar = RR (4*$u) (34*$u) (56*$u) (22*$u) (4.5*$u)
    $g.FillPath((New-Object Drawing.SolidBrush (C 66 74 100)), $bar)
    $g.FillRectangle((New-Object Drawing.SolidBrush (C 120 130 165)), [float](8*$u), [float](34*$u), [float](48*$u), [float]([Math]::Max(1, 0.9*$u)))
    if ($S -ge 32) {   # a "start" button on the left
        $g.FillPath((New-Object Drawing.SolidBrush (C 150 160 195)), (RR (9*$u) (41.5*$u) (7*$u) (7*$u) (1.5*$u)))
    }
    $mid = 45 * $u   # the taskbar's middle line

    # CPU: a chip with pins
    $cs = 9*$u; $cx = 22*$u; $cy = $mid - $cs / 2
    $pen = New-Object Drawing.Pen $blue, ([float][Math]::Max(1, 1.3*$u))
    for ($i = 0; $i -lt 2; $i++) {
        $o = $cx + (3 + $i * 3) * $u
        $g.DrawLine($pen, $o, $cy - 1.8*$u, $o, $cy); $g.DrawLine($pen, $o, $cy + $cs, $o, $cy + $cs + 1.8*$u)
        $o = $cy + (3 + $i * 3) * $u
        $g.DrawLine($pen, $cx - 1.8*$u, $o, $cx, $o); $g.DrawLine($pen, $cx + $cs, $o, $cx + $cs + 1.8*$u, $o)
    }
    $g.FillPath((New-Object Drawing.SolidBrush $blue), (RR $cx $cy $cs $cs (1.8*$u)))
    if ($S -ge 32) { $g.FillPath((New-Object Drawing.SolidBrush (C 20 60 100)), (RR ($cx + 2.6*$u) ($cy + 2.6*$u) ($cs - 5.2*$u) ($cs - 5.2*$u) (0.8*$u))) }

    # RAM: an upright stick with chips and gold contacts
    $rw = 6*$u; $rh = 14*$u; $rx = 37*$u; $ry = $mid - $rh / 2
    $g.FillPath((New-Object Drawing.SolidBrush $purple), (RR $rx $ry $rw $rh (1.2*$u)))
    if ($S -ge 32) {
        for ($i = 0; $i -lt 2; $i++) {
            $g.FillRectangle((New-Object Drawing.SolidBrush (C 70 45 120)), [float]($rx + 1.5*$u), [float]($ry + (2 + $i * 4.2) * $u), [float]($rw - 3*$u), [float](2.8*$u))
        }
    }
    $g.FillRectangle((New-Object Drawing.SolidBrush (C 245 200 90)), [float]($rx + 0.9*$u), [float]($ry + $rh - 2*$u), [float]($rw - 1.8*$u), [float](1.4*$u))

    # GPU: a card with a fan and a bracket
    $gw = 11*$u; $gh = 9*$u; $gx = 47.5*$u; $gy = $mid - $gh / 2
    $g.FillPath((New-Object Drawing.SolidBrush $green), (RR $gx $gy $gw $gh (1.8*$u)))
    $g.FillRectangle((New-Object Drawing.SolidBrush $green), [float]($gx - 1.4*$u), [float]($gy - 0.8*$u), [float](1.4*$u), [float]($gh + 0.8*$u))
    $fr = 3.3*$u; $fx = $gx + $gw / 2 + 0.6*$u; $fy = $mid
    $g.FillEllipse((New-Object Drawing.SolidBrush (C 30 80 30)), $fx - $fr, $fy - $fr, 2*$fr, 2*$fr)
    if ($S -ge 48) {
        $blade = New-Object Drawing.SolidBrush (C 150 230 120)
        for ($a = 0; $a -lt 360; $a += 72) {
            $g.FillPie($blade, $fx - ($fr - 0.7*$u), $fy - ($fr - 0.7*$u), 2*($fr - 0.7*$u), 2*($fr - 0.7*$u), [float]$a, [float]40)
        }
    }
    $g.FillEllipse((New-Object Drawing.SolidBrush (C 30 80 30)), $fx - 1*$u, $fy - 1*$u, 2*$u, 2*$u)

    $g.Dispose()
    [IconFx]::Outline($bmp, [Math]::Max(1.0, 1.6 * $u), 150)
    return $bmp
}

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$pngs = @()
foreach ($s in $sizes) {
    $b = Draw $s
    $ms = New-Object IO.MemoryStream
    $b.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    if ($s -eq 256) { $b.Save($Preview, [Drawing.Imaging.ImageFormat]::Png) }
    $b.Dispose()
    $pngs += ,($ms.ToArray())
}
# .ico with PNG images inside (Windows Vista and later)
$fs = [IO.File]::Create($Out); $w = New-Object IO.BinaryWriter $fs
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $d = $pngs[$i]
    $w.Write([byte]($s % 256)); $w.Write([byte]($s % 256)); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32); $w.Write([UInt32]$d.Length); $w.Write([UInt32]$offset)
    $offset += $d.Length
}
foreach ($d in $pngs) { $w.Write($d) }
$w.Close()
# preview sheet: the small sizes enlarged 4x (pixel for pixel), on light and dark
$sheet = New-Object Drawing.Bitmap 760, 400
$sg = [Drawing.Graphics]::FromImage($sheet); $sg.Clear((C 243 243 243))
$sg.FillRectangle((New-Object Drawing.SolidBrush (C 32 32 32)), 0, 200, 760, 200)
$sg.InterpolationMode = 'NearestNeighbor'; $sg.PixelOffsetMode = 'Half'
$x = 12
foreach ($s in 16, 24, 32, 48) {
    $i = [Array]::IndexOf($sizes, $s)
    $img = [Drawing.Image]::FromStream((New-Object IO.MemoryStream (,$pngs[$i])))
    $sg.DrawImage($img, $x, 100 - $s * 2, $s * 4, $s * 4); $sg.DrawImage($img, $x, 300 - $s * 2, $s * 4, $s * 4)
    $x += $s * 4 + 24
}
$sg.Dispose(); $sheet.Save("$PSScriptRoot\icon-sheet.png")
"wrote $Out ($((Get-Item $Out).Length) bytes) and $Preview"
