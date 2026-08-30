// console.h — reading back what a console program printed.
//
// takeown, icacls, powershell: with their output redirected to a pipe these encode it in
// the *console* output code page — 857 on a Turkish install — while Qt reads a QByteArray
// back with the ANSI one, 1254 on that same machine. Every non-ASCII byte therefore
// arrives as the wrong character, in exactly the panes that exist to show the user what a
// tool said about their machine.
//
// It lives in its own file because it is the same bug twice: ownership.cpp found it and
// fixed it locally, and the action runner — which shows the output of DISM, cleanmgr and
// icacls in the same spirit, and writes it to actions.log — kept decoding with
// fromLocal8Bit for another eight releases.

#pragma once

#include <QByteArray>
#include <QString>

namespace Console {

/// \a bytes as the console that wrote them meant them. Falls back to QString's own
/// local-8-bit reading when the code page cannot be determined or the text will not
/// convert, which is no worse than the answer without this function.
QString decode(const QByteArray &bytes);

} // namespace Console
