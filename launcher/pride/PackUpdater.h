// SPDX-License-Identifier: GPL-3.0-only
// Pride Launcher: keeps the Pride modpack up to date from the Pride website.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#include "net/NetJob.h"

class BaseInstance;

namespace Pride {

/** where the launcher looks for the pack (override with PRIDE_MANIFEST_URL for testing) */
QString manifestUrl();

struct PackFile {
    QString path;   // relative to the game folder, e.g. "mods/Foo-1.0.jar"
    QString sha1;   // lowercase hex
    qint64 size = 0;
    QString url;    // absolute download URL
};

struct Manifest {
    QString version;
    QString released;
    QString notes;          // what changed (short text, shown in the launcher)
    QString instanceZip;    // a full Prism-format instance zip for the very first install
    QStringList managedDirs; // folders the pack owns; jars there that the pack doesn't list are removed
    QVector<PackFile> files;
    bool valid() const { return !version.isEmpty(); }
    static Manifest parse(const QJsonObject& o);
};

/**
 * The Pride updater (her ask 2026-10-01: "build some sort of updater … that can watch the website for an update to
 * the modpack … download the mod pack" — and later "build it into prism").
 *  check()  → downloads the manifest, emits checked()
 *  apply()  → downloads only the files whose SHA-1 differ (verified), swaps them in, removes jars the pack dropped,
 *             keeps the player's own config edits (a config is only replaced if it still matches what we shipped last
 *             time), records the installed version in <instance>/pride-pack.json.
 */
class PackUpdater : public QObject {
    Q_OBJECT
   public:
    explicit PackUpdater(QObject* parent = nullptr);

    void check();
    void apply(BaseInstance* instance, const Manifest& m);
    const Manifest& manifest() const { return m_manifest; }

    static QString installedVersion(BaseInstance* instance);
    /** -1 if a < b, 0 equal, 1 if a > b (dotted numbers, e.g. 1.10.2 > 1.9) */
    static int compareVersions(const QString& a, const QString& b);

   signals:
    void checked(bool ok, QString error);
    void progress(qint64 done, qint64 total);
    void status(QString text);
    void finished(bool ok, QString message);

   private:
    void finishApply(BaseInstance* instance);

    Manifest m_manifest;
    NetJob::Ptr m_job;
    QByteArray* m_manifestBytes = nullptr;
    QVector<QPair<QString, QString>> m_moves;  // temp file → final path
    QString m_stage;                           // temp folder for this update
};

}  // namespace Pride
