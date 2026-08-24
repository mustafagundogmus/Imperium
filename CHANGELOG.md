# Changelog

Bu dosya sürümler arasındaki dikkate değer değişiklikleri listeler.
Biçim [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/) temellidir ve
proje [Semantic Versioning](https://semver.org/lang/tr/) kullanır.

## [Yayımlanmadı]

### Düzeltildi

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
