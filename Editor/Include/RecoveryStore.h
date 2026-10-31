#pragma once
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <memory>
#include <stdexcept>

// A separate record per running editor. The lock prevents offering a live
// session for recovery. Scene bytes are committed before their manifest.
class RecoveryStore {
public:
    QString directory, scenePath, record;
    std::unique_ptr<QLockFile> lock;
    QJsonObject identity;
    static QJsonObject Read(const QString& path) {
        QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    }
    void Open(const QString& project, const QString& runtime) {
        directory = QFileInfo(project).dir().filePath(".painengine/Recovery");
        if (!QDir().mkpath(directory)) throw std::runtime_error("Cannot create project recovery directory");
        record = QDir(directory).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
        lock = std::make_unique<QLockFile>(record + ".lock"); lock->setStaleLockTime(0);
        if (!lock->tryLock()) throw std::runtime_error("Cannot lock recovery record");
        const auto manifest = Read(project);
        scenePath = QFileInfo(project).dir().absoluteFilePath(manifest["startupScene"].toString());
        identity = {{"format",1},{"project",QFileInfo(project).absoluteFilePath()},
            {"engineVersion",Read(QDir(runtime).filePath("EngineVersion.json"))["version"]},
            {"module",manifest["module"]}};
        RefreshBuild();
    }
    void RefreshBuild() {
        const auto project=identity["project"].toString();
        const auto manifest=Read(project); identity["module"]=manifest["module"];
        QFile module(QFileInfo(project).dir().absoluteFilePath(manifest["module"].toString()));
        if (module.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(&module);
            identity["moduleSha256"] = QString::fromLatin1(hash.result().toHex());
        }
    }
    QString NextScene() const { return record + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".scene.json"; }
    void Commit(const QString& snapshot) {
        const auto previous = Read(record + ".json")["snapshot"].toString();
        auto metadata = identity;
        metadata["sourceScene"] = scenePath;
        metadata["snapshot"] = QFileInfo(snapshot).fileName();
        metadata["savedUtc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        QSaveFile f(record + ".json"); const auto bytes = QJsonDocument(metadata).toJson();
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
            throw std::runtime_error("Cannot commit recovery manifest");
        if (!previous.isEmpty() && QFileInfo(previous).fileName() == previous) QFile::remove(QDir(directory).filePath(previous));
    }
    void Clear() {
        if (record.isEmpty()) return;
        const auto previous = Read(record + ".json")["snapshot"].toString();
        QFile::remove(record + ".json");
        if (!previous.isEmpty() && QFileInfo(previous).fileName() == previous) QFile::remove(QDir(directory).filePath(previous));
    }
    void ForgetSnapshot(const QString& snapshot) {
        for (const auto& metadata : Candidates(identity["project"].toString())) {
            const auto info = Read(metadata);
            const auto old = QFileInfo(metadata).dir().filePath(info["snapshot"].toString());
            if (QFileInfo(old).absoluteFilePath() == QFileInfo(snapshot).absoluteFilePath()) {
                QFile::remove(metadata); QFile::remove(old);
            }
        }
    }
    static QStringList Candidates(const QString& project) {
        QDir dir(QFileInfo(project).dir().filePath(".painengine/Recovery")); QStringList result;
        for (const auto& file : dir.entryList({"*.json"}, QDir::Files, QDir::Time)) {
            if (file.endsWith(".scene.json")) continue;
            const auto path = dir.filePath(file); const auto data = Read(path);
            const auto snapshot = data["snapshot"].toString();
            if (data["format"].toInt() != 1 || snapshot.isEmpty() || QFileInfo(snapshot).fileName() != snapshot || !QFileInfo::exists(dir.filePath(snapshot))) continue;
            QLockFile held(path.left(path.size()-5) + ".lock"); held.setStaleLockTime(0);
            if (held.tryLock()) result.push_back(path);
        }
        return result;
    }
};
