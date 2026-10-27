// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ConsoleLog.h"

#include <QDateTime>
#include <QTextStream>

namespace cli {

namespace {

void write(const QString& operation, const QString& message) {
  QTextStream err(stderr);
  err << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << " [" << operation << "]: " << message << "\n";
}

}  // namespace

ConsoleLog::ConsoleLog(const CliRequest& request) : m_quiet(request.quiet), m_verbose(request.verbose) {}

void ConsoleLog::progress(const QString& operation, const QString& message) const {
  if (!m_quiet) {
    write(operation, message);
  }
}

void ConsoleLog::detail(const QString& operation, const QString& message) const {
  if (m_verbose && !m_quiet) {
    write(operation, message);
  }
}

void ConsoleLog::error(const QString& operation, const QString& message) const {
  write(operation, message);
}

void ConsoleLog::writeError(const QString& message) {
  QTextStream err(stderr);
  err << "Error: " << message << "\n";
}

}  // namespace cli
