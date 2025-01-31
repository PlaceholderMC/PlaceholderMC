#pragma once

#include <QFileInfo>
#include <QWidget>
#include <optional>

namespace GuiUtil {
std::optional<QString> uploadPaste(const QString& name, const QFileInfo& filePath, QWidget* parentWidget);
std::optional<QString> uploadPaste(const QString& name, const QString& data, QWidget* parentWidget);
void setClipboardText(QString text);
QStringList BrowseForFiles(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget);
QString BrowseForFile(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget);
}  // namespace GuiUtil
