#include "fluenticons.h"
#include "../catalog.h"
#include "../views/sidebar.h"

#include <QHash>

namespace FluentIcons {

// Fetched from https://api.iconify.design/lucide/<name>.svg like the set in icons.cpp, and
// kept the same way: the elements verbatim, without the stroke attributes, which draw()
// puts on a wrapping <g>. lucide is ISC; see resources/licenses/lucide-ISC.txt.
namespace Lucide {

using Icons::Glyph;

const Glyph House{
    "house",
    "<path d=\"M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8\"/>"
    "<path d=\"M3 10a2 2 0 0 1 .709-1.528l7-5.999a2 2 0 0 1 2.582 0l7 5.999A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z\"/>"};
const Glyph Wrench{
    "wrench",
    "<path d=\"M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z\"/>"};
const Glyph History{
    "history",
    "<path d=\"M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8\"/>"
    "<path d=\"M3 3v5h5\"/>"
    "<path d=\"M12 7v5l4 2\"/>"};
const Glyph Settings{
    "settings",
    "<path d=\"M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z\"/>"
    "<circle cx=\"12\" cy=\"12\" r=\"3\"/>"};
const Glyph Bell{
    "bell",
    "<path d=\"M10.268 21a2 2 0 0 0 3.464 0\"/>"
    "<path d=\"M3.262 15.326A1 1 0 0 0 4 17h16a1 1 0 0 0 .74-1.673C19.41 13.956 18 12.499 18 8A6 6 0 0 0 6 8c0 4.499-1.411 5.956-2.738 7.326\"/>"};
const Glyph Megaphone{
    "megaphone",
    "<path d=\"m3 11 18-5v12L3 14v-3z\"/>"
    "<path d=\"M11.6 16.8a3 3 0 1 1-5.8-1.6\"/>"};
const Glyph Search{
    "search",
    "<circle cx=\"11\" cy=\"11\" r=\"8\"/>"
    "<path d=\"m21 21-4.3-4.3\"/>"};
const Glyph MapPin{
    "map-pin",
    "<path d=\"M20 10c0 4.993-5.539 10.193-7.399 11.799a1 1 0 0 1-1.202 0C9.539 20.193 4 14.993 4 10a8 8 0 0 1 16 0\"/>"
    "<circle cx=\"12\" cy=\"10\" r=\"3\"/>"};
const Glyph Camera{
    "camera",
    "<path d=\"M14.5 4h-5L7 7H4a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-3l-2.5-3z\"/>"
    "<circle cx=\"12\" cy=\"13\" r=\"3\"/>"};
const Glyph Mic{
    "mic",
    "<path d=\"M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z\"/>"
    "<path d=\"M19 10v2a7 7 0 0 1-14 0v-2\"/>"
    "<line x1=\"12\" x2=\"12\" y1=\"19\" y2=\"22\"/>"};
const Glyph Folder{
    "folder",
    "<path d=\"M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z\"/>"};
const Glyph RefreshCw{
    "refresh-cw",
    "<path d=\"M3 12a9 9 0 0 1 9-9 9.75 9.75 0 0 1 6.74 2.74L21 8\"/>"
    "<path d=\"M21 3v5h-5\"/>"
    "<path d=\"M21 12a9 9 0 0 1-9 9 9.75 9.75 0 0 1-6.74-2.74L3 16\"/>"
    "<path d=\"M8 16H3v5\"/>"};
const Glyph Power{
    "power",
    "<path d=\"M12 2v10\"/>"
    "<path d=\"M18.4 6.6a9 9 0 1 1-12.77.04\"/>"};
const Glyph Keyboard{
    "keyboard",
    "<path d=\"M10 8h.01\"/><path d=\"M12 12h.01\"/><path d=\"M14 8h.01\"/><path d=\"M16 12h.01\"/>"
    "<path d=\"M18 8h.01\"/><path d=\"M6 8h.01\"/><path d=\"M7 16h10\"/><path d=\"M8 12h.01\"/>"
    "<rect width=\"20\" height=\"16\" x=\"2\" y=\"4\" rx=\"2\"/>"};
const Glyph Mouse{
    "mouse",
    "<rect x=\"5\" y=\"2\" width=\"14\" height=\"20\" rx=\"7\"/>"
    "<path d=\"M12 6v4\"/>"};
const Glyph Volume2{
    "volume-2",
    "<path d=\"M11 4.702a.705.705 0 0 0-1.203-.498L6.413 7.587A1.4 1.4 0 0 1 5.416 8H3a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h2.416a1.4 1.4 0 0 1 .997.413l3.383 3.384A.705.705 0 0 0 11 19.298z\"/>"
    "<path d=\"M16 9a5 5 0 0 1 0 6\"/>"
    "<path d=\"M19.364 18.364a9 9 0 0 0 0-12.728\"/>"};
const Glyph Bluetooth{
    "bluetooth",
    "<path d=\"m7 7 10 10-5 5V2l5 5L7 17\"/>"};
const Glyph Trash2{
    "trash-2",
    "<path d=\"M3 6h18\"/>"
    "<path d=\"M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6\"/>"
    "<path d=\"M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2\"/>"
    "<line x1=\"10\" x2=\"10\" y1=\"11\" y2=\"17\"/>"
    "<line x1=\"14\" x2=\"14\" y1=\"11\" y2=\"17\"/>"};
const Glyph LayoutGrid{
    "layout-grid",
    "<rect width=\"7\" height=\"7\" x=\"3\" y=\"3\" rx=\"1\"/>"
    "<rect width=\"7\" height=\"7\" x=\"14\" y=\"3\" rx=\"1\"/>"
    "<rect width=\"7\" height=\"7\" x=\"14\" y=\"14\" rx=\"1\"/>"
    "<rect width=\"7\" height=\"7\" x=\"3\" y=\"14\" rx=\"1\"/>"};
const Glyph Sparkles{
    "sparkles",
    "<path d=\"M9.937 15.5A2 2 0 0 0 8.5 14.063l-6.135-1.582a.5.5 0 0 1 0-.962L8.5 9.936A2 2 0 0 0 9.937 8.5l1.582-6.135a.5.5 0 0 1 .963 0L14.063 8.5A2 2 0 0 0 15.5 9.937l6.135 1.581a.5.5 0 0 1 0 .964L15.5 14.063a2 2 0 0 0-1.437 1.437l-1.582 6.135a.5.5 0 0 1-.963 0z\"/>"
    "<path d=\"M20 3v4\"/><path d=\"M22 5h-4\"/><path d=\"M4 17v2\"/><path d=\"M5 18H3\"/>"};
const Glyph Cloud{
    "cloud",
    "<path d=\"M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z\"/>"};
const Glyph Clipboard{
    "clipboard",
    "<rect width=\"8\" height=\"4\" x=\"8\" y=\"2\" rx=\"1\" ry=\"1\"/>"
    "<path d=\"M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2\"/>"};
const Glyph Printer{
    "printer",
    "<path d=\"M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2\"/>"
    "<path d=\"M6 9V3a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v6\"/>"
    "<rect x=\"6\" y=\"14\" width=\"12\" height=\"8\" rx=\"1\"/>"};
const Glyph Gamepad2{
    "gamepad-2",
    "<line x1=\"6\" x2=\"10\" y1=\"11\" y2=\"11\"/>"
    "<line x1=\"8\" x2=\"8\" y1=\"9\" y2=\"13\"/>"
    "<line x1=\"15\" x2=\"15.01\" y1=\"12\" y2=\"12\"/>"
    "<line x1=\"18\" x2=\"18.01\" y1=\"10\" y2=\"10\"/>"
    "<path d=\"M17.32 5H6.68a4 4 0 0 0-3.978 3.59c-.006.052-.01.101-.017.152C2.604 9.416 2 14.456 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.414-1.414A2 2 0 0 1 9.828 16h4.344a2 2 0 0 1 1.414.586L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.545-.604-6.584-.685-7.258-.007-.05-.011-.1-.017-.151A4 4 0 0 0 17.32 5z\"/>"};
const Glyph Rocket{
    "rocket",
    "<path d=\"M4.5 16.5c-1.5 1.26-2 5-2 5s3.74-.5 5-2c.71-.84.7-2.13-.09-2.91a2.18 2.18 0 0 0-2.91-.09z\"/>"
    "<path d=\"m12 15-3-3a22 22 0 0 1 2-3.95A12.88 12.88 0 0 1 22 2c0 2.72-.78 7.5-6 11a22.35 22.35 0 0 1-4 2z\"/>"
    "<path d=\"M9 12H4s.55-3.03 2-4c1.62-1.08 5 0 5 0\"/>"
    "<path d=\"M12 15v5s3.03-.55 4-2c1.08-1.62 0-5 0-5\"/>"};
const Glyph Menu{
    "menu",
    "<line x1=\"4\" x2=\"20\" y1=\"12\" y2=\"12\"/>"
    "<line x1=\"4\" x2=\"20\" y1=\"6\" y2=\"6\"/>"
    "<line x1=\"4\" x2=\"20\" y1=\"18\" y2=\"18\"/>"};
const Glyph Zap{
    "zap",
    "<path d=\"M4 14a1 1 0 0 1-.78-1.63l9.9-10.2a.5.5 0 0 1 .86.46l-1.92 6.02A1 1 0 0 0 13 10h7a1 1 0 0 1 .78 1.63l-9.9 10.2a.5.5 0 0 1-.86-.46l1.92-6.02A1 1 0 0 0 11 14z\"/>"};
const Glyph KeyRound{
    "key-round",
    "<path d=\"M2.586 17.414A2 2 0 0 0 2 18.828V21a1 1 0 0 0 1 1h3a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h1a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h.172a2 2 0 0 0 1.414-.586l.814-.814a6.5 6.5 0 1 0-4-4z\"/>"
    "<circle cx=\"16.5\" cy=\"7.5\" r=\".5\"/>"};
const Glyph Info{
    "info",
    "<circle cx=\"12\" cy=\"12\" r=\"10\"/>"
    "<path d=\"M12 16v-4\"/>"
    "<path d=\"M12 8h.01\"/>"};
// The debloat page's parcel with a plus on it: the same package, being added to.
const Glyph PackagePlus{
    "package-plus",
    "<path d=\"M16 16h6\"/>"
    "<path d=\"M19 13v6\"/>"
    "<path d=\"M21 10V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l2-1.14\"/>"
    "<path d=\"m7.5 4.27 9 5.15\"/>"
    "<polyline points=\"3.29 7 12 12 20.71 7\"/>"
    "<line x1=\"12\" x2=\"12\" y1=\"22\" y2=\"12\"/>"};
// Two blocks, one set on the other: the optional components stacked onto the image.
const Glyph Blocks{
    "blocks",
    "<rect width=\"7\" height=\"7\" x=\"14\" y=\"3\" rx=\"1\"/>"
    "<path d=\"M10 21V8a1 1 0 0 0-1-1H4a1 1 0 0 0-1 1v12a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-5a1 1 0 0 0-1-1H3\"/>"};

} // namespace Lucide

QPixmap draw(const Icons::Glyph &g, const QColor &c, int size, qreal strokeUnits, qreal dpr)
{
    QString inner = QStringLiteral("<g fill=\"none\" stroke=\"%1\" stroke-width=\"%2\" "
                                   "stroke-linecap=\"round\" stroke-linejoin=\"round\">")
                        .arg(c.name(QColor::HexRgb))
                        .arg(strokeUnits);
    inner += QString::fromLatin1(g.shapes);
    inner += QStringLiteral("</g>");
    return Icons::fragment(QStringLiteral("fl:%1:%2:%3").arg(QString::fromLatin1(g.name),
                                                             c.name(QColor::HexRgb))
                               .arg(strokeUnits),
                           inner, 24.0, QSize(size, size), dpr);
}

const Icons::Glyph &pageGlyph(const QString &id)
{
    using namespace Lucide;
    using namespace Icons::Lucide;
    static const QHash<QString, const Icons::Glyph *> table = {
        {QStringLiteral("ov"), &House},
        {QStringLiteral("sys"), &AppWindow},
        {QStringLiteral("upd"), &Download},
        {QStringLiteral("vis"), &SunMoon},
        {QStringLiteral("priv"), &EyeOff},
        {QStringLiteral("sec"), &ShieldCheck},
        {QStringLiteral("net"), &Globe},
        {QStringLiteral("svc"), &Layers},
        {QStringLiteral("boot"), &Rocket},
        {QStringLiteral("task"), &CalendarClock},
        {QStringLiteral("exp"), &Folder},
        {QStringLiteral("ctx"), &Menu},
        {QStringLiteral("perf"), &Gauge},
        {QStringLiteral("pwr"), &BatteryCharging},
        {QStringLiteral("cln"), &HardDrive},
        {QStringLiteral("adv"), &TriangleAlert},
        {Sidebar::actionsId(), &Zap},
        {Sidebar::cleanerId(), &Trash2},
        {Sidebar::godModeId(), &LayoutGrid},
        {Sidebar::officeId(), &Icons::Lucide::Download},
        {Sidebar::tiLauncherId(), &KeyRound},
        {Sidebar::debloatId(), &Package},
        {Sidebar::appsId(), &PackagePlus},
        {Sidebar::featuresId(), &Blocks},
        {Sidebar::journalId(), &History},
        {Sidebar::settingsId(), &Settings},
        {Sidebar::aboutId(), &Info},
    };
    const Icons::Glyph *g = table.value(id, nullptr);
    return g ? *g : Icons::Lucide::SlidersHorizontal;
}

const Icons::Glyph &tweakGlyph(const Tweak &t, const QString &categoryId)
{
    using namespace Lucide;
    using namespace Icons::Lucide;

    // The machine's own rows carry Windows' names, not words this table knows.
    if (t.id.startsWith(QLatin1String("svc-")) || t.id.startsWith(QLatin1String("startup-"))
        || t.id.startsWith(QLatin1String("task-")))
        return pageGlyph(categoryId);

    // Matched against the catalogue's Turkish, which is the one text every row has whatever
    // the interface language is. First match wins, so the specific words come first.
    // Substrings, so a word with a leading space is one that must start a word: "ağ" is
    // inside too many other words to be let loose, and " pin" keeps "tipini" from turning
    // into a PIN. Turkish agglutinates, so most stems are given without their endings —
    // "izlen" for izlenme/izlenmesini/izleniyor — but never so short that they land inside
    // another word ("izleme" is inside "temizleme", "deney" inside "deneyim").
    struct Rule { const Icons::Glyph *glyph; const char *words[20]; };
    static const Rule rules[] = {
        {&Camera,      {"kamera", ""}},
        {&Mic,         {"mikrofon", "sesle etkinleştirme", "konuşma tanıma", "konuşma modeli", ""}},
        {&MapPin,      {"konum", "sensör", "cihazımı bul", "harita", ""}},
        {&Bluetooth,   {"bluetooth", ""}},
        {&Printer,     {"yazdır", "yazıcı", "biriktirici", ""}},
        {&Clipboard,   {" pano", ""}},
        {&Volume2,     {" ses ", " sesi", " sesler", "hoparlör", "mikser", ""}},
        {&Keyboard,    {"klavye", "num lock", " tuş", ""}},
        {&Mouse,       {" fare", "tekerlek", "imleç", "dokunmatik", ""}},
        {&Gamepad2,    {"oyun", "game", "xbox", ""}},
        {&Sparkles,    {"yapay zek", "copilot", "recall", "click to do", "paint", ""}},
        {&History,     {"geçmiş", "zaman çizelgesi", "son kullanılan", "son açılan", ""}},
        {&Megaphone,   {"reklam", "öneri", "tanıtım", "ipucu", "ipuçları", "spotlight", "kampanya", ""}},
        {&Bell,        {"bildirim", ""}},
        {&Search,      {"arama", "bing", "cortana", "dizin", ""}},
        {&EyeOff,      {"telemetri", "tanılama", "veri", "rapor", "ceip", "deneyler", "deneysel", "izlen",
                        "takip", "takib", "gönderim", "geri bildirim", "insider"}},
        {&Download,    {"indir", ""}},
        {&RefreshCw,   {"güncelle", "update", "sürücü", "yeniden başlat", ""}},
        {&Cloud,       {"bulut", "onedrive", "eşitle", "senkron", ""}},
        {&Lock,        {"kilit", "kilid", "şifre", "bitlocker", " uac", ""}},
        {&User,        {"hesap", "oturum", "parola", " pin", "kullanıcı", "hello", "biyometri", ""}},
        {&ShieldCheck, {"defender", "smartscreen", "güvenlik", "koruma", "kimlik avı", "virüs", ""}},
        {&Cpu,         {"işlemci", "cpu", "çekirdek", "zamanla", "öncelik", ""}},
        {&MemoryStick, {"bellek", "önbellek", "sayfa dosyası", "prefetch", "superfetch", "sıkıştır", ""}},
        {&HardDrive,   {"disk", "ntfs", "depolama", "ssd", "trim", ""}},
        {&BatteryCharging, {" pil", ""}},
        {&Power,       {" güç", "uyku", "hazırda", "kapanış", " kapatma", "açılış", "önyükleme",
                        "hızlı başlat", ""}},
        {&Menu,        {"menü", "sağ tık", "bağlam", ""}},
        {&Folder,      {"gezgin", "klasör", "dosya", "kısayol", "önizleme", "sekme", ""}},
        {&LayoutGrid,  {"görev çubuğu", "başlat", "widget", "pencere öğ", "masaüstü", "simge", ""}},
        {&Monitor,     {"ekran", "görsel", "animasyon", "saydam", "gölge", "tema", "renk", "yazı tipi", ""}},
        {&Wifi,        {"wi-fi", "wifi", "kablosuz", "hücresel", ""}},
        {&Globe,       {" ağ", "internet", "dns", "ipv6", "teredo", "bağlantı", "smb", "netbios", "llmnr",
                        "paylaş", "tcp", "edge", "tarayıcı", "brave", "chrome", "firefox"}},
        {&Trash2,      {"temizle", " sil", "geçici", "çöp", ""}},
        {&Layers,      {"hizmet", "servis", ""}},
        {&CalendarClock, {"görev", ""}},
        {&AppWindow,   {"office", "visual studio", "media player", ""}},
    };

    // The name first, on its own, and only then the description: a row named for its
    // subject should not be iconed for a word its second sentence happens to use.
    const auto pick = [&](const QString &text) -> const Icons::Glyph * {
        for (const Rule &rule : rules)
            for (const char *word : rule.words) {
                if (!word || !*word)
                    break;
                if (text.contains(QString::fromUtf8(word)))
                    return rule.glyph;
            }
        return nullptr;
    };
    if (const Icons::Glyph *g = pick(QLatin1Char(' ') + t.name.toLower()))
        return *g;
    if (const Icons::Glyph *g = pick(QLatin1Char(' ') + t.desc.left(80).toLower()))
        return *g;
    return pageGlyph(categoryId);
}

} // namespace FluentIcons
