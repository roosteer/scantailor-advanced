// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_CONFIGPATHS_H_
#define SCANTAILOR_CORE_CONFIGPATHS_H_

#include <QString>

/**
 * Where this process keeps profiles and settings; set once, before any filter
 * or settings provider is built.
 */
class ConfigPaths {
 public:
  /**
   * Probes \p applicationDirPath for the portable config directory.
   *
   * \return The portable root, or an empty string when this build is not portable.
   */
  static QString resolvePortableRoot(const QString& applicationDirPath);

  /** Empty clears the portable root. */
  static void setPortableRoot(const QString& root);

  static bool isPortable();

  /** Portable root when set, else the standard per-user application data location. */
  static QString configRoot();

  static QString profilesDir();

  /** QSettings::IniFormat for both scopes, and the portable root as the UserScope path when set. */
  static void configureQSettings();
};

#endif  // ifndef SCANTAILOR_CORE_CONFIGPATHS_H_
