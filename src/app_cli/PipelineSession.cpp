// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PipelineSession.h"

#include <BackgroundTask.h>
#include <ImageFileInfo.h>
#include <ImageMetadata.h>
#include <ImageMetadataLoader.h>
#include <PageInfo.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <ProjectDocument.h>
#include <ProjectPages.h>
#include <ProjectReader.h>
#include <StageChain.h>
#include <StageSequence.h>
#include <ThumbnailPixmapCache.h>

#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSize>

#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "ConsoleLog.h"
#include "FilterConfigAdapter.h"

namespace cli {

namespace {

/** Headless stand-in for MainWindow: the CLI always processes every page. */
class CliPageSelectionProvider : public PageSelectionProvider {
 public:
  explicit CliPageSelectionProvider(std::shared_ptr<ProjectPages> pages) : m_pages(std::move(pages)) {}

  PageSequence allPages() const override { return m_pages->toPageSequence(PAGE_VIEW); }

  std::set<PageId> selectedPages() const override {
    std::set<PageId> all;
    const PageSequence seq = allPages();
    for (size_t i = 0; i < seq.numPages(); ++i) {
      all.insert(seq.pageAt(i).id());
    }
    return all;
  }

  std::vector<PageRange> selectedRanges() const override { return std::vector<PageRange>(); }

 private:
  std::shared_ptr<ProjectPages> m_pages;
};

}  // namespace

PipelineSession::PipelineSession(const CliRequest& request) : m_request(request) {}

PipelineSession::~PipelineSession() {
  // The cache keeps files open, so it has to go before the directory does.
  m_thumbnailCache.reset();
  if (!m_thumbnailDir.isEmpty()) {
    QDir(m_thumbnailDir).removeRecursively();
  }
}

std::unique_ptr<PipelineSession> PipelineSession::open(const CliRequest& request, CliError& error) {
  std::unique_ptr<PipelineSession> session(new PipelineSession(request));

  const bool loaded = request.projectFile.isEmpty() ? session->loadImages(error) : session->loadProject(error);
  if (!loaded) {
    return nullptr;
  }

  if (session->m_pageSequence.numPages() == 0) {
    error = {ExitCode::INVALID_INPUT, QStringLiteral("No pages to process.")};
    return nullptr;
  }

  if (!session->m_outputDir.isEmpty()) {
    QDir dir(session->m_outputDir);
    if (!dir.exists() && !dir.mkpath(".")) {
      error = {ExitCode::OUTPUT_ERROR,
               QString("Failed to create output directory: %1").arg(session->m_outputDir)};
      return nullptr;
    }
  }

  session->m_thumbnailDir = QDir::tempPath() + "/scantailor-cli-thumbs-"
                            + QString::number(QCoreApplication::applicationPid());
  session->m_thumbnailCache = std::make_shared<ThumbnailPixmapCache>(session->m_thumbnailDir, QSize(100, 100), 100, 50);

  // A saved project records one selected page. The CLI has always recorded the first, and it
  // is chosen here, once, while the page order the request was made against is still in hand.
  session->m_document->selectPage(session->m_pageSequence.pageAt(0).id(), PAGE_VIEW);

  return session;
}

bool PipelineSession::loadImages(CliError& error) {
  std::vector<ImageFileInfo> files;
  files.reserve(m_request.images.size());

  for (const QString& path : m_request.images) {
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
      error = {ExitCode::FILE_NOT_FOUND, QString("File not found: %1").arg(path)};
      return false;
    }

    std::vector<ImageMetadata> metadataList;
    const auto status = ImageMetadataLoader::load(
        fileInfo.absoluteFilePath(),
        [&metadataList](const ImageMetadata& metadata) { metadataList.push_back(metadata); });
    if (status != ImageMetadataLoader::LOADED) {
      error = {ExitCode::FILE_NOT_FOUND, QString("Cannot load image: %1").arg(path)};
      return false;
    }

    files.emplace_back(fileInfo, metadataList);
  }

  // The page sequence the user asked for, not just filter settings: loading as
  // AUTO_PAGES and switching the layout later leaves the sequence split, so the
  // project file and the per-page params disagree with the output.
  ProjectPages::Pages pageMode = ProjectPages::AUTO_PAGES;
  if (m_request.layoutType) {
    switch (*m_request.layoutType) {
      case page_split::SINGLE_PAGE_UNCUT:
      case page_split::PAGE_PLUS_OFFCUT:
        pageMode = ProjectPages::ONE_PAGE;
        break;
      case page_split::TWO_PAGES:
        pageMode = ProjectPages::TWO_PAGES;
        break;
      case page_split::AUTO_LAYOUT_TYPE:
        break;
    }
  }

  auto pages = std::make_shared<ProjectPages>(files, pageMode, Qt::LeftToRight);
  m_outputDir = m_request.outputDir;

  auto provider = std::make_shared<CliPageSelectionProvider>(pages);
  m_document = ProjectDocument::create(pages, m_outputDir, PageSelectionAccessor(provider));

  m_pageSequence = pages->toPageSequence(PAGE_VIEW);
  for (const PageInfo& page : m_pageSequence) {
    for (const auto& filter : m_document->stages()->filters()) {
      filter->loadDefaultSettings(page);
    }
  }

  return true;
}

bool PipelineSession::loadProject(CliError& error) {
  const QString& projectFilePath = m_request.projectFile;

  QFile file(projectFilePath);
  if (!file.exists()) {
    error = {ExitCode::FILE_NOT_FOUND, QString("Project file not found: %1").arg(projectFilePath)};
    return false;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    error = {ExitCode::FILE_NOT_FOUND, QString("Cannot open project file: %1").arg(projectFilePath)};
    return false;
  }

  QDomDocument doc;
  if (!doc.setContent(&file)) {
    file.close();
    error = {ExitCode::FILTER_ERROR, QString("Invalid or corrupt project file: %1").arg(projectFilePath)};
    return false;
  }
  file.close();

  ProjectReader reader(doc, projectFilePath);
  if (!reader.success()) {
    error = {ExitCode::FILTER_ERROR, QString("Failed to read project file: %1").arg(projectFilePath)};
    return false;
  }

  const std::shared_ptr<ProjectPages> pages = reader.pages();

  m_outputDir = m_request.outputDir;
  if (m_outputDir.isEmpty()) {
    m_outputDir = reader.outputDirectory();
  }
  if (m_outputDir.isEmpty()) {
    m_outputDir = QFileInfo(projectFilePath).absolutePath();
  }

  std::set<QString> missingFiles;
  for (const PageInfo& page : pages->toPageSequence(IMAGE_VIEW)) {
    const QString& path = page.id().imageId().filePath();
    if (!QFileInfo::exists(path)) {
      missingFiles.insert(path);
    }
  }
  if (!missingFiles.empty()) {
    QStringList paths;
    for (const QString& path : missingFiles) {
      paths << path;
    }
    error = {ExitCode::FILE_NOT_FOUND,
             QString("Project references missing image(s): %1").arg(paths.join(QLatin1String("; ")))};
    return false;
  }

  auto provider = std::make_shared<CliPageSelectionProvider>(pages);
  m_document = ProjectDocument::fromProject(reader, m_outputDir, PageSelectionAccessor(provider));

  m_pageSequence = pages->toPageSequence(PAGE_VIEW);
  return true;
}

void PipelineSession::configure(const FilterRange& range) {
  FilterConfigAdapter adapter(m_request, m_document->stages(), m_pageSequence);
  adapter.apply(range);
}

BatchReport PipelineSession::process(const FilterRange& range, const ConsoleLog& log) {
  // range.start cannot shorten the chain: the CLI always begins at LoadFileTask
  // and has no cached stage inputs to resume from. It only limits which filters
  // receive the request's overrides.
  const StageChain chain(m_document->stages(),
                         StageChain::Deps{m_thumbnailCache, m_document->pages(),
                                          m_document->outputFileNameGenerator(), true, false});

  BatchReport report;
  report.pages.reserve(m_pageSequence.numPages());

  const bool producesOutput = (range.end >= m_document->stages()->outputFilterIdx());
  const int numPages = static_cast<int>(m_pageSequence.numPages());

  for (size_t i = 0; i < m_pageSequence.numPages(); ++i) {
    const PageInfo& page = m_pageSequence.pageAt(i);
    const QString inputFile = page.id().imageId().filePath();

    log.progress("process", QString("Page %1/%2: %3").arg(i + 1).arg(numPages).arg(inputFile));

    PageOutcome outcome;
    outcome.index = static_cast<int>(i);
    outcome.inputFile = inputFile;

    const std::shared_ptr<BackgroundTask> task = chain.taskFor(page, range.end);
    try {
      (*task)();
      outcome.ok = true;
      if (producesOutput) {
        outcome.outputFile = m_document->outputFileNameGenerator().filePathFor(page.id());
      }
    } catch (const std::exception& e) {
      outcome.errorMessage = QString::fromUtf8(e.what());
    } catch (...) {
      outcome.errorMessage = QStringLiteral("unknown error");
    }

    if (outcome.ok) {
      log.detail("process", QString("%1 -> %2").arg(outcome.inputFile, outcome.outputFile));
    } else {
      log.error("process", QString("Page %1 failed: %2").arg(outcome.inputFile, outcome.errorMessage));
    }

    report.pages.push_back(outcome);
  }

  return report;
}

bool PipelineSession::saveProject(const QString& path, CliError& error) {
  if (!m_document->save(path)) {
    error = {ExitCode::OUTPUT_ERROR, QString("Failed to save project file: %1").arg(path)};
    return false;
  }
  return true;
}

}  // namespace cli
