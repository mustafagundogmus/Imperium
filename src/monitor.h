// monitor.h — live machine telemetry for the Genel Bakış screen.
//
// One timer, one sample per second, a rolling 60-sample window. Everything is read with
// plain Win32 counters (GetSystemTimes, GlobalMemoryStatusEx, GetIfTable2) — no WMI, no
// PDH, no elevation, nothing written.

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVector>

class QTimer;

struct Sample
{
    qreal cpuPercent = 0.0;      ///< 0–100, whole machine
    qreal ramPercent = 0.0;      ///< 0–100
    quint64 ramUsed = 0;
    quint64 ramTotal = 0;
    quint64 diskUsed = 0;
    quint64 diskTotal = 0;
    quint64 downBytesPerSec = 0;
    quint64 upBytesPerSec = 0;
};

class SystemMonitor : public QObject
{
    Q_OBJECT

public:
    static constexpr int HistorySize = 60;   ///< seconds shown on the chart

    explicit SystemMonitor(QObject *parent = nullptr);

    void start(int intervalMs = 1000);
    void stop();

    Sample latest() const { return m_latest; }
    const QVector<qreal> &cpuHistory() const { return m_cpu; }
    const QVector<qreal> &ramHistory() const { return m_ram; }

    /// Physical / logical core counts, read once.
    int physicalCores() const { return m_physicalCores; }
    int logicalCores() const { return m_logicalCores; }

Q_SIGNALS:
    void sampled(const Sample &sample);

private:
    /// \a record false takes a reading for display only — the rate counters are not
    /// meaningful yet and nothing is appended to the history.
    void poll(bool record = true);
    qreal readCpuPercent();
    void readNetwork(Sample &sample);

    QTimer *m_timer = nullptr;
    Sample m_latest;
    QVector<qreal> m_cpu;
    QVector<qreal> m_ram;

    quint64 m_prevIdle = 0;
    quint64 m_prevKernel = 0;
    quint64 m_prevUser = 0;
    bool m_haveCpuBaseline = false;

    quint64 m_prevIn = 0;
    quint64 m_prevOut = 0;
    bool m_haveNetBaseline = false;
    QElapsedTimer m_netClock;

    int m_physicalCores = 0;
    int m_logicalCores = 0;
};
