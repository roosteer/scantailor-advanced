// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ProjectDocument.h"

#include <QFileInfo>

#include <cassert>
#include <utility>

#include "AbstractRelinker.h"
#include "DocumentChangeNotifier.h"
#include "FileNameDisambiguator.h"
#include "ImageId.h"
#include "ImageInfo.h"
#include "PageSelectionAccessor.h"
#include "PageSequence.h"
#include "ProjectPages.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "StageSequence.h"

namespace {

std::shared_ptr<FileNameDisambiguator> newDisambiguator() {
  return std::make_shared<FileNameDisambiguator>();
}

}  // namespace

ProjectDocument::ProjectDocument(const std::shared_ptr<ProjectPages>& pages,
                                 std::shared_ptr<FileNameDisambiguator> disambiguator,
                                 const QString& outputDir)
    : m_pages(pages),
      m_outputFileNameGenerator(std::move(disambiguator), outputDir, pages->layoutDirection()),
      m_changeNotifier(std::make_shared<DocumentChangeNotifier>()),
      m_savedStateValid(false) {
  // IMAGE_VIEW order is the order the editor and the CLI both show, and the order that
  // gives two inputs with equal basenames distinct labels. It has to be decided once.
  for (const PageInfo& page : m_pages->toPageSequence(IMAGE_VIEW)) {
    m_outputFileNameGenerator.disambiguator()->registerFile(page.imageId().filePath());
  }

  // The page list is the one place that sees worker-thread writes of metadata and of the
  // per-page bookkeeping, so its signal is what turns them into document notifications. The
  // pages object is the connection's context, so the connection dies with the document.
  QObject::connect(
      m_pages.get(), &ProjectPages::modified, m_pages.get(), [notifier = m_changeNotifier] { notifier->notify(); },
      Qt::DirectConnection);
}

std::shared_ptr<ProjectDocument> ProjectDocument::createEmpty(const PageSelectionAccessor& accessor) {
  std::shared_ptr<ProjectDocument> document(
      new ProjectDocument(std::make_shared<ProjectPages>(), newDisambiguator(), QString()));
  document->m_stages
      = std::make_shared<StageSequence>(document->m_pages, accessor, document->m_changeNotifier);
  return document;
}

std::shared_ptr<ProjectDocument> ProjectDocument::create(const std::shared_ptr<ProjectPages>& pages,
                                                         const QString& outputDir,
                                                         const PageSelectionAccessor& accessor) {
  assert(pages);

  std::shared_ptr<ProjectDocument> document(new ProjectDocument(pages, newDisambiguator(), outputDir));
  document->m_stages
      = std::make_shared<StageSequence>(document->m_pages, accessor, document->m_changeNotifier);
  return document;
}

std::shared_ptr<ProjectDocument> ProjectDocument::fromProject(const ProjectReader& reader,
                                                              const QString& outputDir,
                                                              const PageSelectionAccessor& accessor) {
  assert(reader.success());

  // The reader's disambiguator already carries the labels stored in the file; registering
  // the pages again only fills in records written by older versions.
  std::shared_ptr<ProjectDocument> document(
      new ProjectDocument(reader.pages(), reader.namingDisambiguator(), outputDir));
  document->m_selectedPage = reader.selectedPage();
  document->m_stages
      = std::make_shared<StageSequence>(document->m_pages, accessor, document->m_changeNotifier);
  reader.readFilterSettings(document->m_stages->filters());
  return document;
}

bool ProjectDocument::isLoaded() const {
  return !m_outputFileNameGenerator.outDir().isEmpty();
}

void ProjectDocument::selectPage(const PageId& page, const PageView view) {
  m_selectedPage.set(page, view);
  m_changeNotifier->notify();
}

PageId ProjectDocument::resolveSelection(const PageSequence& availablePages,
                                         const PageView view,
                                         const PageId& fallback) const {
  if (availablePages.numPages() == 0) {
    return PageId();
  }

  const PageId remembered(m_selectedPage.get(view));
  const PageId candidates[] = {
      remembered,
      PageId(remembered.imageId(), PageId::LEFT_PAGE),
      PageId(remembered.imageId(), PageId::RIGHT_PAGE),
      PageId(remembered.imageId(), PageId::SINGLE_PAGE),
  };
  const int numCandidates = static_cast<int>(sizeof(candidates) / sizeof(candidates[0]));

  // One pass over the sequence, keeping the earliest candidate that is present: the
  // sequence may hold both halves of a spread, so document order alone is not enough.
  int best = numCandidates;
  for (const PageInfo& page : availablePages) {
    for (int i = 0; i < best; ++i) {
      if (page.id() == candidates[i]) {
        best = i;
        break;
      }
    }
    if (best == 0) {
      break;
    }
  }

  return (best < numCandidates) ? candidates[best] : fallback;
}

std::vector<PageInfo> ProjectDocument::insertImage(const ImageInfo& image,
                                                   const BeforeOrAfter position,
                                                   const ImageId& existing,
                                                   const PageView view) {
  std::vector<PageInfo> inserted(m_pages->insertImage(image, position, existing, view));

  // Only an inserted image gets a name of its own; a failed insertion must not eat a label.
  for (const PageInfo& page : inserted) {
    m_outputFileNameGenerator.disambiguator()->registerFile(page.imageId().filePath());
  }

  if (!inserted.empty()) {
    m_changeNotifier->notify();
  }
  return inserted;
}

void ProjectDocument::setLayoutDirection(const Qt::LayoutDirection direction) {
  m_pages->setLayoutDirection(direction);
  m_outputFileNameGenerator.setLayoutDirection(direction);
  m_changeNotifier->notify();
}

void ProjectDocument::performRelinking(const AbstractRelinker& relinker) {
  m_pages->performRelinking(relinker);
  m_stages->performRelinking(relinker);
  m_outputFileNameGenerator.performRelinking(relinker);
  m_changeNotifier->notify();
}

QByteArray ProjectDocument::serializedState(const QString& filePath) const {
  const ProjectWriter writer(m_pages, m_selectedPage, m_outputFileNameGenerator);
  return writer.serialize(filePath, m_stages->filters());
}

bool ProjectDocument::save(const QString& filePath) const {
  const QByteArray bytes(serializedState(filePath));

  if (!ProjectWriter::writeFile(filePath, bytes)) {
    // A failed write must not make the model look saved.
    return false;
  }

  m_savedState = bytes;
  m_savedPath = QFileInfo(filePath).absoluteFilePath();
  m_savedStateValid = true;
  return true;
}

bool ProjectDocument::hasUnsavedChanges(const QString& filePath) const {
  if (filePath.isEmpty() || !m_savedStateValid) {
    return true;
  }

  // The destination is part of the state: saving elsewhere produces different relative paths.
  if (QFileInfo(filePath).absoluteFilePath() != m_savedPath) {
    return true;
  }

  return serializedState(m_savedPath) != m_savedState;
}

void ProjectDocument::markSavedState(const QString& filePath) {
  if (filePath.isEmpty()) {
    m_savedState.clear();
    m_savedPath.clear();
    m_savedStateValid = false;
    return;
  }

  m_savedPath = QFileInfo(filePath).absoluteFilePath();
  m_savedState = serializedState(m_savedPath);
  m_savedStateValid = true;
}
