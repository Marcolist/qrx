param([string]$Destination="")
$ErrorActionPreference='Stop'
$Repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(-not $Destination){$Destination=Join-Path $Repo 'GUIWALLET\src-tauri\resources\upscaler'}
$Cache=if($env:QRX_AI_DOWNLOAD_CACHE){$env:QRX_AI_DOWNLOAD_CACHE}else{Join-Path $Repo '.cache\qrx-ai'}
New-Item -ItemType Directory -Force -Path $Cache|Out-Null
$Lock=Join-Path $PSScriptRoot 'upscaler-ai-archives.sha256'; $Tag='v0.2.5.0'
$ModelAsset='realesrgan-ncnn-vulkan-20220424-ubuntu.zip'; $RuntimeAsset='realesrgan-ncnn-vulkan-20220424-windows.zip'
function LockHash([string]$Asset,[string]$EnvName){
  $v=''; if(Test-Path $Lock){$line=Get-Content $Lock|Where-Object{$_ -match "^[0-9a-fA-F]{64}\s+$([regex]::Escape($Asset))$"}|Select-Object -First 1; if($line){$v=($line -split '\s+')[0]}}
  if(-not $v){$v=[Environment]::GetEnvironmentVariable($EnvName)}
  if($v -notmatch '^[0-9a-fA-F]{64}$'){throw "No reviewed SHA-256 lock for $Asset. Add it to scripts/upscaler-ai-archives.sha256 or set $EnvName. QRX refuses trust-on-first-use."}; return $v.ToLowerInvariant()
}
function FetchLocked([string]$Asset,[string]$EnvName){
  $expected=LockHash $Asset $EnvName; $out=Join-Path $Cache $Asset
  if(Test-Path $out){$got=(Get-FileHash -Algorithm SHA256 $out).Hash.ToLowerInvariant(); if($got -ne $expected){Remove-Item $out -Force}}
  if(-not(Test-Path $out)){if($env:QRX_AI_OFFLINE -eq '1'){throw "QRX_AI_OFFLINE=1 and verified cache unavailable: $Asset"}; $url="https://github.com/xinntao/Real-ESRGAN/releases/download/$Tag/$Asset"; Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile "$out.part"; Move-Item -Force "$out.part" $out}
  $got=(Get-FileHash -Algorithm SHA256 $out).Hash.ToLowerInvariant(); if($got -ne $expected){throw "AI asset SHA256 mismatch: $Asset expected=$expected actual=$got"}; return $out
}
function Find-One([string]$Root,[string]$Name){$p=Get-ChildItem $Root -Recurse -File -Filter $Name|Select-Object -First 1; if(-not $p){throw "Required AI file missing: $Name"}; return $p.FullName}
$modelZip=FetchLocked $ModelAsset 'QRX_REAL_ESRGAN_MODEL_ARCHIVE_SHA256'; $runtimeZip=FetchLocked $RuntimeAsset 'QRX_REAL_ESRGAN_WINDOWS_X64_ARCHIVE_SHA256'
$tmp=Join-Path ([IO.Path]::GetTempPath()) ('qrx-ai-'+[guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Force -Path $tmp|Out-Null
try{
  $mu=Join-Path $tmp 'models'; $ru=Join-Path $tmp 'runtime'; Expand-Archive $modelZip $mu -Force; Expand-Archive $runtimeZip $ru -Force
  $runtime=Find-One $ru 'realesrgan-ncnn-vulkan.exe'; $bytes=[IO.File]::ReadAllBytes($runtime); if($bytes[0]-ne 0x4d -or $bytes[1]-ne 0x5a){throw 'AI runtime is not PE/MZ'}; $off=[BitConverter]::ToInt32($bytes,0x3c); if([BitConverter]::ToUInt32($bytes,$off)-ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$off+4)-ne 0x8664){throw 'AI runtime is not Windows x86-64'}
  if(Test-Path $Destination){Remove-Item $Destination -Recurse -Force}; New-Item -ItemType Directory -Force -Path (Join-Path $Destination 'runtime'),(Join-Path $Destination 'models'),(Join-Path $Destination 'licenses')|Out-Null
  Copy-Item $runtime (Join-Path $Destination 'runtime\realesrgan-ncnn-vulkan.exe')
  foreach($n in 'realesr-animevideov3-x2.param','realesr-animevideov3-x2.bin','realesrgan-x4plus.param','realesrgan-x4plus.bin'){Copy-Item (Find-One $mu $n) (Join-Path $Destination "models\$n")}
  $rhash=(Get-FileHash -Algorithm SHA256 (Join-Path $Destination 'runtime\realesrgan-ncnn-vulkan.exe')).Hash.ToLowerInvariant(); Set-Content -Encoding ascii (Join-Path $Destination 'runtime\runtime.sha256') $rhash
  $msha=(Get-FileHash -Algorithm SHA256 $modelZip).Hash.ToLowerInvariant();
  foreach($s in 2,4){if($s-eq 2){$id='realesr-animevideov3';$pa='realesr-animevideov3-x2.param';$we='realesr-animevideov3-x2.bin';$mf='realesrgan-x2plus.qrxmodel'}else{$id='realesrgan-x4plus';$pa='realesrgan-x4plus.param';$we='realesrgan-x4plus.bin';$mf='realesrgan-x4plus.qrxmodel'};$ph=(Get-FileHash -Algorithm SHA256 (Join-Path $Destination "models\$pa")).Hash.ToLowerInvariant();$wh=(Get-FileHash -Algorithm SHA256 (Join-Path $Destination "models\$we")).Hash.ToLowerInvariant();@("format=qrx-upscaler-model-v2","id=$id","scale=$s","param=$pa","weights=$we","param_sha256=$ph","weights_sha256=$wh","license_id=Real-ESRGAN-BSD-3-Clause","provenance_source_url=https://github.com/xinntao/Real-ESRGAN","provenance_release=v0.2.5.0@685d429","provenance_archive_sha256=$msha")|Set-Content -Encoding ascii (Join-Path $Destination "models\$mf")}
  $lic=Get-ChildItem $ru,$mu -Recurse -File -Filter LICENSE|Select-Object -First 1;if($lic){Copy-Item $lic.FullName (Join-Path $Destination 'licenses\Real-ESRGAN-ncnn-vulkan-LICENSE.txt')}
  $runtimeArchive=(Get-FileHash -Algorithm SHA256 $runtimeZip).Hash.ToLowerInvariant(); @('QRX verified AI bundle v3','target=windows-x64','upstream_project=https://github.com/xinntao/Real-ESRGAN','runtime_project=https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan','model_release=v0.2.5.0@685d429',"model_asset=$ModelAsset","model_archive_sha256=$msha",'runtime_origin=official-portable',"runtime_asset=$RuntimeAsset","runtime_archive_sha256=$runtimeArchive",'runtime_source_commit=n/a',"runtime_sha256=$rhash",'supply_chain_policy=qrx-0.0.9.65-sha256-release-lock','model_x2=realesr-animevideov3-x2','model_x4=realesrgan-x4plus','backend=ncnn-vulkan','acceleration_windows=Vulkan (Intel/AMD/NVIDIA driver)','license_models=Real-ESRGAN BSD-3-Clause')|Set-Content -Encoding ascii (Join-Path $Destination 'PROVENANCE.txt')
  Write-Host "[AI bundle] staged verified windows-x64 bundle at $Destination"
} finally {Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue}
