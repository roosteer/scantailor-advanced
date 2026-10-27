// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <QByteArray>
#include <QTextStream>
#include <QtGlobal>

#include <exception>
#include <memory>
#include <optional>

#include "Application.h"
#include "BatchReport.h"
#include "CliParser.h"
#include "CliRequest.h"
#include "CliUsage.h"
#include "ConsoleLog.h"
#include "ExitCode.h"
#include "FilterRange.h"
#include "PipelineSession.h"
#include "StageSequence.h"

using namespace cli;

int main(int argc, char* argv[]) {
  // This program never opens a window, so ask Qt for the platform plugin that
  // needs no display. An explicit QT_QPA_PLATFORM from the environment wins.
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }

  // Core services (DefaultParamsProfileManager) cast qApp to Application,
  // so the CLI must construct that subclass rather than QApplication.
  Application app(argc, argv);
  QApplication::setApplicationName("scantailor-advanced-cli");

  try {
    // argv[0] is the program name.
    const ParseResult parsed = parseCommandLine(QApplication::arguments().mid(1));
    if (!parsed.error.ok()) {
      ConsoleLog::writeError(parsed.error.message);
      QTextStream(stderr) << "Use --help for usage information.\n";
      return static_cast<int>(parsed.error.code);
    }

    const CliRequest& request = parsed.request;
    if (request.help) {
      printHelp();
      return static_cast<int>(ExitCode::SUCCESS);
    }
    if (request.version) {
      printVersion();
      return static_cast<int>(ExitCode::SUCCESS);
    }

    const ConsoleLog log(request);

    CliError error;
    const std::unique_ptr<PipelineSession> session = PipelineSession::open(request, error);
    if (!session) {
      ConsoleLog::writeError(error.message);
      return static_cast<int>(error.code);
    }

    const std::optional<FilterRange> range = resolveFilterRange(request, session->stages()->count(), error);
    if (!range) {
      ConsoleLog::writeError(error.message);
      return static_cast<int>(error.code);
    }

    session->configure(*range);
    const BatchReport report = session->process(*range, log);

    if (!request.outputProjectFile.isEmpty() && report.failed() == 0) {
      if (!session->saveProject(request.outputProjectFile, error)) {
        log.error("output", error.message);
        return static_cast<int>(error.code);
      }
    }

    if (request.outputFormat == OutputFormat::JSON) {
      QTextStream(stdout) << renderJsonReport(report);
    }

    log.progress("process",
                 QString("Complete. %1 succeeded, %2 failed.").arg(report.processed()).arg(report.failed()));
    return static_cast<int>(exitCodeFor(report));
  } catch (const std::exception& e) {
    ConsoleLog::writeError(QString::fromUtf8(e.what()));
    return static_cast<int>(ExitCode::INTERNAL_ERROR);
  } catch (...) {
    ConsoleLog::writeError(QStringLiteral("Unhandled exception"));
    return static_cast<int>(ExitCode::INTERNAL_ERROR);
  }
}
