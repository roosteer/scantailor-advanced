// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_cli_qt.h"

#include "BatchReport.h"
#include "CliParser.h"
#include "CliRequest.h"
#include "CliUsage.h"
#include "ExitCode.h"
#include "FilterRange.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QtTest/QtTest>

namespace {

int codeOf(const cli::ParseResult& result) {
  return static_cast<int>(result.error.code);
}

int codeOf(cli::ExitCode code) {
  return static_cast<int>(code);
}

}  // namespace

void TestCliQt::parsesDefaults() {
  const cli::ParseResult result = cli::parseCommandLine({"--output-dpi=300", "a.png", "/tmp/out"});
  QVERIFY(result.error.ok());

  const cli::CliRequest& request = result.request;
  QCOMPARE(request.images, QStringList{"a.png"});
  QCOMPARE(request.outputDir, QStringLiteral("/tmp/out"));
  QVERIFY(request.projectFile.isEmpty());
  QVERIFY(request.outputProjectFile.isEmpty());
  QCOMPARE(static_cast<int>(request.outputFormat), static_cast<int>(cli::OutputFormat::TEXT));
  QCOMPARE(request.outputDpi.value_or(-1), 300);

  QVERIFY(!request.startFilter.has_value());
  QVERIFY(!request.endFilter.has_value());
  QVERIFY(!request.orientation.has_value());
  QVERIFY(!request.layoutType.has_value());
  QVERIFY(!request.deskewMode.has_value());
  QVERIFY(!request.deskewAngle.has_value());
  QVERIFY(!request.contentDetection.has_value());
  QVERIFY(!request.contentBox.has_value());
  QVERIFY(!request.margins.has_value());
  QVERIFY(!request.alignment.has_value());
  QVERIFY(!request.colorMode.has_value());
  QVERIFY(!request.thresholdAdjustment.has_value());
  QVERIFY(!request.despeckleLevel.has_value());
  QVERIFY(!request.dewarpingMode.has_value());
  QVERIFY(!request.depthPerception.has_value());
  QVERIFY(!request.equalizeIllumination);
  QVERIFY(!request.verbose);
  QVERIFY(!request.quiet);
  QVERIFY(!request.help);
  QVERIFY(!request.version);
}

void TestCliQt::parsesValueFlags() {
  const cli::ParseResult result = cli::parseCommandLine(
      {"--margins=1,2,3,4", "--color-mode=mixed", "--deskew=manual", "--deskew-angle=1.5", "a.png", "/tmp/out"});
  QVERIFY(result.error.ok());

  const cli::CliRequest& request = result.request;
  QVERIFY(request.margins.has_value());
  QCOMPARE(request.margins->left, 1.0);
  QCOMPARE(request.margins->top, 2.0);
  QCOMPARE(request.margins->right, 3.0);
  QCOMPARE(request.margins->bottom, 4.0);
  QVERIFY(request.colorMode.has_value());
  QCOMPARE(static_cast<int>(*request.colorMode), static_cast<int>(output::MIXED));
  QVERIFY(request.deskewMode.has_value());
  QCOMPARE(static_cast<int>(*request.deskewMode), static_cast<int>(cli::DeskewMode::MANUAL));
  QCOMPARE(request.deskewAngle.value_or(0.0), 1.5);
}

void TestCliQt::acceptsLegacyColorModeAliases() {
  const cli::ParseResult bw = cli::parseCommandLine({"--color-mode=black_and_white", "a.png", "/o"});
  QVERIFY(bw.error.ok());
  QVERIFY(bw.request.colorMode.has_value());
  QCOMPARE(static_cast<int>(*bw.request.colorMode), static_cast<int>(output::BLACK_AND_WHITE));

  const cli::ParseResult color = cli::parseCommandLine({"--color-mode=color_grayscale", "a.png", "/o"});
  QVERIFY(color.error.ok());
  QVERIFY(color.request.colorMode.has_value());
  QCOMPARE(static_cast<int>(*color.request.colorMode), static_cast<int>(output::COLOR_GRAYSCALE));
}

void TestCliQt::rejectsUnknownFlag() {
  const cli::ParseResult result = cli::parseCommandLine({"--foo=bar"});
  QVERIFY(!result.error.ok());
  QCOMPARE(codeOf(result), codeOf(cli::ExitCode::INVALID_INPUT));
  QVERIFY2(result.error.message.contains("--foo"), qPrintable(result.error.message));
}

void TestCliQt::rejectsMalformedValues() {
  const QList<QStringList> cases = {
      {"--output-dpi"},
      {"--verbose=1"},
      {"--layout=7"},
      {"--margins=1,2,3"},
      {"--output=yaml"},
      {"--start-filter=6"},
  };

  for (const QStringList& args : cases) {
    const cli::ParseResult result = cli::parseCommandLine(args);
    QVERIFY2(!result.error.ok(), qPrintable(args.join(' ')));
    QVERIFY2(codeOf(result) == codeOf(cli::ExitCode::INVALID_INPUT), qPrintable(args.join(' ')));
  }
}

void TestCliQt::rejectsConflictingOptions() {
  const QList<QStringList> cases = {
      {"--quiet", "--verbose", "a.png", "/tmp/out"},
      {"--content-detection=manual", "a.png", "/tmp/out"},
      {"--deskew-angle=1.5", "a.png", "/tmp/out"},
      {"--deskew=auto", "--deskew-angle=1.5", "a.png", "/tmp/out"},
  };

  for (const QStringList& args : cases) {
    const cli::ParseResult result = cli::parseCommandLine(args);
    QVERIFY2(!result.error.ok(), qPrintable(args.join(' ')));
    QVERIFY2(codeOf(result) == codeOf(cli::ExitCode::INVALID_INPUT), qPrintable(args.join(' ')));
  }
}

void TestCliQt::resolvesPositionals() {
  {
    const cli::ParseResult result = cli::parseCommandLine({"p.ScanTailor"});
    QVERIFY(result.error.ok());
    QCOMPARE(result.request.projectFile, QStringLiteral("p.ScanTailor"));
    QVERIFY(result.request.outputDir.isEmpty());
  }
  {
    const cli::ParseResult result = cli::parseCommandLine({"p.ScanTailor", "/o"});
    QVERIFY(result.error.ok());
    QCOMPARE(result.request.projectFile, QStringLiteral("p.ScanTailor"));
    QCOMPARE(result.request.outputDir, QStringLiteral("/o"));
  }
  {
    const cli::ParseResult result = cli::parseCommandLine({"p.ScanTailor", "/o", "x"});
    QVERIFY(!result.error.ok());
    QCOMPARE(codeOf(result), codeOf(cli::ExitCode::INVALID_INPUT));
  }
  {
    const cli::ParseResult result = cli::parseCommandLine({"a.png", "b.png", "/o"});
    QVERIFY(result.error.ok());
    QCOMPARE(result.request.images, QStringList({"a.png", "b.png"}));
    QCOMPARE(result.request.outputDir, QStringLiteral("/o"));
    QVERIFY(result.request.projectFile.isEmpty());
  }
  {
    const cli::ParseResult result = cli::parseCommandLine({"a.png"});
    QVERIFY(!result.error.ok());
    QCOMPARE(codeOf(result), codeOf(cli::ExitCode::INVALID_INPUT));
  }
}

void TestCliQt::flagTableIsWellFormed() {
  QSet<QString> descriptions;
  for (const cli::FlagSpec& spec : cli::flagSpecs()) {
    const QString name = QString::fromLatin1(spec.name);
    const QString description = QString::fromLatin1(spec.description);
    const QString group = QString::fromLatin1(spec.group);

    QVERIFY2(name.startsWith("--"), qPrintable(name));
    QVERIFY2(!description.isEmpty(), qPrintable(name));
    QVERIFY2(!group.isEmpty(), qPrintable(name));
    QVERIFY2(!description.contains('\n'), qPrintable(name));
    QVERIFY2(spec.handler != nullptr, qPrintable(name));
    QCOMPARE(cli::findFlagSpec(name), &spec);
    QVERIFY2(!descriptions.contains(description), qPrintable(description));
    descriptions.insert(description);
  }

  QCOMPARE(cli::findFlagSpec(QStringLiteral("--white-margins")), nullptr);
  QCOMPARE(cli::findFlagSpec(QStringLiteral("--not-a-flag")), nullptr);
}

void TestCliQt::resolvesFilterRange() {
  {
    cli::CliRequest request;
    request.endFilter = 4;
    cli::CliError error;
    const std::optional<cli::FilterRange> range = cli::resolveFilterRange(request, cli::kFilterCount, error);
    QVERIFY(range.has_value());
    QCOMPARE(range->start, 0);
    QCOMPARE(range->end, 4);
    QVERIFY(error.ok());
  }
  {
    cli::CliRequest request;
    request.startFilter = 2;
    cli::CliError error;
    const std::optional<cli::FilterRange> range = cli::resolveFilterRange(request, cli::kFilterCount, error);
    QVERIFY(range.has_value());
    QCOMPARE(range->start, 2);
    QCOMPARE(range->end, cli::kFilterCount - 1);
  }
  {
    cli::CliRequest request;
    request.startFilter = 4;
    request.endFilter = 3;
    cli::CliError error;
    QVERIFY(!cli::resolveFilterRange(request, cli::kFilterCount, error).has_value());
    QCOMPARE(static_cast<int>(error.code), static_cast<int>(cli::ExitCode::INVALID_INPUT));
  }
  {
    cli::CliRequest request;
    request.endFilter = 9;
    cli::CliError error;
    QVERIFY(!cli::resolveFilterRange(request, cli::kFilterCount, error).has_value());
    QCOMPARE(static_cast<int>(error.code), static_cast<int>(cli::ExitCode::INVALID_INPUT));
  }
}

void TestCliQt::rendersReport() {
  cli::PageOutcome okPage;
  okPage.index = 0;
  okPage.inputFile = QStringLiteral("/i/a.png");
  okPage.outputFile = QStringLiteral("/o/a.tif");
  okPage.ok = true;

  cli::PageOutcome failedPage;
  failedPage.index = 1;
  failedPage.inputFile = QStringLiteral("/i/b.png");
  failedPage.errorMessage = QStringLiteral("boom");

  cli::BatchReport report;
  report.pages = {okPage, failedPage};

  const QJsonDocument doc = QJsonDocument::fromJson(cli::renderJsonReport(report).toUtf8());
  QVERIFY(doc.isObject());
  const QJsonObject json = doc.object();

  QCOMPARE(json.value("status").toString(), QStringLiteral("partial"));
  QCOMPARE(json.value("pages_processed").toInt(), 1);
  QCOMPARE(json.value("pages_failed").toInt(), 1);

  const QJsonArray outputFiles = json.value("output_files").toArray();
  QCOMPARE(outputFiles.size(), 1);
  QVERIFY(outputFiles[0].toObject().value("page_id").isDouble());
  QCOMPARE(outputFiles[0].toObject().value("page_id").toInt(), 0);
  QCOMPARE(outputFiles[0].toObject().value("input_file").toString(), QStringLiteral("/i/a.png"));
  QCOMPARE(outputFiles[0].toObject().value("output_file").toString(), QStringLiteral("/o/a.tif"));

  const QJsonArray errors = json.value("errors").toArray();
  QCOMPARE(errors.size(), 1);
  QVERIFY(errors[0].toObject().value("filter").isNull());
  QCOMPARE(errors[0].toObject().value("page_id").toInt(), 1);
  QCOMPARE(errors[0].toObject().value("message").toString(), QStringLiteral("boom"));

  // A page whose output stage never ran has no file to report.
  cli::PageOutcome skippedOutput;
  skippedOutput.index = 0;
  skippedOutput.inputFile = QStringLiteral("/i/c.png");
  skippedOutput.ok = true;

  const cli::BatchReport skippedReport{{skippedOutput}};
  const QJsonDocument skippedDoc = QJsonDocument::fromJson(cli::renderJsonReport(skippedReport).toUtf8());
  const QJsonArray skippedFiles = skippedDoc.object().value("output_files").toArray();
  QCOMPARE(skippedFiles.size(), 1);
  QVERIFY(skippedFiles[0].toObject().value("output_file").isNull());
  QCOMPARE(skippedDoc.object().value("status").toString(), QStringLiteral("ok"));
}

void TestCliQt::mapsExitCode() {
  cli::BatchReport empty;
  QCOMPARE(codeOf(cli::exitCodeFor(empty)), codeOf(cli::ExitCode::SUCCESS));

  cli::PageOutcome okPage;
  okPage.ok = true;
  cli::BatchReport allOk;
  allOk.pages = {okPage};
  QCOMPARE(codeOf(cli::exitCodeFor(allOk)), codeOf(cli::ExitCode::SUCCESS));

  cli::PageOutcome failedPage;
  cli::BatchReport withFailure;
  withFailure.pages = {okPage, failedPage};
  QCOMPARE(codeOf(cli::exitCodeFor(withFailure)), codeOf(cli::ExitCode::FILTER_ERROR));
}
