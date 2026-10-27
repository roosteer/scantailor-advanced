// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "BatchReport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace cli {

int BatchReport::processed() const {
  int count = 0;
  for (const PageOutcome& page : pages) {
    if (page.ok) {
      ++count;
    }
  }
  return count;
}

int BatchReport::failed() const {
  return static_cast<int>(pages.size()) - processed();
}

QString renderJsonReport(const BatchReport& report) {
  QJsonObject json;

  if (report.failed() == 0) {
    json.insert("status", QStringLiteral("ok"));
  } else if (report.processed() > 0) {
    json.insert("status", QStringLiteral("partial"));
  } else {
    json.insert("status", QStringLiteral("error"));
  }
  json.insert("pages_processed", report.processed());
  json.insert("pages_failed", report.failed());

  QJsonArray outputFiles;
  QJsonArray errors;
  for (const PageOutcome& page : report.pages) {
    if (page.ok) {
      QJsonObject fileObj;
      fileObj.insert("page_id", page.index);
      fileObj.insert("input_file", page.inputFile);
      fileObj.insert("output_file", page.outputFile.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                              : QJsonValue(page.outputFile));
      outputFiles.append(fileObj);
    } else {
      QJsonObject errObj;
      errObj.insert("page_id", page.index);
      errObj.insert("input_file", page.inputFile);
      // The composite task chain throws without saying which stage failed;
      // --start-filter / --end-filter are the only stage facts we have.
      errObj.insert("filter", QJsonValue(QJsonValue::Null));
      errObj.insert("message", page.errorMessage);
      errors.append(errObj);
    }
  }
  json.insert("output_files", outputFiles);
  json.insert("errors", errors);

  return QJsonDocument(json).toJson(QJsonDocument::Indented) + QLatin1Char('\n');
}

ExitCode exitCodeFor(const BatchReport& report) {
  return (report.failed() > 0) ? ExitCode::FILTER_ERROR : ExitCode::SUCCESS;
}

}  // namespace cli
