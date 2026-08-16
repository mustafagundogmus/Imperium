// migration.h — cleanup for keys an older build of this app left behind.
//
// When a tweak's registry shape changes, the entries the previous version wrote stop
// being reachable: the new definition no longer names them, so reverting the tweak
// cannot take them back out. Anything stranded that way is removed here, once, at
// startup.
//
// The rule this stays inside: only keys and values Arbitrium itself created are touched,
// recognised by the name it gave them. Nothing that belongs to Windows or to another
// program is in reach of this file.

#pragma once

class Migration
{
public:
    /// Runs whatever steps this install has not seen yet and records how far it got.
    /// Steps that fail are simply skipped — none of them is worth blocking startup for.
    static void runOnce();
};
