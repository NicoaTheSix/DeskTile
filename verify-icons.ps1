param([string]$ExecutablePath=(Join-Path $PSScriptRoot 'build\DeskTiles-update.exe'))
$ErrorActionPreference='Stop'
$icoPath=Join-Path $PSScriptRoot 'assets\DeskTiles.ico'
$outputPath=Join-Path $PSScriptRoot 'build\icon-verification.txt'
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
Write-Host "輸入圖示：$icoPath"
Write-Host "輸入執行檔：$ExecutablePath"
Write-Host "輸出結果：$outputPath"
Write-Host '[圖示驗證 1/3] 讀取多尺寸圖示及完整 EXE 資源'
Add-Type -AssemblyName System.Drawing
if(-not ('DeskTiles.IconCheck' -as [type])){
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Runtime.InteropServices;
namespace DeskTiles {
 public static class IconCheck {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryExW(string file,IntPtr h,uint flags);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr FindResourceW(IntPtr h,IntPtr name,IntPtr type);
  [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr LoadResource(IntPtr h,IntPtr resource);
  [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
  [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr h,IntPtr resource);
  [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr h);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadImageW(IntPtr h,IntPtr name,uint type,int x,int y,uint flags);
  [DllImport("user32.dll")] static extern bool DestroyIcon(IntPtr icon);
  static void Require(bool ok,string message){if(!ok)throw new InvalidDataException(message);}
  static byte[] Resource(IntPtr h,int id,int type){
   var r=FindResourceW(h,new IntPtr(id),new IntPtr(type));
   Require(r!=IntPtr.Zero,"Missing icon resource");
   var bytes=new byte[SizeofResource(h,r)];
   var p=LockResource(LoadResource(h,r));Require(p!=IntPtr.Zero,"Cannot load icon bytes");
   Marshal.Copy(p,bytes,0,bytes.Length);return bytes;
  }
  public static string Run(string exe,string ico){
   var source=File.ReadAllBytes(ico);int[] sizes={16,20,24,32,40,48,64,128,256};
   Require(BitConverter.ToUInt16(source,4)==sizes.Length,"Unexpected ICO frame count");
   var h=LoadLibraryExW(exe,IntPtr.Zero,2);Require(h!=IntPtr.Zero,"Cannot open executable resources");
   try{
    var group=Resource(h,101,14);Require(BitConverter.ToUInt16(group,4)==sizes.Length,"EXE icon group differs");
    for(int i=0;i<sizes.Length;i++){
     int entry=6+16*i,n=(int)BitConverter.ToUInt32(source,entry+8),offset=(int)BitConverter.ToUInt32(source,entry+12);
     int actual=source[entry]==0?256:source[entry];Require(actual==sizes[i],"Incorrect ICO dimensions");
     int id=BitConverter.ToUInt16(group,6+14*i+12);var embedded=Resource(h,id,3);
     Require(embedded.Length==n,"Embedded frame length mismatch");
     for(int j=0;j<n;j++)Require(embedded[j]==source[offset+j],"EXE still has another icon");
     using(var memory=new MemoryStream(source,offset,n))
     using(var bitmap=new Bitmap(memory)){
      Require(bitmap.Width==sizes[i]&&bitmap.Height==sizes[i],"PNG frame dimensions mismatch");
      Require(bitmap.GetPixel(0,0).A==0,"Outer corners are not transparent");
      int partial=0,black=0,white=0;
      for(int y=0;y<bitmap.Height;y++)for(int x=0;x<bitmap.Width;x++){
       var c=bitmap.GetPixel(x,y);
       if(c.A>0&&c.A<255)partial++;
       if(c.A==255&&c.R<40)black++;
       if(c.A==255&&c.R>240)white++;
      }
      Require(partial>0&&black>0&&white>0,"Missing anti-aliasing or black/white artwork");
     }
     var icon=LoadImageW(h,new IntPtr(101),1,sizes[i],sizes[i],0);
     Require(icon!=IntPtr.Zero,"Windows cannot load this icon size");DestroyIcon(icon);
    }
   } finally {FreeLibrary(h);}
   return "PASS: all 9 ICO sizes decode, preserve transparent corners and smoothing; packaged EXE resources match every frame and Windows loads every size.";
  }
 }
}
'@ -ReferencedAssemblies System.Drawing
}
Write-Host '[圖示驗證 2/3] 逐尺寸解碼、透明角落、抗鋸齒及 Windows LoadImage'
$result=[DeskTiles.IconCheck]::Run($ExecutablePath,$icoPath)
Write-Host '[圖示驗證 3/3] 保存結果'
$result|Set-Content -LiteralPath $outputPath -Encoding utf8
Write-Host $result
