#pragma once

#include "data/Certification.h"
#include "globals/Globals.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>

namespace helper {

enum ButtonStyle
{
    ButtonPrimary,
    ButtonInfo,
    ButtonDanger,
    ButtonSuccess,
    ButtonWarning
};

QString toLatin1PercentEncoding(QString str);
QString urlDecode(QString str);
QString urlEncode(QString str);
QString formatTrailerUrl(QString url);
bool isDvd(QString path, bool noSubFolder = false);
bool isBluRay(QString path);
QImage& resizeBackdrop(QImage& image, bool& resized);
QByteArray& resizeBackdrop(QByteArray& image);
QString& sanitizeFileName(QString& fileName);
QString stackedBaseName(const QString& fileName);
QString appendArticle(const QString& text);
QString mapGenre(const QString& text);
QStringList mapGenre(const QStringList& genres);
Certification mapCertification(const Certification& certification);
QString mapStudio(const QString& text);
QString mapCountry(const QString& text);
QString formatFileSize(const qint64& size);
void removeFocusRect(QWidget* widget);
void applyStyle(QWidget* widget, bool removeFocus = true, bool isTable = false);
void applyEffect(QWidget* parent);
qreal similarity(const QString& s1, const QString& s2);
QMap<ColorLabel, QString> labels();
QColor colorForLabel(ColorLabel label);
QIcon iconForLabel(ColorLabel label);
qreal devicePixelRatio(QLabel* label);
qreal devicePixelRatio(QPushButton* button);
qreal devicePixelRatio(QWidget* widget);
qreal devicePixelRatio(const QPixmap& pixmap);
void setDevicePixelRatio(QPixmap& pixmap, qreal devicePixelRatio);
void setDevicePixelRatio(QImage& image, qreal devicePixelRatio);
int compareVersionNumbers(const QString& oldVersion, const QString& newVersion);
void setButtonStyle(QPushButton* button, ButtonStyle style);
void fillStereoModeCombo(QComboBox* box);
QMap<QString, QString> stereoModes();
QString matchResolution(int width, int height, const QString& scanType);
QImage getImage(QString path);
QString secondsToTimeCode(quint32 duration);

} // namespace helper
