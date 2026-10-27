// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PROJECTDOCUMENT_H_
#define SCANTAILOR_CORE_PROJECTDOCUMENT_H_

#include <QString>
#include <Qt>
#include <memory>
#include <vector>

#include "BeforeOrAfter.h"
#include "NonCopyable.h"
#include "OutputFileNameGenerator.h"
#include "PageId.h"
#include "PageInfo.h"
#include "PageView.h"
#include "SelectedPage.h"

class AbstractRelinker;
class FileNameDisambiguator;
class ImageId;
class ImageInfo;
class PageSequence;
class PageSelectionAccessor;
class ProjectPages;
class ProjectReader;
class StageSequence;

/**
 * \brief Everything a project is: pages, stages, output naming and the selection.
 *
 * The editor, the CLI and the out-of-memory save dialog all own one of these instead of
 * keeping their own copies of the four. That is what keeps the disambiguation labels, the
 * registration order and the selection rules identical between them.
 */
class ProjectDocument {
  DECLARE_NON_COPYABLE(ProjectDocument)

 public:
  /** A project with no pages: what the editor holds before anything is opened. */
  static std::shared_ptr<ProjectDocument> createEmpty(const PageSelectionAccessor& accessor);

  /** Wraps \p pages, natively registered in IMAGE_VIEW order, with new filter stages. */
  static std::shared_ptr<ProjectDocument> create(const std::shared_ptr<ProjectPages>& pages,
                                                 const QString& outputDir,
                                                 const PageSelectionAccessor& accessor);

  /** Takes pages, disambiguation labels, selection and filter settings from \p reader. */
  static std::shared_ptr<ProjectDocument> fromProject(const ProjectReader& reader,
                                                      const QString& outputDir,
                                                      const PageSelectionAccessor& accessor);

  const std::shared_ptr<ProjectPages>& pages() const { return m_pages; }

  const std::shared_ptr<StageSequence>& stages() const { return m_stages; }

  const OutputFileNameGenerator& outputFileNameGenerator() const { return m_outputFileNameGenerator; }

  const SelectedPage& selectedPage() const { return m_selectedPage; }

  bool isLoaded() const;

  void selectPage(const PageId& page, PageView view) { m_selectedPage.set(page, view); }

  /**
   * \brief The page to select in \p availablePages for the remembered selection.
   *
   * Tries the remembered sub-page, then the left half, the right half and the single page,
   * and finally \p fallback, which is the first page as the caller displays them.
   */
  PageId resolveSelection(const PageSequence& availablePages, PageView view, const PageId& fallback) const;

  /** Inserts \p image and returns the pages that were actually added, in the order asked for. */
  std::vector<PageInfo> insertImage(const ImageInfo& image,
                                    BeforeOrAfter position,
                                    const ImageId& existing,
                                    PageView view);

  void setLayoutDirection(Qt::LayoutDirection direction);

  void performRelinking(const AbstractRelinker& relinker);

  bool save(const QString& filePath) const;

 private:
  ProjectDocument(const std::shared_ptr<ProjectPages>& pages,
                  std::shared_ptr<FileNameDisambiguator> disambiguator,
                  const QString& outputDir);

  std::shared_ptr<ProjectPages> m_pages;
  OutputFileNameGenerator m_outputFileNameGenerator;
  std::shared_ptr<StageSequence> m_stages;
  SelectedPage m_selectedPage;
};

#endif  // ifndef SCANTAILOR_CORE_PROJECTDOCUMENT_H_
