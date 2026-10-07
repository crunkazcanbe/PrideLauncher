// SPDX-License-Identifier: GPL-3.0-only
#include "PackUpdater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "Application.h"
#include "BaseInstance.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"

namespace Pride {

QString manifestUrl()
{
    QString env = qEnvironmentVariable("PRIDE_MANIFEST_URL");
    return env.isEmpty() ? QStringLiteral("https://pride.example.com/pack/manifest.json") : env;
}

Manifest Manifest::parse(const QJsonObject& o)
{
    Manifest m;
    m.version = o.value("version").toString();
    m.released = o.value("released").toString();
    m.notes = o.value("notes").toString();
    m.instanceZip = o.value("instanceZip").toString();
    for (const auto& d : o.value("managedDirs").toArray())
        m.managedDirs << d.toString();
    if (m.managedDirs.isEmpty())
        m.managedDirs = { "mods" };
    for (const auto& v : o.value("files").toArray()) {
        auto f = v.toObject();
        PackFile p;
        p.path = QDir::cleanPath(f.value("path").toString());
        p.sha1 = f.value("sha1").toString().toLower();
        p.size = static_cast<qint64>(f.value("size").toDouble());
        p.url = f.value("url").toString();
        // never let a manifest write outside the game folder
        if (p.path.isEmpty() || p.path.startsWith("..") || QDir::isAbsolutePath(p.path) || p.sha1.size() != 40 || p.url.isEmpty())
            continue;
        m.files.push_back(p);
    }
    return m;
}

PackUpdater::PackUpdater(QObject* parent) : QObject(parent) {}

static QString sha1Of(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}

static QJsonObject readState(BaseInstance* instance)
{
    QFile f(QDir(instance->instanceRoot()).filePath("pride-pack.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString PackUpdater::installedVersion(BaseInstance* instance)
{
    return instance ? readState(instance).value("version").toString() : QString();
}

int PackUpdater::compareVersions(const QString& a, const QString& b)
{
    auto pa = a.split('.'), pb = b.split('.');
    for (int i = 0; i < std::max(pa.size(), pb.size()); i++) {
        int x = i < pa.size() ? pa[i].toInt() : 0, y = i < pb.size() ? pb[i].toInt() : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

void PackUpdater::check()
{
    auto [dl, bytes] = Net::Download::makeByteArray(QUrl(manifestUrl()));
    m_manifestBytes = bytes;
    m_job.reset(new NetJob("Pride pack check", APPLICATION->network()));
    m_job->addNetAction(dl);
    connect(m_job.get(), &Task::succeeded, this, [this] {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(*m_manifestBytes, &err);
        m_manifest = Manifest::parse(doc.object());
        if (err.error != QJsonParseError::NoError || !m_manifest.valid())
            emit checked(false, tr("The Pride website sent a pack list we couldn't read."));
        else
            emit checked(true, {});
    });
    connect(m_job.get(), &Task::failed, this, [this](QString reason) {
        emit checked(false, tr("Couldn't reach the Pride website (%1).").arg(reason));
    });
    m_job->start();
}

void PackUpdater::apply(BaseInstance* instance, const Manifest& m)
{
    m_manifest = m;
    m_moves.clear();
    QDir game(instance->gameRoot());
    QJsonObject shipped = readState(instance).value("files").toObject();  // what we installed last time
    m_stage = QDir(instance->instanceRoot()).filePath(".pride-update");
    QDir(m_stage).removeRecursively();
    QDir().mkpath(m_stage);

    m_job.reset(new NetJob("Pride pack update", APPLICATION->network(), 6));
    qint64 bytes = 0;
    int n = 0;
    for (const auto& f : m.files) {
        QString target = game.filePath(f.path);
        QString local = QFileInfo::exists(target) ? sha1Of(target) : QString();
        if (local == f.sha1)
            continue;
        bool isConfig = f.path.startsWith("config/");
        // the player changed this config themselves → keep theirs (unless it never existed)
        if (isConfig && !local.isEmpty() && shipped.contains(f.path) && shipped.value(f.path).toString() != local)
            continue;
        QString tmp = QDir(m_stage).filePath(QString::number(n++) + "_" + QFileInfo(f.path).fileName());
        auto dl = Net::Download::makeFile(QUrl(f.url), tmp);
        dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, f.sha1));
        m_job->addNetAction(dl);
        m_moves.push_back({ tmp, target });
        bytes += f.size;
    }
    emit status(m_moves.isEmpty() ? tr("Checking files…") : tr("Downloading %1 files (%2 MB)…").arg(m_moves.size()).arg(bytes / 1048576));
    if (m_moves.isEmpty()) {
        finishApply(instance);
        return;
    }
    connect(m_job.get(), &Task::progress, this, &PackUpdater::progress);
    connect(m_job.get(), &Task::succeeded, this, [this, instance] { finishApply(instance); });
    connect(m_job.get(), &Task::failed, this, [this](QString reason) {
        QDir(m_stage).removeRecursively();
        emit finished(false, tr("The update didn't finish (%1). Nothing was changed — try again.").arg(reason));
    });
    m_job->start();
}

void PackUpdater::finishApply(BaseInstance* instance)
{
    QDir game(instance->gameRoot());
    // 1. swap the verified downloads in
    for (const auto& mv : m_moves) {
        QDir().mkpath(QFileInfo(mv.second).absolutePath());
        QFile::remove(mv.second);
        if (!QFile::rename(mv.first, mv.second) && !QFile::copy(mv.first, mv.second)) {
            emit finished(false, tr("Couldn't write %1 — is the game still running?").arg(QFileInfo(mv.second).fileName()));
            return;
        }
    }
    // 2. jars in pack-owned folders that the pack no longer lists are switched off (renamed, never deleted)
    QSet<QString> wanted;
    for (const auto& f : m_manifest.files)
        wanted.insert(QDir::cleanPath(game.filePath(f.path)));
    int removed = 0;
    for (const auto& dir : m_manifest.managedDirs) {
        QDirIterator it(game.filePath(dir), { "*.jar", "*.zip" }, QDir::Files);
        while (it.hasNext()) {
            QString p = QDir::cleanPath(it.next());
            if (!wanted.contains(p) && QFile::rename(p, p + ".disabled-by-pride-update"))
                removed++;
        }
    }
    // 3. remember what we shipped (so the player's own config edits survive the next update)
    QJsonObject files;
    for (const auto& f : m_manifest.files)
        files.insert(f.path, f.sha1);
    QJsonObject state{ { "version", m_manifest.version }, { "released", m_manifest.released }, { "files", files } };
    QSaveFile out(QDir(instance->instanceRoot()).filePath("pride-pack.json"));
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(state).toJson());
        out.commit();
    }
    QDir(m_stage).removeRecursively();
    emit finished(true, removed > 0 ? tr("Pride %1 is ready (%2 old files switched off).").arg(m_manifest.version).arg(removed)
                                    : tr("Pride %1 is ready.").arg(m_manifest.version));
}

}  // namespace Pride
