# Changelog

Bu dosya sürümler arasındaki dikkate değer değişiklikleri listeler.
Biçim [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/) temellidir ve
proje [Semantic Versioning](https://semver.org/lang/tr/) kullanır.

## [Yayınlanmamış]

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

[Yayınlanmamış]: https://github.com/shadesofdeath/Arbitrium/compare/v0.9.10...main
[0.9.10]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.10
[0.9.9]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.9
[0.9.8]: https://github.com/shadesofdeath/Arbitrium/releases/tag/v0.9.8
