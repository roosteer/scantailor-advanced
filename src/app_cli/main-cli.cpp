// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <memory>
#include <vector>

#include "CliRequest.h"
#include "ConsoleBatch.h"
#include "ExitCode.h"
#include "FilterConfigAdapter.h"
#include "FileNameDisambiguator.h"
#include "ImageFileInfo.h"
#include "ImageMetadata.h"
#include "ImageMetadataLoader.h"
#include "OutputFileNameGenerator.h"
#include "PageSelectionAccessor.h"
#include "PageSelectionProvider.h"
#include "PageSequence.h"
#include "ProjectPages.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "SelectedPage.h"
#include "StageSequence.h"

using namespace cli;

namespace {

/**
 * Minimal PageSelectionProvider for headless CLI use.
 * Returns all pages — the CLI always processes everything.
 */
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

  std::vector<PageRange> selectedRanges() const override {
    return std::vector<PageRange>();
  }

 private:
  std::shared_ptr<ProjectPages> m_pages;
};

}  // anonymous namespace

static ExitCode loadImages(const CliRequest& request,
                           std::shared_ptr<ProjectPages>& outPages,
                           std::shared_ptr<StageSequence>& outStages) {
  std::vector<ImageFileInfo> files;
  files.reserve(request.images.size());

  for (const QString& path : request.images) {
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
      QTextStream err(stderr);
      err << "Error: File not found: " << path << "\n";
      return ExitCode::FILE_NOT_FOUND;
    }

    std::vector<ImageMetadata> metadataList;
    const auto status = ImageMetadataLoader::load(
        fileInfo.absoluteFilePath(),
        [&metadataList](const ImageMetadata& metadata) { metadataList.push_back(metadata); });

    if (status != ImageMetadataLoader::LOADED) {
      QTextStream err(stderr);
      err << "Error: Cannot load image: " << path << "\n";
      return ExitCode::FILE_NOT_FOUND;
    }

    files.emplace_back(fileInfo, metadataList);
  }

  outPages = std::make_shared<ProjectPages>(files, ProjectPages::AUTO_PAGES, Qt::LeftToRight);

  auto provider = std::make_shared<CliPageSelectionProvider>(outPages);
  outStages = std::make_shared<StageSequence>(outPages, PageSelectionAccessor(provider));

  // Load default settings for all pages.
  const PageSequence pageSequence = outPages->toPageSequence(PAGE_VIEW);
  for (size_t i = 0; i < pageSequence.numPages(); ++i) {
    const PageInfo& page = pageSequence.pageAt(i);
    for (const auto& filter : outStages->filters()) {
      filter->loadDefaultSettings(page);
    }
  }

  return ExitCode::SUCCESS;
}

static ExitCode loadProject(const QString& projectFilePath,
                            std::shared_ptr<ProjectPages>& outPages,
                            std::shared_ptr<StageSequence>& outStages) {
  QFile file(projectFilePath);
  if (!file.exists()) {
    QTextStream err(stderr);
    err << "Error: Project file not found: " << projectFilePath << "\n";
    return ExitCode::FILE_NOT_FOUND;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    QTextStream err(stderr);
    err << "Error: Cannot open project file: " << projectFilePath << "\n";
    return ExitCode::FILE_NOT_FOUND;
  }

  QDomDocument doc;
  if (!doc.setContent(&file)) {
    file.close();
    QTextStream err(stderr);
    err << "Error: Invalid or corrupt project file: " << projectFilePath << "\n";
    return ExitCode::FILTER_ERROR;
  }
  file.close();

  ProjectReader reader(doc, projectFilePath);
  if (!reader.success()) {
    QTextStream err(stderr);
    err << "Error: Failed to read project file: " << projectFilePath << "\n";
    return ExitCode::FILTER_ERROR;
  }

  outPages = reader.pages();

  auto provider = std::make_shared<CliPageSelectionProvider>(outPages);
  outStages = std::make_shared<StageSequence>(outPages, PageSelectionAccessor(provider));

  // Load filter settings from project.
  reader.readFilterSettings(outStages->filters());

  return ExitCode::SUCCESS;
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("scantailor-advanced-cli");

  const QStringList args = QApplication::arguments();
  // args[0] is the program name; skip it.
  const QStringList cliArgs = args.mid(1);

  CliRequest request;
  QString errorMsg;
  const ExitCode parseResult = CliRequest::parse(cliArgs, request, errorMsg);

  if (parseResult != ExitCode::SUCCESS) {
    QTextStream err(stderr);
    err << "Error: " << errorMsg << "\n";
    err << "Use --help for usage information.\n";
    return static_cast<int>(parseResult);
  }

  if (request.help) {
    printHelp();
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (request.version) {
    printVersion();
    return static_cast<int>(ExitCode::SUCCESS);
  }

  // Validate input files exist.
  std::shared_ptr<ProjectPages> pages;
  std::shared_ptr<StageSequence> stages;
  ExitCode loadResult;

  if (!request.projectFile.isEmpty()) {
    loadResult = loadProject(request.projectFile, pages, stages);
  } else {
    loadResult = loadImages(request, pages, stages);
  }

  if (loadResult != ExitCode::SUCCESS) {
    return static_cast<int>(loadResult);
  }

  // Determine output directory for project mode (use project's output dir).
  CliRequest effectiveRequest = request;
  if (!request.projectFile.isEmpty() && effectiveRequest.outputDir.isEmpty()) {
    // Read output directory from project
    QFile file(request.projectFile);
    if (file.open(QIODevice::ReadOnly)) {
      QDomDocument doc;
      if (doc.setContent(&file)) {
        ProjectReader reader(doc, request.projectFile);
        effectiveRequest.outputDir = reader.outputDirectory();
      }
      file.close();
    }
  }

  ConsoleBatch batch(effectiveRequest, pages, stages);
  const ExitCode processResult = batch.process();

  // Save project file if requested.
  if (!request.outputProjectFile.isEmpty() && processResult == ExitCode::SUCCESS) {
    const PageSequence pageSequence = pages->toPageSequence(PAGE_VIEW);
    auto disambiguator = std::make_shared<FileNameDisambiguator>();
    OutputFileNameGenerator outFileNameGen(disambiguator, effectiveRequest.outputDir, pages->layoutDirection());
    const SelectedPage selectedPage(pageSequence.pageAt(0).id(), PAGE_VIEW);
    ProjectWriter writer(pages, selectedPage, outFileNameGen);
    if (!writer.write(request.outputProjectFile, stages->filters())) {
      QTextStream err(stderr);
      err << "Error: Failed to save project file: " << request.outputProjectFile << "\n";
      return static_cast<int>(ExitCode::OUTPUT_ERROR);
    }
  }

  return static_cast<int>(processResult);
}
