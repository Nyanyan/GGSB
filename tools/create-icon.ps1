$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$imageSizes = @(32, 48, 64, 128, 256)
$images = @()
foreach ($size in $imageSizes) {
    $bitmap = [Drawing.Bitmap]::new($size, $size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size / 256.0, $size / 256.0)
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc(8, 8, 128, 128, 180, 90)
    $path.AddArc(120, 8, 128, 128, 270, 90)
    $path.AddArc(120, 120, 128, 128, 0, 90)
    $path.AddArc(8, 120, 128, 128, 90, 90)
    $path.CloseFigure()
    $mintBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#64e8ba'))
    $blackBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#09121a'))
    $whiteBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#f5faf8'))
    $graphics.FillPath($mintBrush, $path)
    $graphics.FillEllipse($blackBrush, 48, 48, 80, 80)
    $graphics.FillEllipse($blackBrush, 128, 128, 80, 80)
    $graphics.FillEllipse($whiteBrush, 128, 48, 80, 80)
    $graphics.FillEllipse($whiteBrush, 48, 128, 80, 80)
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
    $images += ,$stream.ToArray()
    $stream.Dispose(); $graphics.Dispose(); $bitmap.Dispose(); $path.Dispose()
    $mintBrush.Dispose(); $blackBrush.Dispose(); $whiteBrush.Dispose()
}
$iconPath = Join-Path $workspacePath 'assets\icon.ico'
$output = [IO.File]::Create($iconPath)
$writer = [IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
    $offset = 6 + 16 * $images.Count
    for ($i = 0; $i -lt $images.Count; $i++) {
        $dimension = if ($imageSizes[$i] -eq 256) { 0 } else { $imageSizes[$i] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($imageBytes in $images) { $writer.Write([byte[]]$imageBytes) }
} finally { $writer.Dispose(); $output.Dispose() }
Copy-Item -LiteralPath $iconPath -Destination (Join-Path $workspacePath 'App\icon.ico') -Force
