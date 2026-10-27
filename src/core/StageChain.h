// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_STAGECHAIN_H_
#define SCANTAILOR_CORE_STAGECHAIN_H_

#include <memory>

#include "NonCopyable.h"
#include "OutputFileNameGenerator.h"

class BackgroundTask;
class CompositeCacheDrivenTask;
class PageInfo;
class ProjectPages;
class StageSequence;
class ThumbnailPixmapCache;

/**
 * Builds the per-page processing chain: LoadFileTask followed by the enabled
 * stages, in stage order. Both the GUI and the CLI go through here, so the
 * stage order is written once.
 */
class StageChain {
  DECLARE_NON_COPYABLE(StageChain)

 public:
  /** Everything a chain needs beyond the page itself. */
  struct Deps {
    std::shared_ptr<ThumbnailPixmapCache> thumbnailCache;  // null is fine for cache-driven chains
    std::shared_ptr<ProjectPages> pages;
    OutputFileNameGenerator outFileNameGen;
    bool batch = false;
    bool debug = false;
  };

  StageChain(const std::shared_ptr<StageSequence>& stages, const Deps& deps);

  /** Chain that starts at LoadFileTask and stops after stage \p endStage (clamped to the last stage). */
  std::shared_ptr<BackgroundTask> taskFor(const PageInfo& page, int endStage) const;

  /** Cache-driven counterpart used by thumbnails and page propagators. */
  std::shared_ptr<CompositeCacheDrivenTask> cacheDrivenTaskFor(int endStage) const;

 private:
  std::shared_ptr<StageSequence> m_stages;
  Deps m_deps;
};

#endif  // ifndef SCANTAILOR_CORE_STAGECHAIN_H_
