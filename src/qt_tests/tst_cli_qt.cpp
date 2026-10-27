// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_cli_qt.h"

#include "BatchReport.h"
#include "CliParser.h"
#include "CliRequest.h"
#include "CliUsage.h"
#include "ExitCode.h"
#include "FilterConfigAdapter.h"
#include "FilterRange.h"

#include <ImageFileInfo.h>
#include <ImageMetadata.h>
#include <Margins.h>
#include <PageInfo.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <PageSequence.h>
#include <ProjectPages.h>
#include <StageSequence.h>
#include <imageproc/Dpi.h>

#include <filters/deskew/Params.h>
#include <filters/deskew/Settings.h>
#include <filters/fix_orientation/Settings.h>
#include <filters/output/Params.h>
#include <filters/output/Settings.h>
#include <filters/page_layout/Alignment.h>
#include <filters/page_layout/Params.h>
#include <filters/page_layout/Settings.h>
#include <filters/page_split/Settings.h>
#include <filters/select_content/Params.h>
#include <filters/select_content/Settings.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QtTest/QtTest>
#include <memory>
#include <set>
#include <vector>

namespace {

int codeOf(const cli::ParseResult& result) {
  return static_cast<int>(result.error.code);
}

int codeOf(cli::ExitCode code) {
  return static_cast<int>(code);
}

/** Every page of the run, as the CLI's own provider answers it. */
class TestPageSelectionProvider : public PageSelectionProvider {
 public:
  explicit TestPageSelectionProvider(std::shared_ptr<ProjectPages> pages) : m_pages(std::move(pages)) {}

  PageSequence allPages() const override { return m_pages->toPageSequence(PAGE_VIEW); }

  std::set<PageId> selectedPages() const override {
    std::set<PageId> ids;
    for (const PageInfo& page : m_pages->toPageSequence(PAGE_VIEW)) {
      ids.insert(page.id());
    }
    return ids;
  }

  std::vector<PageRange> selectedRanges() const override { return {}; }

 private:
  std::shared_ptr<ProjectPages> m_pages;
};

/** The stages and pages a CLI run configures, without touching image files. */
struct TestSession {
  std::shared_ptr<ProjectPages> pages;
  std::shared_ptr<StageSequence> stages;
  PageSequence sequence;
};

TestSession makeSession(const int imageCount) {
  std::vector<ImageFileInfo> files;
  for (int i = 0; i < imageCount; ++i) {
    const QString path = QStringLiteral("/scans/page-%1.png").arg(i);
    files.emplace_back(QFileInfo(path), std::vector<ImageMetadata>{ImageMetadata(QSize(1000, 1400), Dpi(300, 300))});
  }

  TestSession session;
  session.pages = std::make_shared<ProjectPages>(files, ProjectPages::ONE_PAGE, Qt::LeftToRight);
  const auto provider = std::make_shared<TestPageSelectionProvider>(session.pages);
  session.stages = std::make_shared<StageSequence>(session.pages, PageSelectionAccessor(provider));
  session.sequence = session.pages->toPageSequence(PAGE_VIEW);
  return session;
}

cli::CliRequest fullRequest() {
  cli::CliRequest request;
  request.orientation = 90;
  request.layoutType = page_split::TWO_PAGES;
  request.deskewMode = cli::DeskewMode::MANUAL;
  request.deskewAngle = 2.0;
  request.contentBox = cli::MmRect{10.0, 10.0, 110.0, 210.0};
  request.margins = cli::MmRect{5.0, 6.0, 7.0, 8.0};
  request.alignment = cli::Alignment::CENTER;
  request.outputDpi = 300;
  request.colorMode = output::MIXED;
  return request;
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

void TestCliQt::appliesStageOverrides() {
  const TestSession session = makeSession(2);
  const cli::CliRequest request = fullRequest();

  cli::FilterConfigAdapter adapter(request, session.stages, session.sequence);
  adapter.apply(cli::FilterRange());

  for (const PageInfo& page : session.sequence) {
    const PageId pageId = page.id();

    QCOMPARE(session.stages->fixOrientationFilter()->getSettings()->getRotationFor(pageId.imageId()).toDegrees(), 90);

    QVERIFY2(session.stages->pageSplitFilter()->getSettings()->getPageRecord(pageId.imageId()).combinedLayoutType()
                 == page_split::TWO_PAGES,
             qPrintable(pageId.imageId().filePath()));

    const std::unique_ptr<deskew::Params> deskewParams =
        session.stages->deskewFilter()->getSettings()->getPageParams(pageId);
    QVERIFY(deskewParams != nullptr);
    QVERIFY(deskewParams->mode() == MODE_MANUAL);
    QVERIFY(qAbs(deskewParams->deskewAngle() - 2.0) < 1e-9);

    const std::unique_ptr<select_content::Params> contentParams =
        session.stages->selectContentFilter()->getSettings()->getPageParams(pageId);
    QVERIFY(contentParams != nullptr);
    QCOMPARE(contentParams->pageRect(), QRectF(10.0, 10.0, 100.0, 200.0));

    const Margins margins = session.stages->pageLayoutFilter()->getSettings()->getHardMarginsMM(pageId);
    QCOMPARE(margins.left(), 5.0);
    QCOMPARE(margins.top(), 6.0);
    QCOMPARE(margins.right(), 7.0);
    QCOMPARE(margins.bottom(), 8.0);
    QVERIFY(session.stages->pageLayoutFilter()->getSettings()->getPageAlignment(pageId)
            == page_layout::Alignment(page_layout::Alignment::VCENTER, page_layout::Alignment::HCENTER));

    const output::Params outputParams = session.stages->outputFilter()->getSettings()->getParams(pageId);
    QVERIFY(outputParams.outputDpi() == Dpi(300, 300));
    QVERIFY(outputParams.colorParams().colorMode() == output::MIXED);
  }
}

void TestCliQt::preservesUnspecifiedStageSettings() {
  const TestSession session = makeSession(2);
  cli::CliRequest request;
  request.orientation = 270;

  cli::FilterConfigAdapter adapter(request, session.stages, session.sequence);
  adapter.apply(cli::FilterRange());

  const auto pageSplitSettings = session.stages->pageSplitFilter()->getSettings();
  for (const PageInfo& page : session.sequence) {
    const PageId pageId = page.id();
    QCOMPARE(session.stages->fixOrientationFilter()->getSettings()->getRotationFor(pageId.imageId()).toDegrees(), 270);
    QVERIFY(pageSplitSettings->getPageRecord(pageId.imageId()).params() == nullptr);
    QVERIFY(session.stages->deskewFilter()->getSettings()->isParamsNull(pageId));
    QVERIFY(session.stages->selectContentFilter()->getSettings()->isParamsNull(pageId));
    QVERIFY(session.stages->pageLayoutFilter()->getSettings()->isParamsNull(pageId));
    QVERIFY(session.stages->outputFilter()->getSettings()->isParamsNull(pageId));
  }
}

void TestCliQt::respectsApplicationRange() {
  {
    const TestSession session = makeSession(1);
    const cli::CliRequest request = fullRequest();

    cli::FilterConfigAdapter adapter(request, session.stages, session.sequence);
    adapter.apply(cli::FilterRange{0, session.stages->pageSplitFilterIdx()});

    const PageId pageId = session.sequence.pageAt(0).id();
    QVERIFY(!session.stages->fixOrientationFilter()->getSettings()->isRotationNull(pageId.imageId()));
    QVERIFY(session.stages->pageSplitFilter()->getSettings()->getPageRecord(pageId.imageId()).combinedLayoutType()
            == page_split::TWO_PAGES);
    QVERIFY(session.stages->deskewFilter()->getSettings()->isParamsNull(pageId));
    QVERIFY(session.stages->pageLayoutFilter()->getSettings()->isParamsNull(pageId));
    QVERIFY(session.stages->outputFilter()->getSettings()->isParamsNull(pageId));
  }
  {
    const TestSession session = makeSession(1);
    const cli::CliRequest request = fullRequest();

    cli::FilterConfigAdapter adapter(request, session.stages, session.sequence);
    adapter.apply(cli::FilterRange{session.stages->deskewFilterIdx(), cli::kFilterCount - 1});

    const PageId pageId = session.sequence.pageAt(0).id();
    QVERIFY(session.stages->fixOrientationFilter()->getSettings()->isRotationNull(pageId.imageId()));
    QVERIFY(session.stages->pageSplitFilter()->getSettings()->getPageRecord(pageId.imageId()).combinedLayoutType()
            == page_split::AUTO_LAYOUT_TYPE);
    QVERIFY(session.stages->deskewFilter()->getSettings()->getPageParams(pageId) != nullptr);
    QVERIFY(session.stages->pageLayoutFilter()->getSettings()->getPageParams(pageId) != nullptr);
    QVERIFY(session.stages->outputFilter()->getSettings()->getParams(pageId).outputDpi() == Dpi(300, 300));
  }
}
