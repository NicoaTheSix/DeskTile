$ErrorActionPreference = 'Stop'
$gridPath = Join-Path $PSScriptRoot 'logo-grid.txt'
$icoPath = Join-Path $PSScriptRoot 'DeskTiles.ico'
$svgPath = Join-Path $PSScriptRoot 'DeskTiles.svg'
$pngPath = Join-Path $PSScriptRoot 'DeskTiles-logo.png'
Write-Host "Logo 輸入絕對路徑：$gridPath"
Write-Host "Logo ICO 輸出絕對路徑：$icoPath"
Write-Host "Logo SVG 輸出絕對路徑：$svgPath"
Write-Host "Logo PNG 輸出絕對路徑：$pngPath"
Write-Host '[Logo 1/4] 讀取並驗證 32×32 黑白像素格'
$grid = [IO.File]::ReadAllLines($gridPath)
if ($grid.Count -ne 32) { throw 'Logo 必須有 32 列。' }
foreach ($row in $grid) { if ($row -notmatch '^[.#]{32}$') { throw 'Logo 每列須含 32 個 . 或 #。' } }
Write-Host '[Logo 2/4] 輸出像素 SVG 與 512px PNG'
$svg = [Text.StringBuilder]::new()
[void]$svg.AppendLine('<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 32 32" shape-rendering="crispEdges" role="img" aria-label="DeskTiles black and white pixel logo">')
[void]$svg.AppendLine('<rect width="32" height="32" fill="white"/><g fill="black">')
Add-Type -AssemblyName System.Drawing
$png = [Drawing.Bitmap]::new(512,512)
$graphics = [Drawing.Graphics]::FromImage($png)
try {
    $graphics.Clear([Drawing.Color]::White)
    for ($y=0; $y -lt 32; $y++) { for ($x=0; $x -lt 32; $x++) { if ($grid[$y][$x] -eq '#') {
        [void]$svg.AppendLine(('<rect x="{0}" y="{1}" width="1" height="1"/>' -f $x,$y))
        $graphics.FillRectangle([Drawing.Brushes]::Black,$x*16,$y*16,16,16)
    } } }
    $png.Save($pngPath,[Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose(); $png.Dispose() }
[void]$svg.AppendLine('</g></svg>')
[IO.File]::WriteAllText($svgPath,$svg.ToString(),[Text.UTF8Encoding]::new($false))
Write-Host '[Logo 3/4] 建立 16／32／48／64／128／256px ICO，使用最近鄰像素取樣'
$sizes = @(16,32,48,64,128,256)
$frames = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $maskRow = [int]([Math]::Ceiling($size/32.0)*4)
    $memory = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($memory)
    try {
        $writer.Write([uint32]40); $writer.Write([int]$size); $writer.Write([int]($size*2))
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]0)
        $writer.Write([uint32]($size*$size*4+$maskRow*$size))
        1..4 | ForEach-Object { $writer.Write([uint32]0) }
        for ($y=$size-1; $y -ge 0; $y--) { for ($x=0; $x -lt $size; $x++) {
            $sourceX = [int][Math]::Floor($x*32.0/$size)
            $sourceY = [int][Math]::Floor($y*32.0/$size)
            if ($grid[$sourceY][$sourceX] -eq '#') { $writer.Write([uint32]0xFF000000L) }
            else { $writer.Write([uint32]0xFFFFFFFFL) }
        } }
        $writer.Write([byte[]]::new($maskRow*$size))
        $writer.Flush(); $frames.Add($memory.ToArray())
    } finally { $writer.Dispose(); $memory.Dispose() }
}
$file = [IO.File]::Create($icoPath)
$ico = [IO.BinaryWriter]::new($file)
try {
    $ico.Write([uint16]0); $ico.Write([uint16]1); $ico.Write([uint16]$sizes.Count)
    $offset = 6+16*$sizes.Count
    for ($i=0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $ico.Write([byte]$dimension); $ico.Write([byte]$dimension); $ico.Write([byte]0); $ico.Write([byte]0)
        $ico.Write([uint16]1); $ico.Write([uint16]32)
        $ico.Write([uint32]$frames[$i].Length); $ico.Write([uint32]$offset)
        $offset += $frames[$i].Length
    }
    foreach ($frame in $frames) { $ico.Write($frame) }
} finally { $ico.Dispose(); $file.Dispose() }
Write-Host '[Logo 4/4] 黑白像素 Logo 已完成'
