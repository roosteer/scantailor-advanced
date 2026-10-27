// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CliParser.h"

#include "CliUsage.h"

namespace cli {

namespace {

ExitCode parseFlag(const QString& arg, CliRequest& request, QString& errorMessage) {
  const int eqIdx = arg.indexOf('=');
  const QString name = (eqIdx >= 0) ? arg.left(eqIdx) : arg;
  const bool hasValue = (eqIdx >= 0);
  const QString value = hasValue ? arg.mid(eqIdx + 1) : QString();

  const FlagSpec* spec = findFlagSpec(name);
  if (spec == nullptr) {
    errorMessage = QString("Unknown flag: %1").arg(name);
    return ExitCode::INVALID_INPUT;
  }

  if (spec->valueHint == nullptr) {
    if (hasValue) {
      errorMessage = QString("%1 takes no value").arg(name);
      return ExitCode::INVALID_INPUT;
    }
  } else if (!hasValue) {
    errorMessage = QString("%1 requires a value (e.g. %1=%2)")
                       .arg(name, QString::fromLatin1(spec->valueHint));
    return ExitCode::INVALID_INPUT;
  }

  return spec->handler(value, request, errorMessage);
}

ExitCode resolvePositionals(QStringList positionals, CliRequest& request, QString& errorMessage) {
  if (request.help || request.version) {
    return ExitCode::SUCCESS;
  }

  if (positionals.isEmpty()) {
    errorMessage = QStringLiteral("No input files specified. Use --help for usage.");
    return ExitCode::INVALID_INPUT;
  }

  const bool firstIsProject = positionals[0].endsWith(".ScanTailor", Qt::CaseInsensitive);

  if (positionals.size() == 1 && firstIsProject) {
    // Project mode; the output directory comes from the project file.
    request.projectFile = positionals[0];
    return ExitCode::SUCCESS;
  }

  if (positionals.size() >= 2 && firstIsProject) {
    if (positionals.size() > 2) {
      errorMessage = QString("Unexpected arguments after output directory in project mode: %1")
                         .arg(positionals[2]);
      return ExitCode::INVALID_INPUT;
    }
    request.projectFile = positionals[0];
    request.outputDir = positionals[1];
    return ExitCode::SUCCESS;
  }

  if (positionals.size() >= 2) {
    request.outputDir = positionals.takeLast();
    request.images = positionals;
    return ExitCode::SUCCESS;
  }

  errorMessage =
      QStringLiteral("Expected images and output directory, or a .ScanTailor project file. Use --help for usage.");
  return ExitCode::INVALID_INPUT;
}

ExitCode checkCombinations(const CliRequest& request, QString& errorMessage) {
  if (request.quiet && request.verbose) {
    errorMessage = QStringLiteral("--quiet and --verbose are mutually exclusive");
    return ExitCode::INVALID_INPUT;
  }
  if (request.contentDetection == ContentDetection::MANUAL && !request.contentBox) {
    errorMessage = QStringLiteral("--content-detection=manual requires --content-box=l,t,r,b");
    return ExitCode::INVALID_INPUT;
  }
  if (request.deskewAngle && request.deskewMode != DeskewMode::MANUAL) {
    errorMessage = QStringLiteral("--deskew-angle requires --deskew=manual");
    return ExitCode::INVALID_INPUT;
  }
  return ExitCode::SUCCESS;
}

}  // namespace

ParseResult parseCommandLine(const QStringList& args) {
  ParseResult result;
  CliRequest& request = result.request;

  QStringList positionals;

  for (const QString& arg : args) {
    if (arg.startsWith('-') && arg != QLatin1String("-")) {
      QString errorMessage;
      const ExitCode code = parseFlag(arg, request, errorMessage);
      if (code != ExitCode::SUCCESS) {
        result.error = {code, errorMessage};
        return result;
      }
    } else {
      positionals << arg;
    }
  }

  QString errorMessage;
  ExitCode code = resolvePositionals(positionals, request, errorMessage);
  if (code == ExitCode::SUCCESS) {
    code = checkCombinations(request, errorMessage);
  }
  if (code != ExitCode::SUCCESS) {
    result.error = {code, errorMessage};
  }
  return result;
}

}  // namespace cli
