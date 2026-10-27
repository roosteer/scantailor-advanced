// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ConfigPaths.h"

#include <config.h>

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

QString& portableRoot() {
  static QString root;
  return root;
}

}  // namespace

QString ConfigPaths::resolvePortableRoot(const QString& applicationDirPath) {
  const QString portableConfigDirName = QString::fromUtf8(PORTABLE_CONFIG_DIR);
  if (portableConfigDirName.isEmpty()) {
    return QString();
  }

  const QDir portableConfigPath(applicationDirPath + '/' + portableConfigDirName);
  // A directory we cannot create a temporary dir in is not writable, so it
  // cannot hold profiles either.
  if ((portableConfigPath.exists() && QTemporaryDir(portableConfigPath.absolutePath()).isValid())
      || (!portableConfigPath.exists() && portableConfigPath.mkpath("."))) {
    return portableConfigPath.absolutePath();
  }

  return QString();
}

void ConfigPaths::setPortableRoot(const QString& root) {
  portableRoot() = root.isEmpty() ? QString() : root;
}

bool ConfigPaths::isPortable() {
  return !portableRoot().isEmpty();
}

QString ConfigPaths::configRoot() {
  if (isPortable()) {
    return portableRoot();
  }
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString ConfigPaths::profilesDir() {
  return configRoot() + "/profiles";
}

void ConfigPaths::configureQSettings() {
  QSettings::setDefaultFormat(QSettings::IniFormat);
  if (isPortable()) {
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configRoot());
  }
}
