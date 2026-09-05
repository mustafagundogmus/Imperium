<#
    Arbitrium Build Script
    ----------------------
    Projeyi tek bir komutla derler.
    Eksik araçları (CMake, MSYS2, Qt vb.) otomatik tespit edip kurar.
    
    Kullanım: PowerShell terminalinde .\build.ps1 yazın.
#>

$ErrorActionPreference = "Stop"
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}

$RootDir = $PSScriptRoot
$MsysDir = "C:\msys64"

# ─── Yardımcı Fonksiyonlar ───

function Write-Step  { param([string]$Msg) Write-Host "`n==> $Msg" -ForegroundColor Cyan }
function Write-Ok    { param([string]$Msg) Write-Host "    [OK] $Msg" -ForegroundColor Green }
function Write-Skip  { param([string]$Msg) Write-Host "    [ATLANDI] $Msg" -ForegroundColor DarkGray }
function Write-Info  { param([string]$Msg) Write-Host "    $Msg" -ForegroundColor Yellow }

function Download-File {
    param(
        [string]$Url,
        [string]$OutFile,
        [string]$Description
    )
    Write-Info "$Description indiriliyor..."
    Write-Host "    URL: $Url" -ForegroundColor DarkGray

    # Invoke-WebRequest, PowerShell 5.1'de varsayılan olarak ilerleme çubuğu gösterir.
    # Ancak büyük dosyalarda yavaşlatmaması için ProgressPreference'ı ayarlıyoruz.
    $ProgressPreference_Backup = $ProgressPreference
    $ProgressPreference = 'Continue'
    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing
    } finally {
        $ProgressPreference = $ProgressPreference_Backup
    }

    if (-not (Test-Path $OutFile)) {
        Write-Error "$Description indirilemedi!"
        exit 1
    }
    Write-Ok "$Description indirildi."
}

# ─── Menü ───

Write-Host ""
Write-Host "  ╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║       Arbitrium Derleme (Build) Aracı           ║" -ForegroundColor Cyan
Write-Host "  ╠══════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "  ║                                                  ║" -ForegroundColor Cyan
Write-Host "  ║  1) Normal Derleme (Visual Studio / Dinamik)     ║" -ForegroundColor Green
Write-Host "  ║     Hızlı derlenir, exe + DLL çıkar.             ║" -ForegroundColor DarkGray
Write-Host "  ║                                                  ║" -ForegroundColor Cyan
Write-Host "  ║  2) Tek Dosya (MinGW / Statik) [ÖNERİLEN]       ║" -ForegroundColor Yellow
Write-Host "  ║     DLL gerektirmeyen tek bir EXE üretir.        ║" -ForegroundColor DarkGray
Write-Host "  ║     İlk seferde ~1-2 GB indirme gerektirir.     ║" -ForegroundColor DarkGray
Write-Host "  ║                                                  ║" -ForegroundColor Cyan
Write-Host "  ╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$choice = Read-Host "  Seçiminiz (1 veya 2)"

# ═══════════════════════════════════════════════════════════════
#  SEÇİM 1: DİNAMİK DERLEME (Visual Studio + Qt DLL)
# ═══════════════════════════════════════════════════════════════
if ($choice -eq '1') {

    $BuildDir = Join-Path $RootDir "build"

    # ── CMake ──
    Write-Step "CMake kontrol ediliyor..."
    if (Get-Command "cmake" -ErrorAction SilentlyContinue) {
        Write-Skip "CMake zaten kurulu."
    } else {
        $installer = "$env:TEMP\cmake-installer.msi"
        Download-File -Url "https://github.com/Kitware/CMake/releases/download/v3.30.2/cmake-3.30.2-windows-x86_64.msi" `
                      -OutFile $installer -Description "CMake"
        
        Write-Info "CMake kuruluyor..."
        $p = Start-Process msiexec.exe -ArgumentList "/i `"$installer`" /qn /norestart ADD_CMAKE_TO_PATH=System" -Wait -PassThru
        if ($p.ExitCode -ne 0) { Write-Error "CMake kurulumu başarısız (Kod: $($p.ExitCode))."; exit 1 }
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        Write-Ok "CMake kuruldu."
    }

    # ── Qt (aqtinstall ile) ──
    $QtVersion = "6.7.2"
    $QtInstallDir = Join-Path $RootDir "Qt"
    $QtPrefixPath = Join-Path $QtInstallDir "$QtVersion\msvc2019_64"

    Write-Step "Qt $QtVersion kontrol ediliyor..."
    if (Test-Path $QtPrefixPath) {
        Write-Skip "Qt $QtVersion zaten mevcut."
    } else {
        # Python kontrolü
        $pythonCmd = "python"
        if (-not (Get-Command $pythonCmd -ErrorAction SilentlyContinue)) {
            $installer = "$env:TEMP\python-installer.exe"
            Download-File -Url "https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe" `
                          -OutFile $installer -Description "Python 3.11"
            
            Write-Info "Python kuruluyor..."
            $p = Start-Process $installer -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1" -Wait -PassThru
            if ($p.ExitCode -ne 0) { Write-Error "Python kurulumu başarısız."; exit 1 }
            $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
            
            if (-not (Get-Command "python" -ErrorAction SilentlyContinue)) {
                $pythonCmd = "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
                if (-not (Test-Path $pythonCmd)) { $pythonCmd = "C:\Program Files\Python311\python.exe" }
            }
            Write-Ok "Python kuruldu."
        }

        Write-Info "aqtinstall yükleniyor..."
        & $pythonCmd -m pip install --quiet aqtinstall

        Write-Info "Qt $QtVersion indiriliyor (Bu işlem birkaç dakika sürebilir)..."
        & $pythonCmd -m aqt install-qt windows desktop $QtVersion win64_msvc2019_64 --outputdir $QtInstallDir

        if (-not (Test-Path $QtPrefixPath)) { Write-Error "Qt kurulumu başarısız."; exit 1 }
        Write-Ok "Qt $QtVersion indirildi."
    }

    # ── CMake Yapılandırma ──
    Write-Step "CMake yapılandırılıyor (Visual Studio 2022)..."
    if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

    $CMakeRoot = $RootDir -replace '\\', '/'
    $CMakeQt   = $QtPrefixPath -replace '\\', '/'

    cmake -S . -B build `
          -G "Visual Studio 17 2022" -A x64 `
          -DCMAKE_BUILD_TYPE=Release `
          -DCMAKE_PREFIX_PATH="$CMakeQt" `
          -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$CMakeRoot" `
          -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$CMakeRoot"

    if ($LASTEXITCODE -ne 0) { Write-Error "CMake yapılandırması başarısız."; exit $LASTEXITCODE }

    # ── Derleme ──
    Write-Step "Arbitrium derleniyor..."
    cmake --build build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { Write-Error "Derleme başarısız."; exit $LASTEXITCODE }

    Write-Host ""
    Write-Host "  ✔ BAŞARILI! Arbitrium.exe ve Qt DLL'leri kök dizine oluşturuldu." -ForegroundColor Green
}

# ═══════════════════════════════════════════════════════════════
#  SEÇİM 2: STATİK DERLEME (Qt kaynağından derlenir = gerçek Tek EXE)
# ═══════════════════════════════════════════════════════════════
#
# Bu seçim artık pacman'in mingw-w64-x86_64-qt6-static paketini KULLANMIYOR.
# O paket Qt'yi -system-freetype, -system-openssl, -system-harfbuzz vb. ile
# yapılandırıyor; yani Qt'nin kendisi statik olsa da freetype/openssl/brotli/
# libb2'yi MSYS2'nin paylaşımlı DLL'lerinden çekmeye devam ediyor — tam da
# "libcrypto-3-x64.dll bulunamadı" gibi hataların kaynağı bu.
#
# Bunun yerine .github/workflows/release.yml'in yaptığı gibi, Qt'yi kendi
# kaynak koduyla ve -DFEATURE_system_*=OFF ile bir kere derleyip
# Qt-static/<sürüm>/ altında önbelleğe alıyoruz — release sayfasındaki
# Arbitrium.exe'nin neden hiçbir DLL istemediğinin sebebi tam olarak bu.
elseif ($choice -eq '2') {

    # ── MSYS2 ──
    Write-Step "MSYS2 kontrol ediliyor..."
    if (Test-Path $MsysDir) {
        Write-Skip "MSYS2 zaten kurulu."
    } else {
        $installer = "$env:TEMP\msys2-installer.exe"
        Download-File -Url "https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-x86_64-latest.exe" `
                      -OutFile $installer -Description "MSYS2"
        
        Write-Info "MSYS2 kuruluyor (bu biraz sürebilir)..."
        $p = Start-Process $installer -ArgumentList "in --confirm-command --accept-messages --root C:\msys64" -Wait -PassThru
        if ($p.ExitCode -ne 0) { Write-Error "MSYS2 kurulumu başarısız (Kod: $($p.ExitCode))."; exit 1 }
        Write-Ok "MSYS2 kuruldu."
    }

    # ── Paket Güncelleme ──
    # Not: freetype/openssl/harfbuzz/brotli vb. burada YOK — Qt bunları kendi
    # kaynağından, kendi içine derleyecek. MSYS2'den yalnızca derleyici,
    # CMake, Ninja ve arşiv/indirme araçları gerekiyor.
    $oldErrAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    & "$MsysDir\usr\bin\bash.exe" -lc "pacman --noconfirm -Syuu" 2>&1 | ForEach-Object { "$_" }
    & "$MsysDir\usr\bin\bash.exe" -lc "pacman --noconfirm -Syuu" 2>&1 | ForEach-Object { "$_" }

    Write-Step "MinGW araç zinciri kuruluyor..."
    $maxRetries = 3
    $pacmanSuccess = $false
    for ($attempt = 1; $attempt -le $maxRetries; $attempt++) {
        if ($attempt -gt 1) {
            Write-Info "Yeniden deneniyor... (Deneme $attempt/$maxRetries)"
        }
        & "$MsysDir\usr\bin\bash.exe" -lc "pacman --noconfirm -S --needed zip unzip tar curl mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-python" 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -eq 0) {
            $pacmanSuccess = $true
            break
        }
        Write-Host "    [!] İndirme hatası oluştu (sunucu yavaş veya bağlantı koptu)." -ForegroundColor Red
    }
    
    $ErrorActionPreference = $oldErrAction
    if (-not $pacmanSuccess) { Write-Error "Paket kurulumu $maxRetries denemeden sonra başarısız oldu. Lütfen internet bağlantınızı kontrol edip tekrar deneyin."; exit 1 }

    # ── Qt'yi kaynağından derle + Arbitrium'u derle ──
    # scripts/build-static.sh her ikisini de yapar ve Qt'yi Qt-static/ altında
    # önbelleğe alır; ilk çalıştırma ~1 saat sürebilir (Qt derleniyor),
    # sonrakiler saniyeler alır.
    $CMakeRoot = ($RootDir -replace '\\', '/')

    Write-Step "Statik Qt derleniyor ve Arbitrium tek EXE olarak build ediliyor..."
    Write-Info "İlk çalıştırmada Qt kaynaktan derlenecek — bu bir saate kadar sürebilir. Sonraki çalıştırmalar önbellekten anında devam eder."
    & "$MsysDir\usr\bin\bash.exe" -lc "export PATH=/mingw64/bin:`$PATH && bash '$CMakeRoot/scripts/build-static.sh'"
    if ($LASTEXITCODE -ne 0) { Write-Error "Derleme başarısız."; exit $LASTEXITCODE }

    Write-Host ""
    Write-Host "  ✔ HARİKA! Tek dosya Arbitrium.exe kök dizine oluşturuldu. DLL gerekmez!" -ForegroundColor Green
}
else {
    Write-Warning "Geçersiz seçim. Lütfen 1 veya 2 girin."
    exit 1
}
