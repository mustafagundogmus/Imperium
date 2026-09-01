# Changelog

Bu dosya sürümler arasındaki dikkate değer değişiklikleri listeler.
Biçim [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/) temellidir ve
proje [Semantic Versioning](https://semver.org/lang/tr/) kullanır.

## [0.12.1] — 2026-09-02

Bir düzeltme sürümü. Tıklanacak yeni bir şey yok; etiketinin söylediğini artık gerçekten
yapan epey satır var. Kaynağın tamamının ve `catalog.json`'daki her satırın, Windows'un
gerçekten okuduğu değerlere karşı okunmasından çıktı.

### Düzeltildi

#### Makineyi okumak kayıt defterine anahtar yazıyordu

`sysinfo.h` "hiçbir şey yazılmaz" diyordu. Doğru değildi. Bir anahtarın toplu okuması
`QSettings` üzerinden gidiyordu ve `QSettings` bir kayıt defteri yolunu `RegCreateKeyEx` ile
açar: olmayan bir anahtara bakmak o anahtarı **yaratır**. Genel Bakış, yokluğu bir makinenin
olağan hâli olan on küsur anahtarı okur — BIOS ile açılan makinede `SecureBoot\State`,
yönetilmeyen makinede `DataCollection` ve `System` ilke anahtarları, Hyper-V kurulu değilken
`vmms` hizmeti, HKCR altında bir ProgId — ve her açılış HKLM'de boş anahtarlar bırakıyordu.
"UEFI mi?" sorusunu `SecureBoot\State` var mı diye soran bir betik, Arbitrium'u bir kez
çalıştırmış BIOS makinesinde evet demeye başlardı.

`Registry::openKey` artık önce hiçbir şey yaratmayan `RegOpenKeyEx` ile bakıyor ve olmayan
anahtarı boş, dosyasız bir depoyla yanıtlıyor. Her değer yok okunuyor; makinede hiçbir şey
değişmiyor.

#### Üç program hâlâ çıplak adıyla başlatılıyordu

0.12.0, "yönetici belirteci içinde, miras alınan `PATH` üzerinden çözülen çıplak program adı"
deliğini PowerShell için kapatmış ve bunu söylemişti. Üç başlatma atlanmıştı: Uygulamalar
sayfasının taraması (`powershell.exe`), sahiplik alma kabuk fiilleri (`takeown.exe`,
`icacls.exe`) ve Sistem Koruması düğmesi (`SystemPropertiesProtection.exe`). Üçü de artık
uygulamanın geri kalanının kullandığı `winpaths.h` üzerinden, System32'nin mutlak yolundan
çalışıyor.

#### Hassas dokunmatik yüzeyler hiçbir şeyi kaydırmıyordu

Hassas bir dokunmatik yüzey ve serbest dönen bir tekerlek, bir çentiğin 120'sinin epey
altında delta gönderir. Yumuşak kaydırma yolu `delta / 120 * step` hesaplıyordu; bu, her biri
için sıfırdır. Olay kabul ediliyor, sayfa yerinde kalıyordu. Yumuşak kaydırma varsayılan
olarak açık olduğundan bu cihazlarda hiçbir sayfa kaymıyordu — ayar kapatılana kadar, ki o
zaman olay Qt'nin kendi işleyicisine gidiyordu. Kalan artık bir olaydan diğerine taşınıyor;
bir çentik değerindeki küçük deltalar bir çentik ediyor.

#### İletişim kutusu perdesi gölge payını da kaplıyordu

İletişim kutusu perdesini *pencereye* göre boyutlandırıyordu ve pencerenin dikdörtgeni,
gölgenin çizildiği saydam payı da içerir. Her onay kutusu kartın dört yanına, masaüstünün
üstüne taşan sert kenarlı, neredeyse mat bir dikdörtgen çiziyordu. Artık karta göre
boyutlanıyor. Ayrıntılar'ı açıp kapamak da sürüklediğiniz kutuyu köşeye geri sıçratmıyor.

#### Genel Bakış'ta düpedüz yanlış olan değerler

- **Açılış süresi** olay 100'ün 0. özelliğini okuyordu; o, sabit 2 olan `BootTsVersion`.
  Olayı olan her makinede "0,0 sn" gösteriyordu. 5. özellik `BootTime`.
- **Boşta süresi** 32 bitlik `LASTINPUTINFO::dwTime`'ı 64 bitlik `GetTickCount64()`'ten
  çıkarıyordu. 49,7 günlük çalışma süresinden sonra satır yetmiş bin dakika civarı okuyordu.
- **Bekleyen etki alanı katılımı** Netlogon anahtarının bir değeri olarak aranıyordu.
  `JoinDomain` ve `AvoidSpnSet` alt anahtardır; neden hiçbir zaman tetiklenemiyordu.
- **Wi-Fi sinyali** "Signal" etiketiyle eşleştiriliyordu; `netsh` o etiketi görüntü dilinde
  basar. Yüzde artık biçiminden seçiliyor.
- **IPv4 ağ geçidi** yalnızca ilk ağ geçidi girişinden okunuyordu; çift yığın bir bağlantıda
  o çoğu zaman IPv6 yönlendiricisidir ve satır "—" gösteriyordu. Gösterilen **IPv6 adresi**
  genellikle link-local `fe80::` olanıydı; artık yönlendirilebilir adres tercih ediliyor.
- **Yüzde işareti** üç yerde — DPI ölçeği, pil, commit — Türkçe konumda (sayının önünde)
  sabitti; dokuz dil "%150" okuyordu. Pil sağlığı satırının zaten yaptığı gibi çeviri
  tablosundan geçiyor.

#### Daha küçük olanlar

- Uygula perdesi fareyi engelliyor, klavyeyi engellemiyordu: Tab odağı perdenin altındaki bir
  anahtara taşıyordu, Ctrl+K yazma sürerken arama kutusuna ulaşıyordu. İkisi de tutuluyor.
- Perdenin "şu an yazılan" satırı her dilde ham Türkçe adı gösteriyordu; her şey gibi
  `displayName()` üzerinden geçiyor.
- TrustedInstaller başlatıcısı, `UpdateProcThreadAttribute` başarısız olduğunda öznitelik
  listesini sızdırıyordu.
- `.reg` dışa aktarma: BINARY baytlar kanonik iki basamaklı yazımdan geçmiyordu ve bir
  MULTI_SZ, listeyi kapatan ikinci null'dan yoksundu.
- Uygulamalar sayfasının Kaldır düğmesi, yazı tipi veya metin boyutu değişince yerinde
  kalıyordu.

### Katalog

#### Yanlış şeyi yazan satırlar

Her biri satırın söylediğine değil, Windows'un okuduğuna karşı denetlendi.

- **gt-lockscreen-timeout** — açık ve kapalı tersti. Bir güç ayarı için `Attributes` 1
  gizler, 2 gösterir; ayarı görünür yapmayı vadeden satır onu gizliyordu.
- **ch-launchto** — "Giriş" 3 yazıyordu; o, İndirilenler. Giriş 2, Bu bilgisayar 1,
  İndirilenler 3 — satır artık tam olarak bu üçünü sunuyor.
- **aud-ducking** — değer haritası bir kaymıştı: 0 susturur, 1 %80, 2 %50, 3 hiçbir şey
  yapmaz. Varsayılan "hiçbir şey yapma" değil %80.
- **cln-do-cache-age** — `DOMaxCacheAge` **saniye** cinsindendir. 1–30 gün yazan kaydırıcı
  1–30 saniye yazıyor ve Teslim İyileştirme önbelleğini neredeyse anında boşaltıyordu. Artık
  gün seçenekleri; "Varsayılan" yapılandırılmamış demek.
- **cln-do-cache-size** — bir bant genişliği sınırı olan `DOPercentageMaxForegroundBandwidth`
  değerini yazıyordu; 50'ye çekmek güncelleme indirmelerini yarı hıza düşürüyordu. Önbellek
  boyutu ilkesi `DOMaxCacheSize`, varsayılanı %20.
- **pwr-update-wake** — `AUPowerManagement`, yazıldığı yerin bir alt seviyesinde,
  `WindowsUpdate\AU` altında yaşar.
- **sec-smartscreen-apps** — `SmartScreenEnabled` (Warn / Block / Off) `Policies\System`'den
  değil Explorer anahtarından okunur.
- **wu-WPFTweaksRevertStartMenu** — `ControlSet001` her zaman canlı denetim kümesi değildir;
  `CurrentControlSet` öyledir.
- **wu-WPFToggleHiddenFiles** — Gezgin "gösterme" için 0 değil 2 yazar.
- **cln-shutdown-app-timeout** — Windows varsayılanı 5000 değil 20000.
- **cln-evt-autobackup** — `AutoBackupLogFiles` yalnızca aynı anahtarda
  `Retention = 0xFFFFFFFF` ile etkili olur; iki günlük de artık onu alıyor.
- **ctx-runas-user** — `runas.exe /netonly /user:%%USERNAME%%` hiçbir zaman çalışamazdı:
  bir REG_SZ fiil ortam değişkenlerini genişletmez ve `/netonly` programı zaten geçerli
  kullanıcı olarak çalıştırır. Windows'ta bu fiil zaten var, Shift+sağ tıkın arkasında
  saklı; satır artık onun `Extended` işaretini kaldırıyor.
- **wu-WPFTweaksHiber** — `HibernateEnabled`'ı okunmadığı yere, `Session Manager\Power`
  altına yazıyordu; değer `Control\Power` altında yaşar ve `pwr-hibernate-file` onu zaten
  oraya yazıyor. Satır öteki yarısını koruyor ve adı ona göre: Hazırda beklet menü girdisini
  gizle.
- **sec-lm-hash, sys-04-2688, priv-01-4010, priv-02-4013, net-02-3286,
  wu-WPFToggleNewOutlook** — "kapalı" konumu *yapılandırılmış* bir ilke değeri yazıyordu
  (`sec-lm-hash`'te LM karması saklamayı açan bir değer). Kapalı artık değeri kaldırıyor.
- **ch-taskbar-size** — `TaskbarSi` 22H2'de çalışmaz oldu; `maxBuild` 22000.
  **net-02-19242** ve **adv-02-29115** 24H2 özellikleri; `minBuild` 26100.
- **sec-smb1-client** — açıklama artık uyarıyor: `mrxsmb10` İş İstasyonu hizmetinin
  bağımlılıkları arasında listeleniyorsa, devre dışı bırakmak o hizmeti bir sonraki açılışta
  durdurur ve bütün ağ paylaşımları onunla gider. SMBv1 istemcisini Windows Özellikleri
  üzerinden kaldırmak daha güvenli.

#### Adı başka bir şey söyleyen satırlar

- **perf-memory-compression** → **Sayfa birleştirme**. `DisablePagingCombining` sayfa
  birleştirmedir, bellek sıkıştırma değil.
- **aud-exclusive-mode** → **Korumalı ses yolu (DRM)**. `DisableProtectedAudioDG` korumalı
  ses grafiğidir; özel kip uç nokta başına bir özelliktir.
- **pwr-sleep-button** → **Gözetimsiz uyku zaman aşımı seçeneği**. `7bc4a2f9-…` GUID'i
  "Sistem gözetimsiz uyku zaman aşımı"dır; Başlat menüsündeki Uyku girdisiyle ilgisi yok.
- **pwr-adaptive-brightness** — satır ayarı görünür yapar; açıklama uyarlanır parlaklığı
  kapattığını söylüyordu.

### Kaldırıldı

- **net-autotuning** — `Tcpip\Parameters` altında `TcpAutotuning` diye bir parametre yok;
  alma penceresi seviyesi NSI deposunda tutulan bir `netsh` ayarıdır.
- **wu-WPFToggleStandbyFix** — HKCU altındaki bir güç ilkesi hiç okunmaz; `net-02-3286` aynı
  GUID'i HKLM altında tutuyor.
- **pwr-pcie-aspm**, **pwr-lid-close** — `Attributes` yalnızca bir güç ayarının gösterilip
  gösterilmediğini denetler ve ikisi de varsayılan olarak gösterilir. Satırlar hiçbir şey
  yapamıyordu.

Katalog 411 satırdan 407'ye iniyor. Kaldırılmış bir id'yi adlandıran ön ayarlar, her zaman
olduğu gibi atlanıp sayılıyor.

### Eylemler

- **act-disk-cleanup** — `cleanmgr` bir GUI programı; betik onu beklemiyordu, DISM altında
  çalışıyordu ve sürücü sabit `C:` idi.
- **act-svchost-threshold** — `Win32_PhysicalMemory` bildirmeyen bir sanal makinede eşik 0
  yazılıyordu; `Win32_ComputerSystem`'e düşüyor.
- **act-remove-edge** — `${Env:ProgramFiles(x86)}`, PowerShell'in kastettiği yazımla.

## [0.12.0] — 2026-08-31

### Eklendi

#### Uygulamanın kendi ileti kutusu

`QMessageBox` uygulamadaki tek yabancı yüzeydi: Windows'un kendi pencere çerçevesini, kendi
başlık çubuğunu, kendi grisini ve kendi düğme ölçülerini getiriyordu. Çerçevesiz, temalı ve
on dilli bir uygulamada bu, araya giren başka bir program gibi okunuyordu — ve temayı takip
edemediği için açık paletlerde sistem ne diyorsa o kalıyordu.

Yeni `Dialog` tasarımın kendi sözlüğüyle çiziliyor: kart `ApplyOverlay`'in kartı (Tile zemin,
TileBorder saç çizgisi, ControlRadius köşeler, Window renginde perde), başlık
`Font::blockTitle()`, gövde `Font::tweakDesc()` ve **kırpılmıyor, sarılıyor** — bir iletişim
kutusu metnin tam okunması gereken tek yerdir. Düğmeler `PillButton`: kabul için Accent,
vazgeçmek için Ghost.

Ayrıntı paneli QMessageBox'ınkinden iyi: eylem betiği tek aralıklı bir iç panelde duruyor ve
**seçilip kopyalanabiliyor**. Panel her zaman tam satır yüksekliğinde — yarım satır, "devamı
var" değil, bir çizim hatası gibi okunur. Satır yüksekliği font metriğinden değil, o satırı
çizecek olan `QPlainTextDocumentLayout::blockBoundingRect()`'ten alınıyor; font metriği bir
piksel eksikti ve dokuz satırda bu tam bir satır ediyordu.

Escape ve perdeye tıklamak vazgeçiyor, Return kabul ediyor, kart sürüklenebiliyor. Sekiz
çağrı yerinin hepsi geçti; kodda `QMessageBox` kalmadı.

#### Dört açık tema daha

Sekiz temanın yalnızca ikisi açıktı. Şimdi altı: **Sis** (kısılmış), **Yüksek karşıtlık**
(AAA, 7:1), **Çayır** (yeşil) ve **Leylak** (mor). Tema anahtarı on iki kartı 6×2 çiziyor —
dört eklendi çünkü on iki, `flexColumns`'un inebileceği her sütun sayısına (12, 6, 4, 3, 2, 1)
tam bölünüyor; on bir, altı şeklin dördünde tırtıklı bir son satır bırakırdı.

#### Yirmi üç yeni tweak

Recall'ı cihazdan kaldırma, uygulama kullanım verisi toplayıcıları (24H2'nin beşi birden),
cihazlar arası pano, OneSettings, çökme raporundaki bellek içeriği, NCSI sınama isteği, gizli
ağ paylaşımları, kilit ekranı pencere öğeleri, Copilot tuşunun hedefi, sudo kipi, düzenli
kayıt defteri yedeği ve diğerleri. Katalog 390'dan 411 satıra çıktı.

Bedeli olan satırın açıklaması **bedelle başlıyor**: Recall satırı "diskte duran anlık
görüntüler silinir ve geri gelmez" diye, kayıt defteri yedeği "SAM ve SECURITY kovanlarının
şifrelenmemiş birer kopyası diskte durur" diye açılıyor. Adlar da mekanizmayı değil etkiyi
anlatıyor — "OneSettings indirmeleri" değil, **"Windows'un kendini uzaktan ayarlaması"**.

### Düzeltildi

#### Açılışta otuz saniyelik donma

Bazı makinelerde uygulama anında açılıyor, bazılarında otuz saniye donuyordu. Sebep
`QStandardPaths::findExecutable("powershell")`: bu fonksiyon PATH'teki **her dizini** tek tek
yokluyor, ve PATH'inde erişilemeyen bir eşlenmiş ağ sürücüsü olan makinede her yoklama SMB
zaman aşımına kadar bloke oluyor. İki yerde, ikisi de UI thread'inde, pencere çizilmeden önce.
PATH'i tamamen yerel olan makinede maliyeti sıfır — bu yüzden geliştiricinin makinesinde hiç
görünmedi.

`ActionEngine` de `powershell.exe`'yi çıplak adla veriyordu; QProcess onu da PATH'ten çözüyor,
üstelik yönetici yetkisiyle çalışan bir süreçte, ardından ona bir betik vermek üzere. Üçü de
projenin daha önce `mmc.exe`, `control.exe`, `tbs.dll` ve `netapi32.dll` için kapattığı açığın
kaçmış örnekleriydi.

Yeni `src/winpaths.{h,cpp}`: System32 `GetSystemDirectoryW`'den alınıyor ve PowerShell sabit,
mutlak yoluyla çözülüyor. Bulunamazsa çıplak ada düşmüyor, temiz hata veriyor.
`godmodepage.cpp`'deki birebir aynı ikinci çözücü silindi.

#### Donmuş açılış ekranı

`splashscreen.h` "splash çizerken ana pencere arkada kuruluyor" diyordu; doğru değildi.
`show()` ile `finish()` arasında olay döngüsüne hiç sıra gelmiyordu, yani kart bir kez çizilip
constructor bitene kadar donuyordu. Yavaş bir makinede kullanıcının gördüğü şey, donmuş bir
uygulamaydı.

`Splash::report()` eklendi. Başlangıç işi beş aşamayı adlandırıyor — **Katalog okunuyor**,
**Hizmetler taranıyor**, **Sistem bilgileri okunuyor**, **Arayüz hazırlanıyor**, **Ayarların
durumu okunuyor** — ve her çağrı kartı yeniden çizip döngüye bir tur veriyor. Animasyon artık
gerçekten akıyor, ve kartın alt satırı hangi aşamada olunduğunu yazıyor: takılan bir kullanıcı
nerede takıldığını ekran görüntüsüyle söyleyebiliyor, hata ayıklama sürümü gerekmeden.

#### Dört bozuk tweak

**`perf-game-mmcss` dört değerden birine indi.** Microsoft'un kendi MMCSS belgesi `GPU Priority`
ve `SFIO Priority` için "bu değer kullanılmıyor" diyor, `Priority` ise Yüksek sınıfta her
hâlükârda 2 sayılıyor — üstelik satır `GPU Priority`'yi `off='8' on='8'` yazıyordu, yani kendi
verisine göre bile bir şey yapmıyordu. Geriye gerçekten okunan tek değer kaldı.

**`perf-vrr` ve `perf-autohdr` kaldırıldı.** İkisi de tek bir REG_SZ içindeki *belirteçlerdi*:
`DirectXUserGlobalSettings`, `AutoHDREnable=1;VRROptimizeEnable=1;...` biçiminde. `perf-autohdr`
bir belirteç yükünü değer *adı* sanıp `AutoHDREnable` adlı bir değer yazıyordu — Windows böyle
bir değeri hiç okumaz. `perf-vrr` ise doğru değere yazıyor ama dizenin **tamamını** eziyordu:
çok ekran kartlı dizüstülerde kullanıcının tercih ettiği GPU seçimi (`HighPerfAdapter`) dâhil
her belirteç siliniyordu. Katalogun `reg` şekli oku-değiştir-yaz yapamadığı için satır doğru
hâle getirilemez; `sec-powershell-v2` emsaliyle aynı karar verildi. İkisi de Ayarlar > Sistem >
Ekran > Grafikler altında duruyor.

**`perf-hpet-timer`'ın adı ve açıklaması değişti.** Değer doğruydu; anlatımı değildi. Bu bir
platform zamanlayıcısı veya HPET ayarı değil: 2004'te gelen süreç başına yalıtımı kaldırıyor,
yani `timeBeginPeriod` çağıran tek bir uygulama bütün makinenin zamanlayıcısını hızlandırıyor.

#### Kontrast: on iki paletin hepsi ölçüldü

Açık ve Sepya paletleri dosyanın kendi 4,5:1 iddiasını tutmuyordu — Açık'ta `textFainter`
2,18, Sepya'da 2,20. Üç koyu palet de aynı belirteçte ve aynı zeminde kaçırıyordu: Koyu'nun
belgelenmiş 4,52'si yalnız pencere değeri, sayının asıl çizildiği kart üzerinde 4,34.

Her metin belirteci artık **gerçekten üzerine çizildiği zeminde** ölçülüyor, çizim koduna
bakılarak; `iconStroke`, `textMono` ve `textMuted` için dosyanın kendi tablosu yanlıştı.
Yalnız metin renkleri oynadı — yüzeyler, kenarlıklar, anahtarlar ve `scrollThumb` tek piksel
değişmedi. Rakamı yazılı olmayan beş şemaya da 55 satır ek açıklama kondu, böylece on iki
paletin tamamı denetlenebilir.

`accentInk()` de artık her şemanın kendi tabanına çözüyor. Tabanı 4,5 sabitiydi; Yüksek
karşıtlık teması AAA vaat edip seçili kategori etiketini 4,55-5,92 arasında bir mürekkeple
çiziyordu.

### Değiştirildi

#### Doğrulayıcıda gerçek bir delik kapandı

46 çeviri anahtarı eksikken `tools/check-data.py` yemyeşil geçiyordu: check 8 `tweak.` önekini
"çalışma zamanı öneki" diye muaf tutuyor, check 6 ise yalnız *var olan* anahtarların eksiksiz
olduğunu denetliyor — sahip olmadığı bir anahtar hakkında hiçbir şey söylemiyor.

Yeni **check 12** her katalog satırının `name` ve `desc` anahtarını, ve içinde kelime geçen her
seçenek etiketinin `opt.` anahtarını şart koşuyor. Kelime ölçütü üç harf: "50 MB" ve "22H2" her
dilde aynı okunur ve anahtar istemez, "5 saniye" ile "1 dakika" ister.

Buna bağlı olarak `TweakOption::displayLabel()` rakamla başlayan etiketleri de artık arıyor.
Erken dönüş bir aralık kaydırıcısının "512 MB"ı için yazılmıştı; ama aynı kural "5 saniye"yi de
yakalıyor ve onu on dilde Türkçe bırakıyordu.

#### Çeviriler

23 yeni satır, onarılan 2 satır, 15 seçenek etiketi ve 4 tema adı — on dilde. Onarılan iki
satırın **İngilizce açıklaması hâlâ eski yanlışı taşıyordu**: `perf-game-mmcss` için "GPU ve CPU
önceliğini birlikte yükseltir", yani onarımın kaldırdığı iddianın ta kendisi.

---

## [0.11.0] — 2026-08-31

### Eklendi

#### Uygulama kendini güncelliyor

Açılışta yeni bir sürüm varsa soruyor; kabul edilirse indiriyor, doğruluyor, yerine koyuyor ve
yeni sürümle açılıyor. Taşınabilirlik bozulmuyor: kurulum yok, hizmet yok, zamanlanmış görev yok,
klasörde kullanıcının zaten sahip olduğu tek dosyadan başka bir şey kalmıyor.

`updater.h`'nin başındaki söz — *"hiçbir şey indirmez ve kurmaz, kullanıcıya sürüm sayfasını
verir"* — artık doğru olmadığı için yeniden yazıldı. Yerine geçen kural şu: **hiçbir şey bir
insan onaylamadan olmuyor.** Açılış kontrolü yalnızca bakar; indiren ve değiştiren tek şey
teklif penceresindeki düğmedir.

İndirilen dosyanın SHA-256 özeti, sürümün yayımladığı `.sha256` ile karşılaştırılıyor ve
uyuşmazsa hiçbir şey çalıştırılmıyor. Kullanıcıya gösterilen metin bunun neyi kanıtlayıp neyi
kanıtlamadığını açıkça söylüyor: dosyanın eksiksiz indiğini ve o sürümün yayımladığı dosya
olduğunu kanıtlar, kimin derlediğini **kanıtlamaz** — release'i değiştirebilen aynı hamlede
özet dosyasını da değiştirir. Kaynağı kanıtlayan şey GitHub'ın yayımladığı build attestation'ı,
ve bir Sigstore paketini süreç içinde doğrulamak buranın işi değil; yorum bunu söyleyip
README'nin zaten belgelediği `gh attestation verify` komutuna işaret ediyor.

Windows çalışan bir exe'nin üzerine yazdırmaz ama yeniden adlandırmaya izin verir: yeni sürüm
mevcut exe'nin yanına iniyor, doğrulanıyor, çalışan dosya kenara alınıyor, yenisi yerine
konuyor, başlatılıyor ve eski süreç çıkıyor. Kalıntı bir sonraki açılışta siliniyor — ilk
denemede başarısız olması beklenen bir şey, çünkü değiştirilen süreç hâlâ çıkıyor olabiliyor;
birkaç saniye sonraki ikinci deneme tutuyor.

Yazılamayan bir klasör **indirmeden önce** tespit ediliyor, sonunda başarısız olmak yerine.
Her başarısızlık hangi aşamada takıldığını söylüyor ve kullanıcıyı çalışan bir uygulamayla
bırakıyor — tek istisnası, yeni dosya yerine konamayıp eski dosyanın da geri alınamadığı dal,
ve o durumda uygulamanın hangi adla nerede olduğu ve elle nasıl geri alınacağı yazılıyor.
`updater.h` bu istisnayı gizlemek yerine adıyla anıyor.

`checkUpdatesOnLaunch` artık **varsayılan olarak açık**. Kapalıyken özellik, düğmeyi aramaya
gitmeyen herkes için ölü koddu; ve yönetici yetkisiyle çalışıp registry'ye yazan bir programın
üç sürüm geride kalmış bir kopyası nötr bir durum değil. Kontrolün ne olduğu bunu savunulabilir
kılıyor: `api.github.com`'a günde en fazla bir kez, yalnızca bir User-Agent taşıyan anonim bir
GET — kimlik yok, makine bilgisi yok, hiçbir telemetri yok. Ayarlar'daki anahtar tek tıkla
kapatıyor ve açıklaması ne yaptığını aynen yazıyor.

#### Genel Bakış kartlarında ikonlar

Yirmi dokuz kart artık başlığının yanında bir ikon taşıyor. Set **lucide**; glyph'ler yazım
anında `api.iconify.design`'dan çekilip `src/icons.cpp`'ye gömüldü — çalışma anında hiçbir şey
indirilmiyor, çünkü bu taşınabilir bir exe ve çevrimdışı çalışmak zorunda.

Mevcut `Icons::fragment()` üzerinden gidiyorlar, yani tema, vurgu rengi, metin boyutu ve
kesirli dpr için zaten çözülmüş yolu kullanıyorlar; ikinci bir mekanizma eklenmedi. Çizgi
kalınlığı lucide'ın kendi 2'si yerine 24 birimde 1.75: 14px kutuda 1.02 mantıksal piksele denk
geliyor, yani uygulamanın elle çizilmiş sekiz glyph'iyle ve `Css::hairline` ile aynı incelikte.

On dilde en uzun kart başlığı 1240px'lik asgari pencerede ölçüldü — en dar durum Rusça
"Программное обеспечение", 274px alanda 165px — hiçbiri ikon yüzünden kısalmıyor. lucide ISC
lisans metni `resources/licenses/` altına eklendi ve release zip'i artık dokuz lisans dosyası
taşıyor.

#### Hakkında sayfası

Dört çıplak satırdan ibaretti. Artık Genel Bakış ızgarasının **kendi kartlarından** ikisi —
yeni bir bileşen değil, aynı `InfoSection` — ve içindeki her rakam çalışan build'den okunuyor:
sürüm, Qt sürümü, lisans, katalog ve eylem sayısı; yanında tema, vurgu rengi, dil, yazı tipi ve
boyut sayıları. Yani sayfa bu binary'nin taşımadığı bir dili ya da yazı tipini reklam edemiyor.

Dördü de tablosundan okunuyor; tema sayısı okunamıyordu çünkü bir C++ enum'unun eleman sayısı
yok, o yüzden `Theme::AppearanceCount` eklendi ve yanına dokuzuncu palet nereye eklenirse
eklensin patlayan bir `static_assert` kondu. Dört bağlantı satırı aynen duruyor; bağış satırı
hâlâ sonda ve hâlâ sade.

### Değiştirildi

#### Ayarlar sayfası yeniden düzenlendi

Şikâyet "sıkışık ve karışık"tı; ölçünce sorun aralık değil şuymuş: **dört galeri, bir liste
satırının sağ kontrolü olarak giydirilmişti.** `SettingRow::Trailing` 30×16'lık bir anahtar ya
da bir hap düğme için tasarlanmış; içine 372×158'lik tema ızgarası, on dillik çip kümesi, altı
yazı tipi örneği ve sekiz renk noktası konuyordu.

Sonuçları ölçüldü: aynı 1px'le birleşmiş listede 158, 58, 27, 22 ve 40 piksellik satır
yükseklikleri — satırlar çizgi ve zemin çizmediği için bir ayarı diğerinden ayıran tek sinyal
boşluktu ve 158px'in yanında o sinyal yok oluyordu. En geniş kontrol de sütunu en uzun
açıklamadan çalıyordu: dil açıklaması Fransızca'da varsayılan boyutta **1136px**, en büyük
boyutta 1439px, ve eline geçen 970px'in **164'üydü**. Ekranda en sıkışık görünen şey buydu.

Yeni bir yerleşim eklendi — `SettingRow::Below`: açıklama tam sütunda, kontrol altında kendi
satırında. Dört galeri de tek bir kurala bağlandı: eşit hücreler, tek bir 10px boşluk ve satır
dengeleyen bir sarma. Sonuç 1240×760'ta her metin boyutunda **on dil tek satırda** (en büyük
boyutta 970'in 930'u) ve sekiz tema **tek sırada**.

Gruplama da tarihe göre değil aramaya göre yeniden yapıldı, kuralı "bir bölüm, bir şekil":
**Görünüm** artık yalnızca bir kümeden seçim yaptıran beş ayar (Dil, Tema, Vurgu, Yazı tipi,
Yazı boyutu), yeni **Arayüz** bölümü ise üç açık/kapalı anahtar. Dil başa alındı, çünkü
okuyamadığı bir dile düşmüş birinin onu **okumadan** bulabilmesi gerekiyor ve on dilin kendi
adları her koşulda okunur.

Yol üstünde iki hata: `TypefacePicker`'ın `typefaceChanged` bağlantısı hiç yokmuş ve örneklerini
`Theme::font()`'u atlayarak kuruyormuş — sayfadaki tek kontroldü ki "Çok büyük"te 1.0 boyutunda
kalıyordu. Ve `SettingRow::positionControl()` yazı tipi değişiminde çalışmıyordu, yani genişleyen
bir hap düğme yerinde kalıyordu.

Tema açıklaması da eskimişti: "dört palet" diyordu, sekiz var. On dilde düzeltildi.

#### Kaydırma çubuğu 8'den 12 piksele

Tutamak iki yanından 2px kenarlıkla içeri alındığı için 8, kavraması zor 4 piksellik bir şerit
bırakıyordu. `sidebar.cpp`'nin iki geçişli ölçümü bu sayıyı satır genişliğinden düşüyor;
aritmetiği yeniden kontrol edildi.

---

## [0.10.0] — 2026-08-31

### Eklendi

#### MIT lisansı

Depo bugüne kadar lisanssızdı: README'deki rozet "Open Source" diyordu ama hiçbir yere
bağlanmıyordu ve kökte bir `LICENSE` dosyası yoktu — yani kaynak herkese açık olsa da kimsenin
onu kullanma, değiştirme veya dağıtma izni yoktu. Artık kökte MIT var; GitHub da lisansı
oradan tanıyor.

- **`LICENSE`** — MIT metni, `Copyright (c) 2026 ShadesOfDeath`.
- **README'deki rozet** artık MIT diyor ve dosyanın kendisine bağlanıyor. Sonuna bir **License**
  bölümü eklendi; bölüm, binary'nin içinde taşınan iki üçüncü taraf lisansını da anıyor:
  Qt 6.11 (LGPL v3 — statik linklendiği için, onu yeniden linklemeye yetecek her şeyin release
  workflow'unda durduğu notuyla) ve IBM Plex (SIL OFL 1.1).
- **Release zip'i artık `LICENSE`'ı da taşıyor.** MIT, bildirimin her kopyayla birlikte gitmesini
  istiyor; zip'te bugüne kadar yalnızca exe ile README vardı.
- **`app.rc`'ye `LegalCopyright` eklendi**, böylece exe zip'inden çıkıp tek başına dolaştığında
  da bildirim Özellikler → Ayrıntılar altında görünüyor.

#### Her commit artık derleniyor: `.github/workflows/ci.yml`

Bugüne kadar bir commit'i ilk derleyen makine, release'i kesen makineydi. Bunun faturası
0.9.9 ile 0.9.10 arasında duruyor: arka arkaya on bir tane sadece-build-düzeltmesi commit'i,
her biri ancak tag atıp tam bir release koşusunu sonuna kadar izleyerek sınanabilmiş.

Yeni workflow `main`'e push'ta ve pull request'te çalışıyor, tag'lerde çalışmıyor — `v*`
release.yml'in işi ve her sürümü iki kez derlemenin anlamı yok. İki iş, iki ayrı hüküm:

- **`data`** (ubuntu) — `tools/check-data.py`. Derleyici yok, Qt yok, birkaç saniye. Build
  bozukken de rapor verebilmesi için ayrı bir job: veri dosyaları binary'ye gömülü olduğundan
  bozuk bir katalog derleme hatası değil, kusursuz derlenen bir build'de boş açılan sayfadır.
- **`build`** (windows) — MSYS2'den önceden derlenmiş paylaşımlı Qt ile yapılandır, derle,
  exe'nin çıktığını doğrula.

Statik Qt bilerek yeniden derlenmiyor: cold cache'te bir saate yakın sürüyor ve o kadar yavaş
bir kontrol, birinin kapattığı kontroldür. CI "bu commit derleniyor mu" sorusunu cevaplıyor;
statik linke özgü her şey release'de kalıyor.

Qt'nin kendi binary'leri yerine MSYS2 kullanılmasının iki nedeni var, ikisi de varsayılmadan
denenerek bulundu: **(1)** aqtinstall Windows'ta Qt 6.11'i hiç kuramıyor — Qt 6.11'de online
depoyu araç zinciri başına bir dizine böldü (`qt6_6111/qt6_6111_mingw/`), aqtinstall hâlâ eski
yolu kuruyor ve `Failed to locate XML data for Qt version '6.11.1'` ile duruyor; düzeltme
master'da, hiçbir tag'de değil. **(2)** Qt elde olsa bile runner'da ona uyan derleyici yok:
`windows-latest`'in tek PATH'teki MinGW'si UCRT, Qt'nin `win64_mingw` paketleri msvcrt — iki
C runtime link uyumlu değil. MSYS2 ikisini birden çözüyor ve release.yml zaten onu kullanıyor.

`clang-format`, `clang-tidy`, `-Werror` ve bir test koşucusu bilerek yok — dördü de ilk gün,
tetikleyen commit'le ilgisi olmayan sebeplerle kırmızı yanardı, ve ilk günden kırmızı bir
kontrol iki hafta içinde silinen kontroldür. Dosyada, bir okuyucunun onları arayacağı yerde
gerekçesiyle yazıyor.

#### `tools/check-data.py` — veri dosyaları için derleyicisiz doğrulayıcı

Katalog, action listesi ve çeviriler elle düzenlenen JSON ve bugüne kadar hiçbir şey onları
denetlemiyordu. Script yalnızca standart kütüphane kullanıyor, 70 ms sürüyor, ve şemayı
tahmin etmek yerine kaynaktan okuyor: diller `i18n.cpp`'deki `Languages`'tan, hive
yazımları `hiveFromString()`'in gövdesinden, riskli servisler `services.cpp`'deki
`RiskyServices[]`'ten geliyor — onbirinci bir dil eklenince burada düzeltilecek bir şey yok.

On kontrol. En önemlisi ikincisi: bir option'ın `data` dizisi `reg` dizisinden kısa olduğunda
`catalog.cpp:386` aradaki farkı boş string'le dolduruyor, `registry.cpp` de onu
`data.toUInt()` ile okuyor — yani gerçek bir anahtara sessizce DWORD 0 yazılıyor. Aynı sessiz
sıfır, bir switch'in `off`/`on` alanına hex ya da boş değer yazıldığında da oluşuyor; kontrol
her ikisini de kapsıyor (90 listelenmiş pozisyon + 714 switch pozisyonu). Diğerleri: tweak id
çakışması, bilinmeyen hive ve tip, eksik zorunlu alanlar, grid dışına düşen range varsayılanı,
her dil için eksik ya da boş i18n değeri, `RiskyServices[]`'in `svc.risk.<Key>` karşılıkları,
`Locale::tr`/`Locale::content`'e verilen her literal'in çözülmesi, ve actions.json'un alanları
ile `ARB|` sonuç token'ları.

Bugün on kontrolün onu da geçiyor: 391 tweak, 0 çift id, 557 registry girdisi, 24 range,
1575 × 10 çeviri, 28 risk notu, 18 action. Amaç zaten bu — script temiz bir durumu bozulmaktan
korumak için var, bozuk bir durumu düzeltmek için değil.

#### `tools/screenshots.ps1` — README'nin görsellerini tek komutla almak

Depoda tek bir ekran görüntüsü yok, oysa uygulama kendini fotoğraflamayı zaten biliyor:
`--screenshot <yol>` pencerenin PNG'sini yazıp çıkıyor, `--theme`, `--typeface`,
`--category` ve `--search` de içinde ne olacağına karar veriyor. Eksik olan tek şey,
tutmaya değer kareler listesiydi — script sadece o.

Sekiz kare: Gizlilik (koyu), Görünüm (açık), Dosya Gezgini (okyanus), Debloat, Eylemler,
Günlük, TrustedInstaller (gece) ve Ayarlar (sepya). Sürüm başına yenilemek bir komut.

İki şeyi bilerek yapmıyor. **CI'da çalışmıyor**: uygulama `requireAdministrator` ve Genel
Bakış sayfası canlı makineyi okuyor, yani runner'da alınan bir görüntü aktive edilmemiş bir
Azure VM'in bilgilerini reklam ederdi. **Genel Bakış sayfasını istenmedikçe fotoğraflamıyor**:
etkinleştirme durumu, BIOS/SMBIOS dizeleri, disk seri numaraları, BitLocker durumu ve makine
adı orada. `-IncludeOverview` var, ama seçenek olsun diye — iyi fikir olduğu için değil.

Yükseltilmiş bir kabuktan çalıştırılması öneriliyor: exe kim başlatırsa başlatsın yönetici
hakkı istiyor, yani yükseltilmemiş bir kabukta sekiz kare sekiz onay demek.

#### Üçüncü taraf lisansları artık indirmeyle birlikte geliyor

`resources/licenses/` altına LGPL-3.0 ve GPL-3.0 metinleri eklendi — qt/qtbase'in `v6.11.1`
etiketinden birebir alındı. Bunlar ve `resources/fonts/` altındaki altı OFL dosyası artık zip
içinde bir `licenses/` klasöründe. Bugüne kadar hepsi depoyla seyahat ediyordu, indirmeyle
hiçbiri.

Glob'un yapamadığı şeyi — bir şeyin *eksik* olduğunu fark etmek — `check` job'ındaki yeni bir
adım yapıyor: sekiz dosya sayılıyor, eksikse adlarıyla birlikte otuz saniyede duruyor, bir
saatlik Qt derlemesinden sonra değil.

#### README: "Windows will warn you about this file"

Getting started listesi "indir" → "çalıştır" diyordu; arada gerçekte olan iki diyalog hiçbir
yerde yazmıyordu. Yeni bölüm ikisini de adıyla anlatıyor, binary'nin neden imzasız olduğunu
özür dilemeden söylüyor (sertifika birkaç yüz dolar/yıl, Haziran 2023'ten beri donanım token'ı
zorunlu, ve bireyin alabildiği türü *Bilinmeyen yayıncı*'yı **hukuki adla** değiştirir — üstelik
SmartScreen itibar tabanlı olduğu için ilk gün o diyaloğu zaten geçmez), ve güven yerine
doğrulama sunuyor: `Get-FileHash` ile sums karşılaştırması ve `gh attestation verify`. İki komut
da bu makinede gerçek `gh` sürümüne karşı sınandı.

License bölümü de düzeltildi: gömülü olan **altı** yazı tipi ailesi (IBM Plex, Monda, Open Sans,
Oxygen, Red Hat Text, Saira), sadece IBM Plex değil.

#### God Mode — Windows'un kendi ayarlarını aramak

Yeni bir sayfa: sekiz grupta 38 kısayol — Windows ayar sayfaları, denetim masası apletleri ve
yönetim konsolları — üstünde bir arama kutusu, her satırın altında neyi açtığı aynen yazılı.
Windows'un kendi "Tüm görevler" (God Mode) klasörünü açan satır da burada.

Sayfa makinede hiçbir şey değiştirmiyor; yalnızca Windows'un kendi penceresini açıyor. Liste
`resources/data/settings-links.json`'da ve katalogla aynı disiplinle **yalnızca derlenmiş qrc'den**
okunuyor: exe'nin yanına bırakılmış bir dosya, yükseltilmiş bir sürecin ne açacağını yönlendiremez.

25 `ms-settings:` hedefinin tamamı, Windows'un kendi `SystemSettings.dll`'inin bildirdiği 268
URI'lik listeye karşı **ölçülerek** doğrulandı. Konsol ve applet satırları `mmc.exe`/`control.exe`
üzerinden, System32'den **mutlak yolla** başlatılıyor — çıplak adla değil; bu, tbs.dll ve
netapi32.dll için zaten düzeltilmiş olan arama sırası sınıfının aynısı. `explorer.exe` ve
`regedit.exe` System32'de değil `C:\Windows`'ta olduğu için çözücü ikinci bir dizine daha bakıyor.
Makinede bulunmayan bir hedefin (ör. Home sürümünde `gpedit.msc`) satırı sebebiyle birlikte soluk
çiziliyor, tıklanınca başarısız olmuyor.

`tools/check-data.py`'ye 11. kontrol eklendi: her id'nin `godmode.<id>` karşılığı olmalı, id'ler
benzersiz olmalı, ve bir hedef kendi yolunu taşımamalı — `"..\\Temp\\x.exe"` gibi bir giriş
System32 garantisini sessizce anlamsızlaştırırdı.

Kenar çubuğunda yeni bir **Araçlar** grubu var; alttaki sabit şeride dokunulmadı, çünkü oranın
geometrisi elle bağlı bir zincir. `debloat`'un katalog dışı bir id olarak nasıl taşındıysa aynen.

#### Gelişmiş yeniden başlatma

Eylemler sayfasına altı yeni giriş: normal yeniden başlatma, **Gelişmiş başlatma** (WinRE kurtarma
menüsü), doğrudan **UEFI/BIOS** kurulumuna, **Güvenli mod**, **ağ ile güvenli mod**, ve
**güvenli mod başlatmayı kaldır**.

Yeni bir sayfa değil, veri: `ActionPage` zaten çalışacak betiği onay diyaloğunda tam metin
gösteriyor ve yalnızca onaylanınca çalıştırıyor. "Ne çalışacağını oku, sonra karar ver" sözleşmesi
tam da bu komutların ihtiyacı olan şey.

Güvenli mod **kalıcıdır** ve notu bunu büyük harfle söylüyor: `bcdedit` önyükleme yapılandırmasına
yazdığı için makine, ayar silinene kadar her açılışta güvenli moda girer. Çıkış yolu aynı sayfada,
adıyla veriliyor. Ve `$LASTEXITCODE` kontrolü `bcdedit` ile `shutdown` **arasında** duruyor:
reddedilen bir `bcdedit`'in ardından yine de yeniden başlatmak, bu değişikliğin verebileceği en kötü
sonuçtu — makine iner, aynı şekilde geri gelir, kullanıcı güvenli modda olduğunu sanır.

UEFI komutu eski BIOS'lu makinelerde `shutdown` hata döndürerek durur; hatayı gizlemek yerine
gösteriyor.

#### Ön ayarlar artık görünümü ve ayarları da taşıyor, ve yalnızca değiştirdiklerini yazıyor

Ön ayar dosyası sürüm 3. İki yeni isteğe bağlı blok: `<appearance>` (tema, vurgu rengi, yazı tipi,
metin boyutu, yoğunluk, arayüz dili) ve `<settings>` (yumuşak kaydırma, kenar parıltısı, açılışta
güncelleme denetimi, uygulamadan önce onay). v1 ve v2 dosyaları aynen yükleniyor; blokların ikisi
de isteğe bağlı, yani hiçbirini taşımayan bir dosya sıradan bir dosya.

Export artık katalogdaki **her** id'yi yazmıyor. Eskiden bu makinede 706 satırdı — 390 katalog
tweak'i artı canlı makineden sentezlenen 307 servis ve 9 başlangıç öğesi — yani kullanıcının
"benim kırk tweak'im" sandığı dosya yedi yüz satırdı ve A makinesinin tüm servis yapılandırmasını
B makinesine taşıyordu. Artık yalnızca Windows'un gönderdiği konumda **olmayanlar** ve kuyruktakiler
yazılıyor.

İçe aktarma tweak konumlarını eskisi gibi yalnızca **kuyruğa alıyor** — Uygula'ya basılana kadar
hiçbir şey yazılmıyor, bu değişiklik o söze dokunmuyor. Görünüm ve ayar blokları ise uygulamanın
kendi tercihleri olduğu için hemen geçerli olur; bu yüzden dosya böyle bir blok taşıyorsa bir kez,
iki düğmeli bir diyalogla soruluyor. Hangi düğmeye basılırsa basılsın tweak'ler kuyruğa alınıyor.

Bu build'in tanımadığı bir tema ya da yazı tipi adı — daha yeni bir sürümden gelen dosya — yok
sayılıyor, çünkü bilinmeyen bir değer, olmayan bir değerden daha kötü davranmamalı: kullanıcının
temasını sıfırlamak yerine olduğu gibi bırakıyor.

### Değiştirildi

#### `-Wall -Wextra -Wshadow` artık build'in kendisinde

0.9.10 commit'i "compiles clean under -Wall -Wextra -Wshadow" diyordu ama `CMakeLists.txt`'de
tek bir uyarı bayrağı yoktu: bu, elle hatırlanması gereken bir kontroldü. Ölçüldü — yirmi bin
satırın tamamı bu üç bayrak altında **sıfır** uyarı veriyor. Yani korunacak durum zaten
mevcut olan durum; bayraklar artık MinGW/GCC derlemelerinde her zaman açık.

Bilerek fatal değil. İlk yeni uyarıyı bulan derleyici build'i bitirip onu göstermeli, orada
durmamalı; okunacağı yer de CI log'u. `-Werror` oraya ait, ve ancak o job bir derleyici
yükseltmesinden sessiz çıktıktan sonra: MSYS2 hangi GCC'yi taşıyorsa onu taşır ve bir salı
günü gelen yeni bir major sürüm, kimsenin yazmadığı bir uyarıyı herkesin commit'inin
düştüğü bir build'e çevirirdi. `ci.yml`'deki gerekçe de buna göre düzeltildi — orada
"uyarı zemini temiz değil" yazıyordu, ki ölçüm bunun tersini söylüyor.

### Kaldırıldı

#### `sec-powershell-v2` — adının söylediği şeyi yapmıyordu

Satırın adı "PowerShell 2.0 motoru", açıklaması eski motoru kapattığını söylüyordu. Yazdığı
değer ise `HKLM\SOFTWARE\Policies\Microsoft\Windows\PowerShell\EnableScripts = 1` — bu,
yürütme politikasının **MachinePolicy** kapsamını açan değer (`Get-ExecutionPolicy -List`'in
ilk satırı), ve kardeşi olan `ExecutionPolicy` REG_SZ değeri olmadan tek başına etkisiz.
PowerShell 2.0 motorunun registry anahtarı yok; o bir Windows optional feature'ı, yani DISM
işi — motorun kendi kayıt yeri (`HKLM\SOFTWARE\Microsoft\PowerShell\PowerShellEngine`)
bu makinede hiç bulunmuyor.

391 satır içinde metniyle yazdığı değer başka bir alt sisteme ait olan tek satırdı. Hiçbir
zaman vaat ettiği şeyi yapmadığı, yaptığı şeyin de etkisiz olduğu için satır kaldırıldı;
on dildeki iki çevirisi de onunla gitti. Gerçekten PowerShell 2.0'ı kapatan bir şey istenirse
yeri katalog değil, `actions.json`'daki bir DISM tek-atışı.

### Düzeltildi

#### Servis satırları, kullanıcının seçmediği bir Start değeri yazabiliyordu

`Tweak::literal` tam olarak şu satırlar için var: `defaultOption`'ı Windows'un gönderdiği
değerden değil, bu makinede bulunan durumdan türetenler. Böyle bir satırı varsayılan konumuna
almak journal'a danışmamalı — kendi option verisini yazmalı. Başlangıç öğeleri bunu ayarlıyordu
(`catalog.cpp:291`), servisler ayarlamıyordu, oysa `defaultOption`'ı tam aynı şekilde canlı
makineden türetiyorlar.

Sonuç: bir oturumda disable edilen servis için sonraki açılışta `defaultOption` Disabled olarak
hesaplanır. Kullanıcı Auto seçip fikrini değiştirip Disabled'a dönerse `tweakengine.cpp:227`'deki
`restoring` true olur ve motor journal'daki eski `Start`'ı yazar. Servis Auto'da kalır, satır ve
uygulanan sayacı Disabled der. Kilitli servisler zaten dışarıda olduğu için makine bootsuz
kalmıyordu, ama servis satırları katalogdaki en kalabalık grup.

`AppState::applyOne()` artık başarılı bir yazmadan sonra `refreshFromMachine()` çağırıyor.
"Yazma başarılı döndü" ile "makine artık o konumu okuyor" aynı iddia değil, ve yapıcı zaten
ikincisine inanıyor: `m_applied`'ı `readAll()` ile dolduruyor. Motorun sözüne güvenmek, satırın
oturum boyunca bir şey, bir sonraki açılışın başka bir şey söylemesine yol açıyordu — bu sınıf
farklılığın tamamını kapatıyor.

#### README, uygulamanın yapmadığı bir şeyi yaptığını söylüyordu

İki yerde: *"Create a System Restore point straight from the app"* ve özelliklerin arasında
sayılan *"restore points"*. Uygulama geri yükleme noktası oluşturmuyor — `settingspage.cpp:310`
Windows'un kendi Sistem Koruması penceresini açıyor, ve oradaki yorum bunun bilinçli olduğunu
söylüyor: nokta oluşturmak sistemi değiştirir, bu build ise istenmeden hiçbir şey yazmıyor.
Uygulama içi metin (`settings.restore.point`) zaten dürüsttü; README onu takip ediyor artık.

#### Action'lar başarısızlığı başarı olarak raporluyordu

PowerShell'in varsayılanı bir cmdlet hatasını yazdırıp bir sonraki satıra geçmek ve 0 ile
çıkmak. Buradaki her script de kendi sonuç satırını `Write-Output 'ARB|…'` ile basarak
bitiyor — o bir cmdlet ve her zaman başarılı. Yani asıl işi başarısız olan bir action
`exitCode == 0` döndürüyor (`actionengine.cpp:73`) ve kullanıcıya temiz koşu için yazılmış
sonuç cümlesi gösteriliyordu.

`Action::script()` artık script'in başına `$ErrorActionPreference = 'Stop'` koyuyor. Sessizce
geçmesi *istenen* satırlar bunu kendileri `-ErrorAction SilentlyContinue` ile söylüyor ve
parametre tercihi yeniyor — bu makinede ölçüldü: korumasız bir cmdlet hatası artık exit 1
veriyor ve ARB satırına hiç gelinmiyor, `-EA SilentlyContinue` taşıyan aynı hata ise exit 0
verip devam ediyor. Preamble `ActionEngine::run()` yerine `script()` içinde, çünkü onay
diyaloğu bu metni gösteriyor ve `action.h`'nin başındaki söz "okuduğun şey çalışır".

Yerel programlar `$ErrorActionPreference`'ı umursamaz, çıkış kodlarıyla konuşurlar. DISM'in
"bitti, yeniden başlatma lazım" dediği 3010 kabul ediliyor, başka her sıfır olmayan kod
action'ı düşürüyor. `cleanmgr`'ınki bilerek okunmuyor — silecek bir şey bulamadığında da
sıfır olmayan dönüyor — ve script bunu kendi içinde yazıyor, çünkü onay diyaloğu okunuyor.

`act-store-search`'te rapor komutla aynı satırdaydı, noktalı virgülden sonra: icacls bir şey
yapmasa da basılıyordu. Ayrıldı ve `$LASTEXITCODE`'a bağlandı. Principal de artık SID ile
adlandırılıyor (`*S-1-1-0`), hem komutta hem de on dildeki geri alma talimatında.

Bu arada ölçülen ve `ownership.cpp`'deki yorumun tersini söyleyen bir şey: Türkçe bir kurulumda
icacls **İngilizce** `Everyone` ve `Administrators` adlarını çözüyor, yerelleştirilmiş `Herkes`
ve `Yöneticiler` ise 1332 ile başarısız oluyor. SID kullanmak yine doğru — hiçbir aramaya
ihtiyaç duymuyor — ama gerekçesi yorumun yazdığı gerekçe değildi; düzeltildi.

#### Action çıktısı yanlış kod sayfasıyla çözülüyordu

`actionengine.cpp:72` `QString::fromLocal8Bit` kullanıyordu. Konsol programları çıktılarını
bir pipe'a yazarken *konsol* çıkış kod sayfasını kullanır — Türkçe kurulumda 857 — Qt ise ANSI
olanla, 1254 ile okur. DISM, cleanmgr ve icacls'in her ASCII olmayan baytı hem eylem panelinde
hem `actions.log`'da bozuk görünüyordu. Çözüm zaten depodaydı: `ownership.cpp` bunu bulup
yerel olarak düzeltmişti. Artık `src/console.h` ortak evi ve ikisi de onu kullanıyor.

#### `act-block-adobe` artık işaretçilerle yazıyor ve geri alınabiliyor

Bu, uygulamada çalışma zamanında internetten gelen verinin ayrıcalıklı bir yazmaya ulaştığı
**tek** yerdi ve üç ayrı sorunu vardı: indirilen dosya olduğu gibi hosts'a ekleniyordu,
iki kez çalıştırılırsa liste iki kez giriyordu, ve `reversible: false` ile "hosts dosyasını
elle düzenleyin" notu taşıyordu.

Artık indirilen metinden yalnızca host satırları alınıyor — `0.0.0.0` veya `127.0.0.1` ile
başlayıp tek bir ad taşıyan satırlar — ve bunlar `# ARBITRIUM-ADOBE-BEGIN` /
`# ARBITRIUM-ADOBE-END` işaretçileri arasına, blok bütün olarak yeniden yazılarak konuyor.
Yeni `act-unblock-adobe` action'ı da tam olarak o bloğu siliyor. Sahte bir hosts dosyasında
ölçüldü: iki kez çalıştırmak satır sayısını değiştirmiyor, filtre indirilen dosyadaki yorum
satırlarını ve `0.0.0.0 evil.example && calc.exe` gibi enjeksiyon denemelerini reddediyor,
kullanıcının kendi satırları duruyor, ve kaldırma dosyayı bayt bayt eski hâline döndürüyor.

Okuma ve yazma aynı kod sayfasında (`Get-Content` Windows PowerShell'de ANSI okur, bu yüzden
`Set-Content -Encoding Default` deniyor), yani dosyada zaten olan her şey çıktığı gibi geri
giriyor. Altı çeviri anahtarı on dilde eklendi; 6. anahtar her dilde o dilin kendi action
adını alıntılıyor.

#### Explorer yeniden başlatma Windows 10'da kabuğu geri getirmiyordu

Bazı tweak'ler ancak kabuk registry'yi yeniden okuduğunda etkili oluyor, yani Explorer'ın yeniden
başlatılması gerekiyor. Windows 11'de süreci öldürdüğünüzde kabuk kendiliğinden geri geliyordu;
Windows 10'da öylece ölü kalıyordu ve kullanıcı masaüstsüz, görev çubuksuz kalıyordu.

Sebep, sanıldığından incelikli. Arbitrium her zaman yükseltilmiş çalışıyor, dolayısıyla
`startDetached("explorer.exe")` yükseltilmiş bir Explorer istiyor — ve Explorer yükseltilmişken
kabuk rolünü üstlenmiyor. Ama işi öylece bırakmıyor da: kendini kullanıcı olarak yeniden başlatan
`CreateExplorerShellUnelevatedTask` adlı zamanlanmış göreve devrediyor. O görev **ilk yükseltilmiş
açılışta oluşturulduğu** için silinmiş, devre dışı bırakılmış ya da tam da bu uygulamanın yaptığı
türden bir debloat sırasında budanmış olabiliyor. Başarısızlık koşullu, bu yüzden başkalarının
Windows 10 makinelerinde görünüp geliştiricinin Windows 11'inde görünmüyordu.

`Shell::restartExplorer()` yeniden yazıldı ve artık adım atmak yerine **kontrol etmek** üzerine
kurulu:

- Kabuk önce **çıkması isteniyor** — gizli "Exit Explorer" menüsünün gönderdiği mesajla — ki
  masaüstü düzenini, görev çubuğu durumunu ve açık tuttuğu simge/küçük resim veritabanlarını
  düzgün kapatabilsin. Gitmezse ancak o zaman sonlandırılıyor, üstelik **yalnızca o tek süreç**:
  eski `taskkill /F /IM explorer.exe` makinedeki her Explorer'ı ada göre öldürüyordu, oturum açmış
  ikinci bir kullanıcının kabuğu dahil.
- Kabuk **yükseltilmemiş** geri geliyor: yeni süreç, oturumdaki sıradan bir sürecin çocuğu olarak
  yaratılıyor ve onun token'ını miras alıyor.
- Sonuç **doğrulanıyor**: `GetShellWindow()` sınırlı süre yoklanıyor ve ancak gerçekten bir kabuk
  masaüstünü tuttuğunda başarı deniyor. Görmediği bir başarıyı rapor eden hiçbir dal kalmadı.

Başarısız olduğunda hata, hangi aşamanın düştüğünü söylüyor — ve kullanıcı gerçekten masaüstsüz
kaldıysa Görev Yöneticisi'nden elle nasıl geri getireceğini de. `explorer.exe` artık
`GetWindowsDirectoryW`'dan mutlak yolla çözülüyor; çıplak ad, yükseltilmiş bir sürecin miras aldığı
PATH'e güvenmek demekti.

`actions.json`'daki üç action da kabuğu artık kendisi öldürmüyor: yükseltilmiş PowerShell'den
başlatılan bir Explorer aynı sorunu yaşıyor. Gerekiyorsa sonuç satırında söyleyip yeniden başlatmayı
uygulama içindeki düğmeye bırakıyorlar — `shell.cpp` artık explorer'a dokunan tek yer.

#### Ekran kartı satırı bir kartın adını, sürücü satırı başka bir kartın sürücüsünü gösteriyordu

İki ayrı yer, iki ayrı yanlış. Ad `EnumDisplayDevices`'in masaüstüne bağlı **ilk** adaptöründe
duruyordu; sürücü sürümü ise kayıt defterinde **sabit yazılmış** `…\Class\{4d36e968-…}\0000`
anahtarından okunuyordu. Hibrit bir dizüstünde bunlar farklı kartlar oluyor — ölçüldü: bir
makinede `0000` NVIDIA, `0001` Intel, `0002` yine NVIDIA. Üstelik iki satır ayrı bloklarda
(Donanım ve Ekran) olduğu için uyuşmazlık göze batmıyordu.

Artık ekran sınıfındaki her adaptör bir kez sayılıyor ve **adı, VRAM'i, sürücü sürümü ve tarihi
birlikte** tutuluyor, yani ikisi bir daha farklı kartlardan gelemiyor. Birden fazla kart varsa
her biri kendi satırında listeleniyor ve masaüstünü süren kart işaretlenip başa alınıyor; tek
kartlı makinelerde 0.9.10'un iki satırı aynen kalıyor.

Tekilleştirme kuralı ölçümle seçildi. `MatchingDeviceId` tam olarak yanlış anahtar: aynı NVIDIA
kartının iki girdisi yalnızca alt sistem kimliğinde ayrılıyor (`subsys_14a71462` /
`subsys_14a61462`), yani o kural bir kartı ikiye bölerdi — ikisinin tek fiziksel kart olduğu
`Enum\PCI` altında aynı aygıt örneğine (`A8BA505BB42DB04800`) çıkmalarıyla doğrulandı. Kural
şu: ekranda aynı adı, aynı belleği ve aynı sürücüyü çizecek iki girdi tek satırdır. Bedeli,
gerçekten özdeş iki kartın tek satıra düşmesi — hibrit dizüstünden çok daha nadir, ve olmayan
bir kart uydurmak yerine eksik sayıyor.

Gerçek adaptörü sınıf anahtarının defter kayıtlarından ayıran şey `DriverDesc` — VRAM olamazdı,
çünkü ölçüldü: Intel girdisi `HardwareInformation.qwMemorySize` taşımıyor, yani o filtre
makinede gerçekten olan bir kartı düşürürdü.

#### Bellek satırı kurulu değil, görünen belleği gösteriyordu

`GlobalMemoryStatusEx().ullTotalPhys` işletim sisteminin adresleyebildiğini verir, takılı olanı
değil. Ölçüldü: 34.048.495.616 bayta karşı 34.359.738.368 kurulu — aradaki 297 MiB'yi firmware ve
dahili grafik tutuyor. `formatBytes` tam GB'a yuvarladığı için bu makinede fark kayboluyordu, ama
yuvarlama eşiği 512 MiB: dahili GPU'ya 2 GB ayıran bir makine 16 GB takılıyken **15 GB** gösterirdi.

Kurulu toplam artık SMBIOS tip 17 modül boyutlarının toplamı — dosya o tabloyu zaten okuyordu,
sadece kapasiteyi toplamıyordu. Boyut alanı spesifikasyona göre çözülüyor: `0x7FFF` ise 0x1C'deki
genişletilmiş DWORD, değilse 15. bit KB/MB seçiyor. Tabloya güvenilmesi için görünen değerden
küçük olmaması aranıyor; bu, tip 17 hiç olmayan sanal makineyi de kapsıyor.

Kurulu ile görünen yuvarlandıktan sonra da farklıysa satır ikisini birden yazıyor —
`16 GB DDR5-5600 (14 GB kullanılabilir)` — Windows'un kendi Sistem sayfasının yaptığı gibi.
Yalnızca farklıyken, çünkü çoğu makinede fark yok ve her seferinde yanındaki sayıyı tekrarlayan
bir parantez gürültüdür; ama farklı olduğunda susmak daha kötü: bu satırın etiketi "Bellek",
"Takılı" değil, ve iki blok ötedeki "Kullanımda" ile "Boşta" görünen değerden hesaplanıyor.

Karışık hızlı modüllerde artık **en düşük** yapılandırılmış hız gösteriliyor — denetleyici
kanalları birlikte saatlediği için makinenin gerçekten çalıştığı hız o. Modüller türde
anlaşmıyorsa tür tahmin edilmek yerine boş bırakılıyor. Ve SMBIOS tablosu üç ayrı yerde üç kez
çekilip ayrıştırılıyordu; artık bir kez.

### Güvenlik

#### Yükseltilmiş process, kendi yazmadığı iki girdiyi artık sorguluyor

**Güncelleme adresi.** `updater.h`'nin başındaki söz net: kontrol hiçbir şey indirmez ve
çalıştırmaz, kullanıcıya sürüm sayfasını verir. Ama o sayfanın adresi GitHub API yanıtındaki
`html_url` alanından olduğu gibi alınıp `QDesktopServices::openUrl`'e veriliyordu — Windows'ta
bu `ShellExecute` demek, ve bu process her zaman yönetici. Programdaki diğer sekiz `openUrl`
çağrısının hepsi kaynakta yazılı sabitler; ağdan gelen tek adres buydu. Artık şeması `https`
ve host'u `github.com` değilse `releasesUrl()`'e düşüyor. Garanti, adresi tüketen sayfada
değil, üreten `Updater`'da — böylece ileride başka bir yer de onu kullanırsa taşınıyor.

**İki DLL yüklemesi.** `sysinfo.cpp`'de TPM (`tbs.dll`) ve son oturum zamanı
(`netapi32.dll`) için yapılan `LoadLibraryW` çağrıları varsayılan arama sırasını
kullanıyordu; o sıra, exe'nin başlatıldığı klasörü System32'den önce yoklar. Arbitrium
İndirilenler klasöründen çalıştırılan tek bir taşınabilir dosya ve her zaman yükseltilmiş:
yanına konan bir `tbs.dll` yönetici yetkisiyle yüklenirdi. İkisi de artık
`LOAD_LIBRARY_SEARCH_SYSTEM32` ile System32'ye sabitlenmiş durumda. Process geneli
`SetDefaultDllDirectories` **bilerek** konmadı — o, native dosya diyaloglarının yüklediği
shell extension'ları da etkiler ve bu değişikliğin böyle bir takasa ihtiyacı yok.

#### Yayınlanan dosyalar artık nereden geldiklerini kanıtlıyor

Release workflow'u her varlığa `actions/attest-build-provenance` ile bir **build provenance
attestation** ekliyor: GitHub, exe ve zip'in özetlerini bu depoya, derlendikleri commit'e ve
onları derleyen workflow koşusuna bağlayan bir ifadeyi kısa ömürlü bir Sigstore sertifikasıyla
imzalıyor. Doğrulamak için kimseden anahtar gerekmiyor:

```
gh attestation verify Arbitrium-vX.Y.Z-win64.exe --repo shadesofdeath/Arbitrium
```

İmzasız, yönetici hakkı isteyen bir binary için bu, kullanıcının "bilinmeyen yayıncı" sorusuna
kendi kontrol ettiği bir şeyle cevap verebildiği tek yer. `build` job'ında değil `publish`'te,
iki sebeple: özetlenen baytlar tam olarak yüklenen baytlar, ve `id-token: write` bir saat
boyunca üçüncü taraf action çalıştıran job'ın dışında kalıyor.

Yanında bir **`.sha256`** dosyası da yayınlanıyor, ne olduğu açıkça yazılarak: bozuk inen bir
indirmeyi yakalar, o kadar. Exe'yi değiştirebilen aynı hamlede onu da değiştirir. Provenance'ı
kuran şey attestation.

#### `contents: write` artık yalnızca yayınlayan job'da

Workflow seviyesindeki tek `contents: write` satırı, üçüncü taraf action'ları çalıştıran
`build` job'ına da release oluşturup tag taşıyabilen bir token veriyordu. Artık workflow
seviyesi `contents: read`; `publish` kendi bloğunda `contents: write` + `id-token: write` +
`attestations: write` istiyor. Job seviyesindeki blok workflow seviyesindekini **eklemez,
değiştirir** — o yüzden her job'ın aldığı token tek tek izlendi.

#### Qt tarball'ları artık doğrulanıyor, tag artık çapalı

`curl … | tar -xJ` hiçbir kontrol yapmıyordu ve sonuç sonraki release'lerin linklediği cache'e
yazılıyordu. Beklenen SHA-256'lar artık **depoda** sabit ve açılmadan önce kontrol ediliyor;
host'un yanındaki `.sha256`'yı çekmek değil, çünkü o dosyayı da aynı host veriyor. Digest'ler
bağımsız olarak indirilip doğrulandı (qtbase 50.648.500 bayt, qtsvg 2.336.944 bayt).

Tag şekil kontrolü `case` glob'uydu (`v[0-9]*.[0-9]*.[0-9]*`); sondaki `*` her şeyi yakalıyor,
yani `v0.9.1"; echo hi; #` şekil kontrolünden geçip sonraki adımlara yerleştiriliyordu. Artık
çapalı bir regex, gerçek kabukta on bir örneğe karşı sınandı.

### Erişilebilirlik

#### Tweak satırları artık ekran okuyucuya adlarını söylüyor

Bir tweak satırındaki hiçbir şey hazır bir widget değil: ad ve açıklama `paintEvent`'te
çiziliyor, yanındaki kontrol de özel. Yani bir ekran okuyucu için satır, içinde adsız bir şey
olan adsız bir dikdörtgendi — `setAccessibleName` tüm `src/` içinde hiç geçmiyordu.

Metinler zaten üye olarak duruyordu, çünkü satır onları çizmek için tutuyor. Hem satıra hem
kontrole veriliyorlar; kontrole de, çünkü odağı alan o. Uygulanamayan bir satır ise kendini
açıklaması yerine *neden* uygulanamadığıyla tanıtıyor — görsel tarafta soluk hâli ve gereksinim
satırı zaten bunu söylüyor. Registry'ye yönetici yetkisiyle yazan bir programda hangi anahtarın
odakta olduğunu söyleyememek kozmetik bir eksik değil.

---
## [0.9.10] — 2026-08-27

Bir yeni araç, altı yeni tema, dört yeni vurgu rengi — ve 0.9.9'un kendi değişikliğinde
açtığı dokuz gerilemenin kapatılması. 0.9.9 diff'i, düzeltmeleri yaparken yeni hata sokup
sokmadığını görmek için ayrıca adversarial denetime alındı; aşağıdaki "Düzeltildi" başlığı
onun bulduklarıdır.

### Eklendi

#### TrustedInstaller Başlatıcı

Sol menüye, Eylemler'in altına yeni bir sekme. Bir yöneticinin bile önce sahipliğini almadan
dokunamadığı dosya ve anahtarların sahibi olan hesabı — TrustedInstaller — kullanarak bir
program veya dosya başlatır. Bir yol yazın ya da Gözat ile seçin; Komut İstemi, PowerShell,
Kayıt Defteri Düzenleyicisi ve Dosya Gezgini için tek dokunuşluk kısayollar da var.

Başlatma yerel Win32 (`src/trustedinstaller.*`), betik değil: TrustedInstaller hizmeti
başlatılır, ardından yeni süreç o process'i **ebeveyn göstererek** birincil belirtecini miras
alır — NSudo/PowerRun'ın kullandığı yöntem. Makinede hiçbir şey yazılmaz; yalnızca uygulama
zaten yükseltilmiş çalıştığı için mümkün, standart bir belirteç bunu hiçbir şekilde yapamaz.
Yürütülebilir bir dosya doğrudan çalışır; başka her şey (bir betik, bir belge, bir kurulum
paketi) kabuk üzerinden, kendi ilişkili uygulamasında, yine TrustedInstaller olarak açılır.
Başlatılan kabukta `whoami` → `nt service\trustedinstaller`. Uygulama yükseltilmemişse sayfa
kararıp nedenini söyler. Tüm metinler on dilde.

#### Sekiz tema

Görünüm dark/light'tan sekiz palete çıktı. İkisi değişmedi; altısı yeni:

| Tema | Ne |
|---|---|
| **Gece** | Siyaha yakın, OLED dostu, kontrastı yükseltilmiş koyu |
| **Sepya** | Sıcak kâğıt zemin, kahve-gri metin (açık) |
| **Okyanus** | Maviye çalan koyu |
| **Orman** | Yeşile çalan koyu |
| **Alaca** | Mora çalan koyu |
| **Gül** | Rosé'ye çalan koyu |

Dört tonlu koyu, kontrastı doğrulanmış Dark paletinden **her tokenin parlaklığı birebir
korunarak, yalnızca ton kaydırılarak** üretildi — hepsinde en soluk içerik tokenı 4.5:1'i
geçer, Dark'ın referansı gibi. Tint bilerek hafif: fark edilen ama bağırmayan bir zemin.
Görünüm sistemi iki değerli bir enum'dan isimli bir palet yapısına taşındı; `--theme`,
kalıcılık, tema anahtarı (artık 4×2 ızgara) ve on dildeki isimler buna göre güncellendi.

#### Sekiz vurgu rengi

Dörtten sekize: amber, sage, periwinkle, neutral üstüne kil gülü, sis mavisi, yumuşak mor ve
sakin yeşil. Hepsi aynı düşük doygunluklu register'da — vurgu, dolu bir alan değil, seçili
satırın arkasındaki yıkama ve bir düğme üzerindeki metin olduğu için.

### Düzeltildi

0.9.9'un kendi diff'i denetlendi; bulunan dokuz gerileme:

- **Escape kısayolu ters bağlanmıştı.** Uygulama katmanı açıkken kısayolu kapatması
  gerekirken açıyordu (Escape yine perdenin arkasındaki aramayı siliyordu), katman
  kapanınca da kalıcı olarak kapatıyordu — böylece o oturum boyunca Escape aramayı hiç
  temizlemiyordu.
- **Dil değiştirmek üç SysInfo alanını siliyordu.** Etkinleştirme, son geri yükleme noktası
  ve son güncelleme yalnızca eşzamansız yoklamadan gelir; dil değişiminde `SysInfo::collect()`
  yeniden okunuyor ve bu üçünü boşaltıyordu. `SysInfo::Probe::retranslate()` eklendi; yoklamanın
  sakladığı cevabı, ikinci kez çalıştırmadan yeni struct'a yeniden yansıtıyor.
- **Bir tweak'in Türkçe açıklaması i18n ile çelişiyordu.** `wu-WPFToggleStartMenuRecommendations`'ın
  kutupluluğu ters çevrilirken i18n güncellendi ama katalogdaki satır içi Türkçe açıklama eski
  yönü anlatmaya devam ediyordu — ve `Locale::content` Türkçede tam olarak o satır içi metni
  kullanır. Senkronlandı.
- **Ultimate Performance kaldırma kontrolü tersti.** `-match` silmeden *sonra* çalışıyordu; eşleşme
  silmenin başarısız olduğu anlamına gelirken "plan yoktu" diyor, plan gerçekten yoksa "kaldırıldı"
  diyordu. Silmeden önce ölçülüyor artık, ve kaldırılamama için yeni bir sonuç anahtarı eklendi.
- **CMake, var olmayan bir OpenSSL TLS eklenti hedefini dışlıyordu** (`QOpenSSLBackendPlugin`),
  dolayısıyla hiçbir şey yapmıyordu — gerçek ad `QTlsBackendOpenSSLPlugin`.
- **Tema ayarlayıcıları kalıcılık yazmasını kimlik korumasından *sonra* yapıyordu**, böylece bir
  `--theme` bayrağının ayarladığı değeri kullanıcı ayarlardan aynen seçerse kayıt atlanıyordu.
- **RTL metin yanlış uçtan kırpılıyordu.** Arapça için `ElideLeft`, çift ters çevirmeye yol açıp
  etiketin başını kesiyordu; iki yön için de `ElideRight`, çünkü bidi yeniden sıralaması üç
  noktayı zaten okuma-ucu tarafına koyar.
- **Açılış güncelleme denetimi sürerken butona basılırsa** o basış yutuluyor ve satır
  "Denetleniyor…"da kalıyordu; devam eden isteğin kaynağı artık taşınıyor.
- Silinen `clearStatus()`'a işaret eden bir yorum, taşınan sabitlerden geri kalan yetim bir
  yorum ve boş bir isim uzayı, ve kaldırılan `LinkLabel` ile ölen `Font::link()` biçemi temizlendi.

### Değiştirildi

- Görünüm `Theme::Appearance` artık iki değil sekiz şema taşıyor; palet isimle saklanıyor, sayıyla
  değil, böylece sıra serbestçe değişebilir.
- Vurgu paleti `Theme::accentPresets()` dörtten sekiz renge çıktı; `AccentPicker` genişliğini
  kendi hesapladığı için kod dışında bir değişiklik gerekmedi.
- Tema anahtarı `ThemeSwitch` sabit iki karttan `Order[]` üzerinde bir 4×2 ızgaraya genelleştirildi.

---

## [0.9.9] — 2026-08-26

0.9.8 büyük bir sürümdü ve büyük sürümlerin bıraktığı izleri bıraktı. Bu sürüm yeni özellik
getirmiyor: 0.9.8'in açtığı deliklerin kapatılması, kataloğun kendisiyle çelişen yerlerinin
temizlenmesi ve paketlemenin eski hâline döndürülmesi.

### Düzeltildi

#### Kayıt defteri ve veri bütünlüğü

- **Günlük sayfasındaki "Geri al" paylaşılan bir anahtarı tümüyle siliyordu.** Değerin
  yazılmadan önce var olmadığı kaydedilmişse, geri alma `RegDeleteTreeW` ile anahtarın
  *tamamını* — içindeki her değeri ve her alt anahtarı — kaldırıyordu. `keyExisted` tek bir
  yazma anındaki fotoğraftır ve bu anahtarlar paylaşılır: yalnız
  `SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate` içine on üç tweak yazıyor. `upd-drivers`
  satırını geri almak, ondan sonra aynı anahtara yazılmış her şeyi — bir etki alanı ilkesinin
  yazdıklarını dahil — birlikte götürüyor ve "geri alındı" diyordu. Artık yalnızca o değer
  siliniyor; anahtar ise ancak *şu anda* bomboşsa ve yalnızca `RegDeleteKeyExW` ile (alt
  anahtarı olan bir anahtarı reddeder) kaldırılıyor.
- **Tüm anahtarı silen bir yazma artık geri alınabilir gibi gösterilmiyor.** `DELETE_KEY` bir
  alt ağacın tamamını götürür; tek değerlik bir günlük satırı onu geri kuramaz. Bu satırların
  düğmesi soluk çiziliyor ve motor açık bir mesajla reddediyor.
- **Günlük, tweak'in kendi yazmalarından *sonraki* durumu kaydediyordu.** "Önceki değer"
  yazma döngüsünün içinde okunuyordu, yani `i` numaralı giriş 0…i-1 zaten uygulandıktan sonra.
  İlk girişi bir `DELETE_KEY` olan yirmi kadar tweak'te (`ctx-control` bunlardan biri) bu,
  uygulamanın bir saniye önce sildiği değerler için "burada hiçbir şey yoktu" yazmak demekti.
  Artık her tweak'in sahip olduğu değerlerin tamamı, hiçbiri yazılmadan önce okunuyor.
- **"Makinede ne varsa ona döndür" yeniden başlatılana kadar hiçbir şey yapmıyordu.**
  `m_originals` yalnızca kurucuda, günlük dosyasından okunuyordu; `journal()` dosyaya
  ekliyor ama haritayı güncellemiyordu. Aç–vazgeç–kapat, yani en olası kullanım, katalogdaki
  varsayılana düşüyordu. Artık ilk yazma haritayı da besliyor.
- **Geçiş adımı 1, kataloğun *şu anki* "Sahipliği al" girişini siliyordu.** 0.9.3 fiili
  `runas`'a taşımıştı, 0.9.5 `ArbitriumTakeOwnership`'e geri getirdi — ve adım hâlâ o adı
  siliyordu. Üstelik bekçisi HKCU'daki bir bayraktı; sildiği anahtar ise makine geneli.
  Artık komut önce okunuyor, `--own`/`--disown` içeren giriş bırakılıyor.
- **Dışa aktarılan `.reg`, sildiği anahtarı hemen yeniden oluşturuyordu.** `.reg` sözdiziminde
  `[anahtar]` başlığı anahtarı *yaratır*; `[-anahtar]`'dan sonra aynı anahtara yazılan değer
  satırları silmeyi geri alıyordu. Yaklaşık yirmi `ctx-*` tweak'i tam olarak bu şekilde.

#### Genel Bakış'ın on iki yeni bloğu

- **PowerShell çıktısı yanlış kod sayfasıyla okunuyordu ve İngilizce olmayan bir makinede
  iki aşamanın ikisi de tümüyle çöpe gidiyordu.** Bir GUI süreci `powershell.exe` başlattığında
  konsolun çıktı kod sayfası sistemin OEM'idir — Türkçe kurulumda 857. Cevabın herhangi bir
  yerindeki tek bir ASCII dışı karakter (bir diskin adı, bir Wi-Fi SSID'si, yerelleştirilmiş
  bir `fsutil` satırı) Qt'ye geçersiz UTF-8 olarak ulaşıyor; Qt bunu atlamıyor, tüm belgeyi
  `IllegalUTF8String` ile reddediyor. Tek karakter, ve Envanter ile Donanım aşamalarının
  tamamı yok. Konsol artık açıkça UTF-8'e alınıyor. Betiğin kendisi de `fromLatin1` yerine
  `fromUtf8` ile okunuyor — iki betikte gömülü olan `·` PowerShell'e `Â·` olarak gidiyordu.
- **Boş kalan satırlar artık "—" diyor.** `DeepInfo::Facts`'in başlığı her alanın "—" ile
  başladığını söylüyordu; hiçbirinin başlangıç değeri yoktu. Cevaplanamayan satır boş
  çiziliyordu.
- **"TPM sahipliği" satırı hiçbir zaman doldurulmuyordu.** Sayfa satırı çiziyordu, hiçbir yer
  alana yazmıyordu. Donanım aşaması artık `Win32_Tpm`'e soruyor.
- **DHCP satırı her makinede "—" idi.** `GetAdaptersAddresses` `GAA_FLAG_INCLUDE_GATEWAYS`
  olmadan çağrılıyordu, dolayısıyla `FirstGatewayAddress` hep boştu ve döngü her bağdaştırıcıyı
  eliyordu.
- **En yaygın iki "yeniden başlatma bekliyor" bayrağı hiç görülemiyordu.** Windows bunları
  *boş* anahtar olarak oluşturur; kod ise anahtarın içinde bir şey olup olmadığına bakıyordu.
  Dahası QSettings okumak için de `RegCreateKeyEx` kullanır — bayrağı ararken yazma ihtimali
  vardı. Artık `RegOpenKeyEx` ile soruluyor.
- **Üçüncü taraf zamanlanmış görev sayısı beşte bir çıkıyordu.** Sayım kök yolundaki (`\`)
  görevleri hariç tutuyordu; kurulum programlarının görevlerini bıraktığı yer tam olarak orası.
- **Pil sağlığı, pili olan makinede "Yok" diyordu.** WMI pil sınıflarının olmaması ile pilin
  olmaması ayrı sorular; artık ayrı cevaplanıyor.
- **Sayfa GiB ile GB'ı karıştırıyordu.** İki değer `formattedDataSize` varsayılanıyla IEC
  birimi basıyor, komşuları GB basıyordu.
- **Saat dilimi yılın yarısında bir saat şaşıyordu.** `DaylightBias`, yaz saati yürürlükte
  olsun olmasın ekleniyordu — hangisinde olduğunu yalnızca çağrının dönüş değeri söyler. Tam
  sayı bölmesi ayrıca yarım saatlik dilimleri kırpıyordu (Hindistan UTC+5 okunuyordu).
- **Mantıksal çekirdek sayısı yalnızca geçerli işlemci grubunu sayıyordu**, fiziksel sayı ise
  hepsini. Aynı etikete iki farklı kapsam besleniyordu.

#### Dil

- **Dil değiştirmek etiketleri çeviriyor, değerleri eski dilde bırakıyordu.** Hem
  `SysInfo::Facts` hem `DeepInfo::Facts` bitmiş cümleler taşır — "Açık", "Şebeke", yerele göre
  yazılmış bir tarih. Aynı yapıyı yeniden itmek bunları çevirmez. `SysInfo` yeniden okunuyor;
  `DeepInfo::Probe::retranslate()` ise iki PowerShell aşamasının cevabını sakladığı JSON'dan
  yeniden kuruyor, yani bedeli ikinci kez ödemiyor.
- **Yirmi sekiz hizmet risk notu on dilin dokuzunda Türkçe görünüyordu.** Notlar tabloda
  Türkçe cümle olarak duruyordu; artık `svc.risk.<HizmetAdı>` anahtarlarıyla on dilde.
- **Yüzde işareti dokuz dilde yanlış tarafta yazılıyordu.** `%%1` Qt'de kaçış değildir;
  Türkçedeki "%92" biçimi diğer dillere olduğu gibi kopyalanmıştı.
- Kalan Türkçe sabit metinler (`%1 ekran`, `Eski (BIOS)`, sahiplik penceresinin başlığı)
  tabloya taşındı.
- **Ayarlar'daki dil satırı artık doğru söylüyor.** "Tweak adları ve açıklamaları şimdilik
  yalnızca Türkçe" cümlesi 0.9.8 ile yanlış hâle gelmişti.
- Arama kutusuna `%2` yazmak başlığı bozuyordu: sorgu ile sayı zincirleme `.arg()`'den
  geçiyordu.
- Arapça artık metin yönü verilerek çiziliyor, böylece satır başındaki noktalama doğru tarafta
  duruyor. Arayüzün tümüyle aynalanması ayrı bir iş; bu sürümde yapılmadı.

#### Arayüz

- **"Sık satırlar" ayarı hiçbir şey yapmıyordu.** `compactChanged` sinyalinin uygulamada tek
  bir dinleyicisi yoktu; ayar yazılıyor, satırlar bir sonraki açılışa kadar eski yükseklikte
  kalıyordu.
- **Hizmetler ve Başlangıç bölüm başlıkları boş çiziliyordu.** `MainWindow::visibleSections()`
  bölümü alan alan kopyalarken `titleKey`'i düşürüyordu ve bu iki sentezlenmiş kategori
  başlığını yalnızca anahtarla taşır.
- **Uygulama katmanı pencere yeniden boyutlandırılınca yerinde kalıyordu**, ve 160 ms'lik
  kapanış geçişi hiç oynamıyordu: `setEndValue(0.0)` durmuş bir animasyonda değeri hemen
  yayımlar, dolayısıyla katman `start()` çağrılmadan gizleniyordu.
- **Escape tuşu katmana hiç ulaşmıyordu.** Pencere kısayolu tuşu widget'lara varmadan yutuyor;
  katman açıkken kısayol devre dışı bırakılıyor.
- **Aralık kaydırıcısının sayı etiketine basmak değeri en yükseğe atıyordu.** Etiket sütunu
  raya dahil değil; basış artık orada kabul edilip yok sayılıyor.
- **Filtre denetimi dil değişince yerinden oynuyordu.** `setLabels()` genişliği değiştirir ama
  denetimi kimse yeniden yerleştirmiyordu.
- **`PillButton` her `setText()`'te yeni bir sinyal bağlantısı bırakıyordu** — durum çubuğunun
  "Uygula (n)" düğmesi bunu her bekleyen değişiklikte yapıyor.
- **Yazı tipi dosyaları her seferinde yeniden kaydediliyordu.** `addApplicationFont()` aynı
  dosyayı ikinci kez kaydettiğinde işlem yapmaz sanılıyordu; yapar.
- Dil seçici arayüz ölçeği değişince kendini yeniden ölçmüyordu; Genel Bakış'ın Katalog kartı
  başlığını çevirip satırlarını çevirmiyordu; Ağ bloğunda iki satır aynı etiketi taşıyordu;
  `Css::hairline()` sözünü verdiği 1 piksellik keskin çizgiyi çizmiyordu.
- Uygulamalar sayfası, bir kaldırma sürerken dil değiştirilirse satırların "çalışıyor"
  işaretini kaybedip düğmeleri yeniden etkinleştiriyordu.

#### Sistem modülleri

- **Hizmet listesinin yaklaşık %15'i işe yaramaz satırlardı.** Kullanıcı başına hizmetlerin
  oturum örnekleri (`CDPUserSvc_81365` gibi) listeye giriyordu; bunlar oturum açılışında
  yaratılır, kapanışında yok olur ve değiştirilemez. Şablonları kalıyor — değiştirilebilen
  şey zaten onlar.
- **Kaldırma betiği başarısız kaldırmayı başarı sayıyordu.** Her istenen paket koşulsuz
  "kaldırıldı" listesine ekleniyordu; hatalar bastırıldığı için `powershell.exe` de 0 ile
  çıkıyordu. Artık her paket iki listede de yok olduğu doğrulanıyor ve kalanlar adıyla
  bildiriliyor.
- **Bir eylem başlatılamadığında satır kalıcı olarak "çalışıyor"da kalıyordu.**
  `QProcess::start()` başlatma hatasını eşzamanlı bildirir, dolayısıyla `started` sinyali
  `finished`'dan *sonra* çıkıyordu.
- **`SysInfo::Probe` başlatılamayan bir süreci hiç çözmüyor ve `QProcess`'i sızdırıyordu.**
- **`REG_MULTI_SZ` okuması tampon dışına taşıyordu** — yirmi satır yukarıda düzeltilmiş olan
  hatanın aynısı.
- **Sahiplik araçlarının çıktısı yanlış kod sayfasıyla çözülüyordu** (ANSI, oysa konsol OEM).
- **Ultimate Performance eylemleri Türkçe Windows'ta çalışmıyordu.** Plan `powercfg -list`
  çıktısında `Ultimate|Üstün` diye aranıyordu; Türkçe adı "Nihai Performans". Üstelik
  bulunamayınca `.ToString()` null üzerinde çağrılıp hata veriyor, betik devam edip başarı
  bildiriyordu. Artık plan sabit bir GUID ile oluşturulup o GUID ile siliniyor.
- **Güncelleme denetimi açılışta tarayıcıyı kendiliğinden açıyordu.** Ayarın kendi açıklaması
  "sessizce bakar" diyor; sessiz denetim artık yalnızca satırı ve bildirimi güncelliyor.

### Değiştirildi

#### Katalog: 411 → 391 tweak

0.9.8'in 143 yeni tweak'i, var olanlarla on yedi yerde çakıştı: iki satır aynı kayıt değerine
sahip oluyor, hangisini en son çevirdiyseniz o kazanıyor ve diğeri yanlış durumu gösteriyordu.
Her çiftte değeri daha geniş kapsayan satır tutuldu; eşdeğer olanlarda eski kimlik tutuldu,
çünkü ön ayar dosyaları ve günlük kayıtları onu adlandırıyor. `wu-WPFTweaksRazerBlock`'un tek
özgün değeri `upd-drivers`'a taşındı, dolayısıyla hiçbir yetenek kaybedilmedi.

Üç tweak de tümüyle kaldırıldı, çünkü yazıldıkları hâlde çalışamazlardı:

- `net-nagle` — `TcpAckFrequency` ve `TCPNoDelay`'i `…\Tcpip\Parameters\Interfaces` kapsayıcı
  anahtarına yazıyordu. TCP/IP bunları yalnızca altındaki bağdaştırıcı başına `{GUID}`
  anahtarlarından okur. Katalog kendi yazdığını geri okuduğu için satır "uygulandı" diyordu.
- `wu-WPFToggleHideSettingsHome` — açıkken `show:home` yazıyordu; `SettingsPageVisibility`
  böyle bir sözcük tanımıyor. Kapalıyken ise etkin bir ilke kuruyordu. `vis-02-16017` aynı
  değeri zaten doğru biçimde tutuyor.
- `upd-no-auto` — `NoAutoUpdate`'in ikinci sahibi; `upd-auoptions` bunu zaten bir konum olarak
  ifade ediyor.

Bunların dışında: fare hızlanmasının üç değeri ile Yapışkan Tuşlar bayrağı `DWORD` ilan
edilmişti, oysa Windows'ta hepsi `REG_SZ` — uygulamak değerin türünü değiştiriyordu.
`wu-WPFToggleStartMenuRecommendations` kapalı konumunda üç ilke *kuruyordu*; konum 0 uygulama
genelinde "Windows'un getirdiği hâl" demektir. `cln-dns-negative-ttl`'in varsayılanı üretilen
adımların üzerinde değildi, sessizce yok sayılıyordu.

#### Sürüm paketlemesi

0.9.8, 0.9.5'ten beri her sürümün taşıdığı iki dosyanın ikisini de taşımadı. Bu workflow'un
kestiği ilk sürümdü ve workflow dinamik bir Qt ile derliyordu: sonuç, `Arbitrium.exe` artı her
Qt DLL'i, hiçbir şeyin çağırmadığı iki D3D derleyicisi ve bir avuç CMake artığı içeren 21 MB'lık
bir arşiv oldu. Ondan önceki her sürüm, yanına hiçbir şey gerekmeyen tek bir 34 MB'lık
çalıştırılabilir dosyaydı.

Bu sürüm onu geri getiriyor ve şeklin bir daha kaymaması için üç şey ekliyor: derleme MSYS2'nin
`qt6-static` paketiyle yapılıyor; `objdump` bitmiş dosyanın içe aktarma tablosunu geri okuyup
Qt, libgcc, libstdc++ veya libwinpthread görürse işi düşürüyor; ve release, derlemeden *sonra*,
varlıkları ekleyen işin içinde oluşturuluyor — eski sıra, yani önce yayımla sonra derle, tam
olarak 0.9.8'in yanlış içerikle ve hiçbir yerde hata olmadan var olabilmesinin yolu.
`workflow_dispatch` artık bir `dry_run` girdisi de alıyor: her şeyi derler, denetler ve hiçbir
şey yayımlamaz.

#### Ortak hâle getirilen kod

- `Registry::openKey()` ve `Registry::isElevated()` — `hklm()`/`hkcu()` yardımcıları üç
  dosyada, yükseltme yoklaması iki dosyada aynen yazılmıştı. QSettings'in okumak için bile
  anahtar *yarattığı* uyarısı artık tek bir yerde duruyor; bu tam da yukarıdaki
  yeniden-başlatma bayrağı hatasının kaynağıydı.
- `Theme::Metric::PagePad*` — yedi sayfanın her biri aynı dört sayının kendi kopyasını
  taşıyordu.
- `Css::rowPadY()` / `rowNameLine()` / `rowDescLine()` — `TweakRow` ve `SettingRow`'da
  bayt bayt aynıydı.
- `Theme::accentSoft(Appearance)` — vurgu yıkamasının alfası `ThemeSwitch` içinde ayrıca
  yazılmıştı ve değerler ayrışmıştı, yani tema önizlemesi önizlediği kenar çubuğuna
  benzemiyordu.
- Tema ayarlayıcıları artık bir `Persist` parametresi alıyor. Komut satırı anahtarları tek
  seferlikti ama kalıcı ayarlayıcılardan geçiyordu: bir rengi denemek için `--accent` vermek
  onu kalıcı olarak kaydediyordu. `--theme` ayrıca "light değilse dark" diye okuyordu, yani
  her yazım hatası koyu temaya dönüyordu.

### Kaldırıldı

Hiçbir yerden çağrılmayan kod: `AppState::applyPending()` ve `committed` sinyali (uygulama
tamamen `applyOne()` üzerinden yürüyor), `AppState::toggle()`, `lastAppliedAt()` ve onunla
birlikte hiç okunmayan `state/lastApplied` kaydı, `TweakEngine::needsElevation()`,
`Outcome::restoredOriginal`, `ActionPage::m_descriptions`, `Theme::accentName()`,
`Locale::hasTranslation()`, `Registry::hiveToString()`, `Icons::logoDiamond()`,
`DebloatRow::clearStatus()`, `ContentHeader::setFilterIndex()`, `ActionEngine::runningId()` ve
`logPath()`, `forEachTweakInCategory()`, `LinkLabel` widget'ının tamamı, kullanılmayan palet
renkleri (`page`, `closeHover`, `linkHover`) ve ölçüleri, iki yazı tipi biçemi ve karşılığı
olmayan i18n anahtarları.

---

## [0.9.8] — 2026-08-24

### Eklendi

#### Genel Bakış'a on iki yeni blok

Sayfa 16 karttan 28 karta çıktı. Yeni bloklar `SysInfo::collect()`'in tek karede
cevaplayamayacağı şeyleri okur, bu yüzden ayrı bir modülden (`src/deepinfo.*`) ve
**üç kademede** gelirler — açılış süresi hiç etkilenmez, her kademe indiği anda
sayfayı tazeler:

| Kademe | Ne okur | Nasıl |
|---|---|---|
| `Instant` | Gizlilik, UAC, sanallaştırma, bağlantı, bütünlük, WinSAT, ayrılan bellek | Kayıt defteri + Win32, alt süreç yok |
| `Inventory` | Hesaplar, zamanlanmış görevler, sürücüler, Windows Update, olay günlüğü | Tek PowerShell çalıştırması |
| `Hardware` | BitLocker, SMART, pil kimyası, termal bölge, Wi-Fi | İkinci çalıştırma |

- **Windows Update** — bekleyen güncelleme, son başarılı kontrol, duraklatma bitiş
  tarihi, sürüm kanalı, `wuauserv` başlatma türü.
- **Sistem bütünlüğü** — son mavi ekran (minidump başlığından okunan gerçek bugcheck
  kodu ve adı), son 24 saatteki kritik olay sayısı, bekleyen yeniden başlatmanın
  *gerekçesi* (bileşen deposu / Windows Update / dosya işlemi / paket / etki alanı),
  minidump sayısı ve boyutu.
- **Zamanlanmış görevler** — toplam, devre dışı, Microsoft telemetri görevlerinin kaçının
  kapalı olduğu, Microsoft dışı görev sayısı.
- **Sürücüler** — Aygıt Yöneticisi hata kodu taşıyan aygıtlar (adı ve koduyla), imzasız
  sürücü sayısı, toplam, en son yüklenen sürücü ve tarihi.
- **Gizlilik** — tanılama veri düzeyi, reklam kimliği, etkinlik geçmişi, konum, yazma
  verisi; hepsi Windows'un okuduğu yerden (önce ilke, sonra kullanıcı ayarı) ve sonunda
  tek bir "%1/5 kısıtlı" özeti.
- **Şifreleme** — birim başına BitLocker durumu ve yöntemi, şifreleme sürerken ilerleme
  çubuğu, kurtarma anahtarının kayıtlı olup olmadığı, TPM sahipliği.
- **Hesaplar** — yerel hesap ve yönetici sayısı, Konuk hesabı, UAC seviyesi (dört
  konumun hangisi olduğu adıyla), parola süresi ve son değişiklikten bu yana geçen gün.
- **Sanallaştırma** — Hyper-V, VBS/HVCI, WSL sürümü ve dağıtımları, Windows Sandbox,
  Credential Guard.
- **Disk sağlığı** — sürücü başına SMART: sağlık durumu, kalan ömür (çubukla), çalışma
  saati, toplam yazılan veri, sıcaklık; ayrıca TRIM ve önyükleme diskinin bölüm stili.
- **Performans** — WinSAT alt puanları, son açılış süresi (Olay 100), sayfa dosyası
  kullanımı, ayrılan bellek.
- **Bağlantı** — varsayılan rotayı taşıyan bağdaştırıcıda DHCP, proxy, DNS-over-HTTPS,
  Wi-Fi SSID ve sinyal, kurulu TCP bağlantı sayısı, ölçülü bağlantı işareti.
- **Sensörler** — CPU sıcaklığı, pil sağlığı (tasarım kapasitesine karşı gerçek kapasite),
  döngü sayısı, fan hızı. Okunamayan bir değer tahmin edilmez, "Okunamıyor" der.

Yeni blokların yüz çeviri anahtarı on dilin tamamına eklendi.

#### 143 yeni tweak, üç yeni kategori

Katalog **268'den 411 tweak'e** çıktı. Kaynaklar aktif olarak bakımı yapılan projeler
üzerinden tarandı: WinUtil'in 66 tweak'inin neredeyse tamamı katalogda zaten vardı,
Optimizer Ocak 2026'da arşivlenmişti, privacy.sexy'nin geçmiş kaydı tarafı ise tümüyle
eksikti. Eklenenler bu boşluklardan seçildi.

**Windows Update** — yeni kategori, 30 tweak. Katalogda bu konuda tek bir tweak bile
yoktu. Otomatik yeniden başlatma, güncelleme davranışı (5 konumlu), özellik ve kalite
güncellemesi erteleme, sürüm sabitleme (24H2'de kal), sürücüleri WU'dan almama, etkin
saatler, bildirim düzeyi, Insider derlemeleri, teslim iyileştirme modu (7 konumlu);
Windows Hata Raporlama, kilitlenme dökümü türü, DiagTrack; ayrılmış depolama, geri
yükleme noktası sıklık sınırı, Depolama Duyarlığı.

**Güvenlik sertleştirme** — yeni kategori, 23 tweak. SmartScreen'in üç ayrı yüzü,
AutoRun/AutoPlay, Windows Script Host, PowerShell 2.0 motoru, LSA korumalı süreç,
anonim SID sayımı, LM karması, WDigest, uzaktan kayıt defteri, RDP NLA, SMBv1,
SMB imzalama, WPAD, IP kaynak yönlendirme, ICMP yönlendirme, Defender bulut/PUA/ağ
koruması, PrintNightmare'in kapatıldığı spooler ayarı, Point and Print, Office makroları.

**Güç yönetimi** — yeni kategori, 11 tweak. USB seçmeli askıya alma, USB denetleyici
uyutma, PCIe ASPM, modern bekleme (S0) yerine S3, hazırda bekletme dosyası ve boyutu,
kapak kapanma eylemi, uyku düğmesi, pil tasarrufu eşiği, uyarlanır parlaklık.

**Temizlik** 3'ten 26'ya. Geçmiş kaydı (son belgeler, program kullanım takibi, atlama
listeleri, arama geçmişi, adres çubuğu önerileri), önbellek denetimi (küçük resim,
Prefetch, DNS TTL, teslim iyileştirme), olay günlüğü boyutları ve PowerShell betik blok
kaydı, kapanış zaman aşımları.

**Bellek & CPU** 9'dan 36'ya. MMCSS yanıt payı, ağ kısıtlama dizini,
Win32PrioritySeparation, DisablePagingExecutive, svchost gruplama eşiği, çekirdek park
etme, güç kısma, SysMain; NTFS son erişim damgası ve 8.3 kısa ad; GameDVR'ın üç anahtarı,
Game Bar, Auto HDR, VRR, tam ekran optimizasyonları, fare ivmelenmesi.

**Ağ & İnternet** 18'den 30'a — yığın ayarı: TCP otomatik ayarlama, Nagle, QoS ayrılmış
bant genişliği, LLMNR, NetBIOS, mDNS, Wi-Fi Sense, ağ bulma, TIME_WAIT süresi, Teredo.

**Menü ekleri** 9'dan 20'ye. Yol olarak kopyala, SHA256 karması, engellemeyi kaldır,
farklı kullanıcı olarak çalıştır, burada PowerShell/Terminal aç, geri dönüşüm kutusunu
boşalt, yanıt vermeyen görevleri sonlandır, ekranı kapat, BitLocker menü girdisini kaldır.

**Ses ve çevre birimleri** — Sistem altında yeni bölüm, 6 tweak.

Yeni tweak'lerin 143'ünün de adı ve açıklaması **on dilin tamamına** çevrildi; yeni
kategori, bölüm ve seçenek etiketleriyle birlikte 302 yeni anahtar. Çeviri tablosu artık
1533 anahtar taşıyor ve hiçbiri eksik dil içermiyor.

Bir not: seçilen gruplardan ikisi kayıt defteriyle temsil edilemiyordu ve o şekilde
eklenmedi. Zamanlanmış görevlerin tek tek açık/kapalı durumu kayıt defterinde geri
alınabilir biçimde durmaz — bunun yerine ilke anahtarı olan dördü (uygulama uyumluluk
değerlendiricisi, zamanlanmış tanılama, MSDT, Xbox oyun kaydetme) tweak olarak eklendi.
Aynı şekilde "son kullanılan izleri" listelerinin çoğu bir anahtarla kapatılmaz,
temizlenir; kaydın kendisini durduran yedi anahtar tweak olarak eklendi.

### Düzeltildi

- **Kenar çubuğu listesi kaydırılabilir hâle geldi.** Liste sabit yükseklikte düz bir
  widget'tı ve alta taşan satır görünmüyordu — kaydırmıyor, hiç çizilmiyordu. Üç yeni
  kategori Gelişmiş'i listenin dışına itecekti; büyük arayüz ölçeğinde aynısı zaten
  oluyordu.

#### Kayıt defteri ve veri bütünlüğü

- **Geçiş adımı artık Arbitrium'a ait olmayan bir kabuk fiilini silmiyor.**
  0.9.5'in `ownershipVerbs` geçişi `HKCR\*\shell\runas` ve
  `HKCR\Directory\shell\runas` anahtarlarını koşulsuz siliyordu. `runas` Windows'un
  kendi tanımladığı bir fiil adı ve "her dosya türü için Yönetici olarak çalıştır"
  girdisini ekleyen yaygın bir kayıt defteri ayarının da kullandığı ad — yani bu adım,
  0.9.4'ü hiç çalıştırmamış makinelerde bile kullanıcının veya başka bir programın
  girdisini siliyordu. Artık anahtarın `command` değeri okunuyor ve yalnızca Arbitrium'un
  kendi yazdığı fiil (`--own` / `--disown`) siliniyor.
- **`MULTI_SZ` değerler artık doğru türle yazılıyor.** `Registry::write` bir `MULTI_SZ`
  değeri sessizce `REG_SZ` olarak yazıyordu; günlükten geri alma (revert) bu yüzden
  aslında liste olan bir değeri tek dizeye çeviriyordu. Okuma tarafı da artık
  öğeler arasındaki ayırıcı null'ları koruyor, yalnızca sonlandırıcıyı atıyor.
- **Hatalı genişlikteki `DWORD`/`QWORD` değerlerde sınır dışı okuma giderildi.**
  `Registry::read` tampon boyutunu doğrulamadan dört ya da sekiz bayt okuyordu.
  Türüne sığmayan bir değer artık tuttuğu baytlar olarak raporlanıyor.
- **Günlük artık gerçekten yazılan değeri kaydediyor.** `TweakEngine::apply`, geri
  yükleme dalı hedefi değiştirmeden *önce* günlüğe yazıyordu; Log sayfası bu yüzden
  hiç yazılmamış bir değeri gösteriyor ve onu geri almayı öneriyordu.
- **Hizmet adları ve açıklamaları sonlandırıcısız kayıt dizelerinde taşmıyor.**
  `Services::readString` uzunluk vermeden `fromWCharArray` çağırıyordu.

#### Çökme ve tanımsız davranış

- **Uygulama taraması çöktüğünde null pointer erişimi giderildi.** Bir `QProcess`
  çöktüğünde hem `errorOccurred` hem `finished` yayılıyor; `errorOccurred` işleyicisi
  süreci temizledikten sonra `finished` işleyicisi çıktıyı null pointer üzerinden
  okumaya çalışıyordu.
- **Seçeneksiz bir tweak uygulanırken `qBound(min > max)` çağrısı giderildi.**
  Sınırlama, boş seçenek listesi kontrolünden önce yapılıyordu.

#### Çalışmayan ayarlar

- **"Uygulamadan önce onayla" artık gerçekten onay soruyor.** Ayar saklanıyor,
  anahtarı Ayarlar sayfasında duruyor ve varsayılan olarak açık — ama hiçbir yerde
  okunmuyordu, dolayısıyla *Uygula* her durumda doğrudan yazmaya geçiyordu.
- **"Açılışta güncellemeleri denetle" artık denetliyor.** `Updater::check()`'in tek
  çağıranı Ayarlar sayfasındaki elle basılan düğmeydi.
- **Başarısız uygulamada hatanın kendisi gösteriliyor.** `ApplyOverlay::finished`
  hata metnini taşımıyordu; `MainWindow` bu yüzden yalnızca kaç tanesinin başarısız
  olduğunu söyleyebiliyor, `mw.notice.applyFailedDetail` çevirisi hiç kullanılamıyordu.

#### Arayüz dili

- **Hizmet ve başlangıç satırları artık dil değişimini takip ediyor.** Katalog bir kez
  kuruluyor; `appendServices()` / `appendStartup()` içindeki `Locale::tr()` çağrıları
  metni uygulamanın açıldığı dilde donduruyordu. Hizmet konumları (Otomatik / Gecikmeli /
  El ile / Devre dışı), başlangıç konumları, bölüm başlıkları ve satır açıklamaları artık
  çizim anında çözülüyor.
- **Çevrilmemiş kalan metinler çevrildi:** hizmet listesi başlığı, `durdu`,
  `dikkat:` öneki, hizmet kilit gerekçesi, `bu hizmet kilitli`, `derleme %1`,
  Windows hata kodu metni ve sahipliği geri verme özetleri. On dilin tamamına eklendi.
- **Alfabetik sıralama ve "en yoğun kategori" artık ekrandaki adı kullanıyor**,
  `catalog.json`'daki ham Türkçeyi değil.
- **Başlangıç öğelerinin çeviri önek testi düzeltildi** (`boot-` → `startup-`);
  hiçbir tweak id'si `boot-` ile başlamıyordu.
- **Ön ayar dosyaları konum etiketini doğru yazıyor.** Hizmet ve başlangıç konumları
  adlarını anahtarla taşıdığı için `label` alanı boştu ve her biri `açık`/`kapalı`
  olarak kaydediliyordu.

### Değiştirildi

- `Catalog::forEachTweak()` / `forEachTweakInCategory()` eklendi: kategori→bölüm→tweak
  üçlü döngüsü uygulamada sekiz kez elle yazılmıştı.
- `Catalog::mutableCategory()` eklendi; iki sentezleyicideki aynı arama döngüsü kaldırıldı.
- `AppState::noteAppliedMove()` eklendi; "etkin" sayacını güncelleyen altı satır üç
  ayrı yerde kopyalanmıştı.
- `Sidebar::isPinnedPage()` eklendi; `MainWindow` içinde üç ayrı yerde beşer karşılaştırma
  olarak yazılan sabitlenmiş sayfa testi tek çağrıya indi.
- `--self-test` artık MULTI_SZ gidiş-dönüşünü de doğruluyor, katalog üzerinde iki yerine
  tek geçiş yapıyor ve raporu tek yazıcıdan geçiriyor.
- Değişken gölgeleme giderildi (`appstate.cpp` `stashed`, `main.cpp` `path`);
  `-Wshadow` ile derleme artık uyarısız.
- `ApplyOverlay::m_dryRun` kaldırıldı — hiç okunmayan bir üye.

#### Bilgi satırlarında iki satırlı düzen

`InfoRow` artık `stacked` taşıyor: etiket üstte, değer altında, ikisi de kartın tam
genişliğinde. Yan yana düzen değeri istediği kadar genişletip etiketi artan yere
sıkıştırıyor; "Sürüm — 24H2 · 26100" için doğru, bir fiziksel diski adlandıran satır
için değil. Depolama, Disk sağlığı, Şifreleme, Sürücüler ve Sistem bütünlüğü satırları
bunu kullanıyor. Yan yana düzende değer de artık kart kenarında kırpılıyor — daha önce
satırdan geniş bir değer sol kenardan taşarak çiziliyordu.

---

[0.12.0]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.12.0
[0.11.0]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.11.0
[0.10.0]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.10.0
[0.9.10]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.10
[0.9.9]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.9
[0.9.8]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.8
