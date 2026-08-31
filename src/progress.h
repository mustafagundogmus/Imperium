// progress.h — how the startup work says where it has got to.
//
// One free function, split out of splashscreen.h so that the parts which report progress
// do not have to include a widget header to do it. catalog.cpp scanning the service
// control manager is exactly the stage a user most needs named, and the catalogue has no
// business knowing what a QWidget is.
//
// The implementation lives in splashscreen.cpp, beside the only thing that can answer it.

#pragma once

#include <QString>

namespace Splash {

/// Names the stage the startup work has reached, repaints the splash and gives the event
/// loop one turn. \a key is an i18n key, resolved at paint time.
///
/// A no-op when no splash is on screen — before it is created, after it has finished, and
/// for --screenshot and --self-test, which skip it entirely — so a caller never has to
/// guard the call or know whether it is running under one.
void report(const QString &key);

} // namespace Splash
