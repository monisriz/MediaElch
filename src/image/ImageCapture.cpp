#include "ImageCapture.h"

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QTemporaryFile>

#include "globals/Helper.h"
#include "ui/notifications/NotificationBox.h"

namespace mediaelch {

ImageCapture::ImageCapture(QObject* parent) : QObject(parent)
{
}

bool ImageCapture::captureImage(QString file, StreamDetails* streamDetails, ThumbnailDimensions dim, QImage& img)
{
    if (streamDetails->videoDetails().value(StreamDetails::VideoDetails::DurationInSeconds, nullptr) == nullptr) {
        streamDetails->loadStreamDetails();
    }
    if (streamDetails->videoDetails().value(StreamDetails::VideoDetails::DurationInSeconds, nullptr) == nullptr) {
        NotificationBox::instance()->showMessage(
            tr("Could not get duration of file"), NotificationBox::NotificationError);
        return false;
    }

#ifdef Q_OS_OSX
    QString ffmpegbin = QCoreApplication::applicationDirPath() + "/ffmpeg";
#elif defined(Q_OS_WIN)
    QString ffmpegbin = QCoreApplication::applicationDirPath() + "/vendor/ffmpeg.exe";
#else
    QString ffmpegbin = "ffmpeg";
#endif

    QProcess ffmpeg;
    qsrand(QTime::currentTime().msec());

    unsigned duration =
        streamDetails->videoDetails().value(StreamDetails::VideoDetails::DurationInSeconds, nullptr).toUInt();

    if (duration == 0) {
        NotificationBox::instance()->showMessage(
            tr("Could not detect runtime of file"), NotificationBox::NotificationError);
        return false;
    }
    unsigned t = static_cast<unsigned>(qrand()) % duration;
    QString timeCode = helper::secondsToTimeCode(t);

    QTemporaryFile tmpFile;
    if (!tmpFile.open()) {
        NotificationBox::instance()->showMessage(
            tr("Temporary output file could not be opened"), NotificationBox::NotificationError);
        return false;
    }
    tmpFile.close();

    ffmpeg.start(ffmpegbin,
        QStringList() << "-y"
                      << "-ss" << timeCode << "-i" << file << "-vframes"
                      << "1"
                      << "-q:v"
                      << "2"
                      << "-f"
                      << "mjpeg" << tmpFile.fileName());
    if (!ffmpeg.waitForStarted()) {
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
        NotificationBox::instance()->showMessage(tr("Could not start ffmpeg"), NotificationBox::NotificationError);
#else
        NotificationBox::instance()->showMessage(
            tr("Could not start ffmpeg. Please install it and make it available in your $PATH"),
            NotificationBox::NotificationError);
#endif
        return false;
    }

    if (!ffmpeg.waitForFinished(10000)) {
        NotificationBox::instance()->showMessage(tr("ffmpeg did not finish"), NotificationBox::NotificationError);
        return false;
    }
    if (!tmpFile.open()) {
        NotificationBox::instance()->showMessage(
            tr("Temporary output file could not be opened"), NotificationBox::NotificationError);
        return false;
    }

    img = QImage::fromData(tmpFile.readAll());
    tmpFile.close();

    img = img.scaled(dim.width, dim.height, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    return true;
}

} // namespace mediaelch
