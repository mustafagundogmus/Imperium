// services.h — the machine's own Win32 services, read from the registry.
//
// A service's start type is not a switch, it is a position: automatic, automatic
// (delayed), manual, disabled. That is `Start` plus `DelayedAutostart` under
// HKLM\SYSTEM\CurrentControlSet\Services\<name>, which is to say two ordinary registry
// values — so the catalogue can carry each service as a choice tweak and everything
// downstream (pending changes, Uygula, presets, the counters) works unchanged.
//
// Nothing here is curated. The list is whatever this machine has, which is also what
// services.msc shows: kernel and filesystem drivers are filtered out, everything with a
// Win32 service type stays. Names and descriptions come back localised, because Windows
// stores them as indirect strings into its own resource DLLs and this resolves them.

#pragma once

#include <QString>
#include <QVector>

namespace Services {

struct Info
{
    QString key;           ///< registry subkey, e.g. "Spooler"
    QString displayName;   ///< resolved, e.g. "Yazdırma Biriktiricisi"
    QString description;   ///< resolved; often empty
    int start = 3;         ///< 2 automatic · 3 manual · 4 disabled
    bool delayed = false;  ///< DelayedAutostart, only meaningful with start == 2
    bool running = false;  ///< asked of the service control manager, not the registry

    /// Set for services the machine cannot be expected to survive without. Offering a
    /// disable switch for these is not a feature, it is a trap: the list below starts at
    /// a seed of core services and then closes over DependOnService, so anything the
    /// seed needs is locked too — that is where most of the danger actually hides.
    bool locked = false;
    QString lockReason;

    /// Not load-bearing enough to lock, but consequential enough that the row should say
    /// what stops working. The switch stays operable — this is a warning, not a lock.
    QString riskNote;
};

/// Every Win32 service on this machine, sorted by display name. Read-only, no elevation.
QVector<Info> enumerate();

} // namespace Services
