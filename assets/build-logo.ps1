$ErrorActionPreference = 'Stop'
$svgPath = Join-Path $PSScriptRoot 'DeskTiles.svg'
$pngPath = Join-Path $PSScriptRoot 'DeskTiles-logo.png'
$icoPath = Join-Path $PSScriptRoot 'DeskTiles.ico'
$macAssets = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\DeskTile_mac\assets'))
Write-Host "Logo 輸入 SVG：$svgPath"
Write-Host "Logo 輸出 PNG：$pngPath"
Write-Host "Logo 輸出 ICO：$icoPath"
Write-Host "Mac 圖示素材輸出：$macAssets"
Write-Host '[Logo 1/4] 讀取圓角向量母稿'
Add-Type -AssemblyName System.Drawing
if (-not ('DeskTiles.VectorIcon' -as [type])) {
Add-Type -ReferencedAssemblies @('System.Drawing','System.Xml') -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Xml;
namespace DeskTiles {
 public static class VectorIcon {
  static float Number(string s) { return float.Parse(s, CultureInfo.InvariantCulture); }
  static GraphicsPath Path(string data) {
   var tokens = Regex.Matches(data, @"[MLCZ]|-?\d+(?:\.\d+)?");
   var p = new GraphicsPath(); var last = new PointF();
   for (int i=0;i<tokens.Count;) {
    string op = tokens[i++].Value;
    if (op=="M") { last=new PointF(Number(tokens[i++].Value),Number(tokens[i++].Value)); p.StartFigure(); }
    else if (op=="L") { var next=new PointF(Number(tokens[i++].Value),Number(tokens[i++].Value));p.AddLine(last,next);last=next; }
    else if (op=="C") {
     var a=new PointF(Number(tokens[i++].Value),Number(tokens[i++].Value));
     var b=new PointF(Number(tokens[i++].Value),Number(tokens[i++].Value));
     var c=new PointF(Number(tokens[i++].Value),Number(tokens[i++].Value));
     p.AddBezier(last,a,b,c);last=c;
    } else if(op=="Z") p.CloseFigure(); else throw new InvalidDataException("Unsupported path command");
   }
   return p;
  }
  public static byte[] Render(string source, int size) {
   var xml=new XmlDocument();xml.XmlResolver=null;xml.Load(source);
   int high=size*4;
   using(var large=new Bitmap(high,high,PixelFormat.Format32bppArgb))
   using(var g=Graphics.FromImage(large)) {
    g.Clear(Color.Transparent);g.SmoothingMode=SmoothingMode.AntiAlias;
    g.ScaleTransform(high/1024f,high/1024f);
    foreach(XmlElement node in xml.GetElementsByTagName("path")) {
     string fill=node.GetAttribute("fill");
     if(fill=="") fill=((XmlElement)node.ParentNode).GetAttribute("fill");
     using(var brush=new SolidBrush(ColorTranslator.FromHtml(fill)))
     using(var path=Path(node.GetAttribute("d"))) g.FillPath(brush,path);
    }
    using(var output=new Bitmap(size,size,PixelFormat.Format32bppArgb))
    using(var final=Graphics.FromImage(output))
    using(var memory=new MemoryStream())
    using(var attributes=new ImageAttributes()) {
     final.CompositingMode=CompositingMode.SourceCopy;
     final.InterpolationMode=InterpolationMode.HighQualityBicubic;
     final.PixelOffsetMode=PixelOffsetMode.HighQuality;
     attributes.SetWrapMode(WrapMode.TileFlipXY);
     final.DrawImage(large,new Rectangle(0,0,size,size),0,0,high,high,GraphicsUnit.Pixel,attributes);
     output.Save(memory,ImageFormat.Png);return memory.ToArray();
    }
   }
  }
 }
}
'@
}
Write-Host '[Logo 2/4] 以抗鋸齒輸出 1024px 透明背景 PNG'
[IO.File]::WriteAllBytes($pngPath,[DeskTiles.VectorIcon]::Render($svgPath,1024))
Write-Host '[Logo 3/4] 逐尺寸由向量產生平滑 ICO'
$sizes=@(16,20,24,32,40,48,64,128,256)
$frames=@{}
foreach($size in $sizes){$frames[$size]=[DeskTiles.VectorIcon]::Render($svgPath,$size)}
$stream=[IO.File]::Create($icoPath)
$writer=[IO.BinaryWriter]::new($stream)
try {
 $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
 $offset=6+16*$sizes.Count
 foreach($size in $sizes){
  $dimension=if($size -eq 256){0}else{$size}
  $writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([byte]0);$writer.Write([byte]0)
  $writer.Write([uint16]1);$writer.Write([uint16]32)
  $writer.Write([uint32]$frames[$size].Length);$writer.Write([uint32]$offset)
  $offset+=$frames[$size].Length
 }
 foreach($size in $sizes){$writer.Write([byte[]]$frames[$size])}
} finally {$writer.Dispose();$stream.Dispose()}
Write-Host '[Logo 4/4] 同步 macOS 的共用向量與高解析 PNG'
if(Test-Path -LiteralPath $macAssets){
 Copy-Item -LiteralPath $svgPath -Destination (Join-Path $macAssets 'DeskTiles.svg') -Force
 Copy-Item -LiteralPath $pngPath -Destination (Join-Path $macAssets 'DeskTiles-logo.png') -Force
}
Write-Host '圓角圖示已完成；Mac build.sh 會由 PNG 產生 iconset／ICNS，再重新簽章。'
