import json
with open("resources/data/i18n.json", "r", encoding="utf-8") as f:
    data = json.load(f)

data["qt.showDetails"] = {
    "tr": "Detayları göster...",
    "en": "Show Details...",
    "de": "Details anzeigen...",
    "fr": "Afficher les détails...",
    "es": "Mostrar detalles...",
    "it": "Mostra dettagli...",
    "pt": "Mostrar detalhes...",
    "pl": "Pokaż szczegóły...",
    "ru": "Показать детали...",
    "ar": "إظهار التفاصيل..."
}

data["qt.hideDetails"] = {
    "tr": "Detayları gizle...",
    "en": "Hide Details...",
    "de": "Details ausblenden...",
    "fr": "Masquer les détails...",
    "es": "Ocultar detalles...",
    "it": "Nascondi dettagli...",
    "pt": "Ocultar detalhes...",
    "pl": "Ukryj szczegóły...",
    "ru": "Скрыть детали...",
    "ar": "إخفاء التفاصيل..."
}

with open("resources/data/i18n.json", "w", encoding="utf-8") as f:
    json.dump(data, f, indent=4, ensure_ascii=False)
