#include "ConcertFileSearcher.h"

#include <QApplication>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlRecord>

#include "globals/Helper.h"
#include "globals/Manager.h"
#include "globals/MessageIds.h"

ConcertFileSearcher::ConcertFileSearcher(QObject* parent) :
    QObject(parent),
    m_progressMessageId{Constants::ConcertFileSearcherProgressMessageId}
{
}

void ConcertFileSearcher::setConcertDirectories(QVector<SettingsDir> directories)
{
    m_directories.clear();

    for (const auto& dir : directories) {
        if (dir.path.isReadable()) {
            qDebug() << "Adding concert directory" << dir.path.path();
            m_directories.append(dir);
        } else {
            qDebug() << "Concert directory is not redable, skipping:" << dir.path.path();
        }
    }
}

/// Starts the scanning process
///
///  1. Clear old concert entries if a reload is either forced here or in its settings
///  2. Reload all entries from disk if it's forced here or in its directory settings
///  3. Load all entries from the database
void ConcertFileSearcher::reload(bool force)
{
    m_aborted = false;

    clearOldConcerts(force);

    emit searchStarted(tr("Searching for Concerts..."));

    auto contents = loadContentsFromDiskIfRequired(force);
    storeContentsInDatabase(contents);

    emit currentDir("");
    emit searchStarted(tr("Loading Concerts..."));

    addConcertsToGui(loadConcertsFromDatabase());

    qDebug() << "Searching for concerts done";
    if (!m_aborted) {
        emit concertsLoaded();
    }
}

/**
 * @brief Scans the given path for concert files.
 * Results are in a list which contains a QStringList for every concert.
 * @param startPath Scanning started at this path
 * @param path Path to scan
 * @param contents List of contents
 * @param separateFolders Are concerts in separate folders
 * @param firstScan When this is true, subfolders are scanned, regardless of separateFolders
 */
void ConcertFileSearcher::scanDir(QString startPath,
    QString path,
    QVector<QStringList>& contents,
    bool separateFolders,
    bool firstScan)
{
    emit currentDir(path.mid(startPath.length()));

    QDir dir(path);
    for (const QString& cDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (m_aborted) {
            return;
        }

        // Skip "Extras" folder
        if (QString::compare(cDir, "Extras", Qt::CaseInsensitive) == 0
            || QString::compare(cDir, ".actors", Qt::CaseInsensitive) == 0
            || QString::compare(cDir, "extrafanarts", Qt::CaseInsensitive) == 0) {
            continue;
        }

        // Handle DVD
        if (helper::isDvd(path + QDir::separator() + cDir)) {
            contents.append(QStringList() << QDir::toNativeSeparators(path + "/" + cDir + "/VIDEO_TS/VIDEO_TS.IFO"));
            continue;
        }

        // Handle BluRay
        if (helper::isBluRay(path + QDir::separator() + cDir)) {
            contents.append(QStringList() << QDir::toNativeSeparators(path + "/" + cDir + "/BDMV/index.bdmv"));
            continue;
        }

        // Don't scan subfolders when separate folders is checked
        if (!separateFolders || firstScan) {
            scanDir(startPath, path + "/" + cDir, contents, separateFolders);
        }
    }

    QStringList files;
    QStringList entries = getFiles(path);
    for (const QString& file : entries) {
        if (m_aborted) {
            return;
        }

        // Skip Trailers and Sample files
        if (file.contains("-trailer", Qt::CaseInsensitive) || file.contains("-sample", Qt::CaseInsensitive)) {
            continue;
        }
        files.append(file);
    }
    files.sort();

    if (separateFolders) {
        QStringList concertFiles;
        for (const QString& file : files) {
            concertFiles.append(QDir::toNativeSeparators(path + "/" + file));
        }
        if (concertFiles.count() > 0) {
            contents.append(concertFiles);
        }
        return;
    }

    QRegExp rx("((part|cd)[\\s_]*)(\\d+)", Qt::CaseInsensitive);
    for (int i = 0, n = files.size(); i < n; i++) {
        if (m_aborted) {
            return;
        }

        QStringList concertFiles;
        QString file = files.at(i);
        if (file.isEmpty()) {
            continue;
        }

        concertFiles << QDir::toNativeSeparators(path + QDir::separator() + file);

        int pos = rx.indexIn(file);
        if (pos != -1) {
            QString left = file.left(pos) + rx.cap(1);
            QString right = file.mid(pos + rx.cap(1).size() + rx.cap(2).size());
            for (int x = 0; x < n; x++) {
                QString subFile = files.at(x);
                if (subFile != file) {
                    if (subFile.startsWith(left) && subFile.endsWith(right)) {
                        concertFiles << QDir::toNativeSeparators(path + QDir::separator() + subFile);
                        files[x] = ""; // set an empty file name, this way we can skip this file in the main loop
                    }
                }
            }
        }
        if (concertFiles.count() > 0) {
            contents.append(concertFiles);
        }
    }
}

void ConcertFileSearcher::clearOldConcerts(bool forceClear)
{
    if (forceClear) {
        database().clearConcerts();
    }

    // clear gui
    Manager::instance()->concertModel()->clear();

    for (const SettingsDir& dir : m_directories) {
        if (dir.autoReload || forceClear) {
            database().clearConcerts(dir.path.path());
        }
    }
}

QVector<QStringList> ConcertFileSearcher::loadContentsFromDiskIfRequired(bool forceReload)
{
    QVector<QStringList> contents;

    for (const SettingsDir& dir : m_directories) {
        QString path = dir.path.path();
        QVector<Concert*> concertsFromDb = database().concerts(path);
        if (dir.autoReload || forceReload || concertsFromDb.isEmpty()) {
            scanDir(path, path, contents, dir.separateFolders, true);
        }
    }
    return contents;
}

void ConcertFileSearcher::storeContentsInDatabase(const QVector<QStringList>& contents)
{
    // Setup concerts
    database().transaction();
    for (const QStringList& files : contents) {
        if (m_aborted) {
            return;
        }

        bool inSeparateFolder = false;
        QString path;
        // get directory
        if (!files.isEmpty()) {
            int index = -1;
            for (int i = 0, n = m_directories.count(); i < n; ++i) {
                if (files.at(0).startsWith(m_directories[i].path.path())) {
                    if (index == -1) {
                        index = i;
                    } else if (m_directories[index].path.path().length() < m_directories[i].path.path().length()) {
                        index = i;
                    }
                }
            }
            if (index != -1) {
                inSeparateFolder = m_directories[index].separateFolders;
                path = m_directories[index].path.path();
            }
        }
        Concert concert(files, this);
        concert.setInSeparateFolder(inSeparateFolder);
        concert.controller()->loadData(Manager::instance()->mediaCenterInterface());
        emit currentDir(concert.name());
        database().add(&concert, path);
    }
    database().commit();
}

void ConcertFileSearcher::setupDatabaseConcerts(QVector<Concert*>& dbConcerts)
{
    int concertCounter = 0;
    for (Concert* concert : dbConcerts) {
        if (m_aborted) {
            break;
        }
        concert->controller()->loadData(Manager::instance()->mediaCenterInterface(), false, false);
        emit currentDir(concert->name());
        emit progress(++concertCounter, dbConcerts.size(), m_progressMessageId);
    }
}

QVector<Concert*> ConcertFileSearcher::loadConcertsFromDatabase()
{
    QVector<Concert*> dbConcerts;
    for (const SettingsDir& dir : m_directories) {
        if (m_aborted) {
            break;
        }
        dbConcerts.append(database().concerts(dir.path.path()));
    }
    setupDatabaseConcerts(dbConcerts);
    return dbConcerts;
}

void ConcertFileSearcher::addConcertsToGui(const QVector<Concert*>& concerts)
{
    // add all entries from database including the previously stored ones
    for (Concert* concert : concerts) {
        Manager::instance()->concertModel()->addConcert(concert);
    }
}

/// Get a list of files in a directory
QStringList ConcertFileSearcher::getFiles(QString path)
{
    const auto& fileFilter = Settings::instance()->advanced()->concertFilters();
    return fileFilter.files(QDir(path));
}

void ConcertFileSearcher::abort()
{
    m_aborted = true;
}

Database& ConcertFileSearcher::database()
{
    return *Manager::instance()->database();
}
