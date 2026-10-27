// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StageChain.h"

#include <algorithm>
#include <cassert>

#include "LoadFileTask.h"
#include "PageInfo.h"
#include "StageSequence.h"
#include "filters/deskew/CacheDrivenTask.h"
#include "filters/deskew/Task.h"
#include "filters/fix_orientation/CacheDrivenTask.h"
#include "filters/fix_orientation/Task.h"
#include "filters/output/CacheDrivenTask.h"
#include "filters/output/Task.h"
#include "filters/page_layout/CacheDrivenTask.h"
#include "filters/page_layout/Task.h"
#include "filters/page_split/CacheDrivenTask.h"
#include "filters/page_split/Task.h"
#include "filters/select_content/CacheDrivenTask.h"
#include "filters/select_content/Task.h"

StageChain::StageChain(const std::shared_ptr<StageSequence>& stages, const Deps& deps)
    : m_stages(stages), m_deps(deps) {}

std::shared_ptr<BackgroundTask> StageChain::taskFor(const PageInfo& page, const int endStage) const {
  const int last = std::min(endStage, m_stages->count() - 1);
  assert(last >= 0);

  std::shared_ptr<fix_orientation::Task> fixOrientationTask;
  std::shared_ptr<page_split::Task> pageSplitTask;
  std::shared_ptr<deskew::Task> deskewTask;
  std::shared_ptr<select_content::Task> selectContentTask;
  std::shared_ptr<page_layout::Task> pageLayoutTask;
  std::shared_ptr<output::Task> outputTask;

  // Only the first rung that is built carries the debug flag.
  bool debug = m_deps.debug;

  if (m_deps.batch) {
    debug = false;
  }

  if (last >= m_stages->outputFilterIdx()) {
    outputTask = m_stages->outputFilter()->createTask(page.id(), m_deps.thumbnailCache, m_deps.outFileNameGen,
                                                      m_deps.batch, debug);
    debug = false;
  }
  if (last >= m_stages->pageLayoutFilterIdx()) {
    pageLayoutTask = m_stages->pageLayoutFilter()->createTask(page.id(), outputTask, m_deps.batch, debug);
    debug = false;
  }
  if (last >= m_stages->selectContentFilterIdx()) {
    selectContentTask = m_stages->selectContentFilter()->createTask(page.id(), pageLayoutTask, m_deps.batch, debug);
    debug = false;
  }
  if (last >= m_stages->deskewFilterIdx()) {
    deskewTask = m_stages->deskewFilter()->createTask(page.id(), selectContentTask, m_deps.batch, debug);
    debug = false;
  }
  if (last >= m_stages->pageSplitFilterIdx()) {
    pageSplitTask = m_stages->pageSplitFilter()->createTask(page, deskewTask, m_deps.batch, debug);
    debug = false;
  }
  if (last >= m_stages->fixOrientationFilterIdx()) {
    fixOrientationTask = m_stages->fixOrientationFilter()->createTask(page.id(), pageSplitTask, m_deps.batch);
    debug = false;
  }

  assert(fixOrientationTask);
  return std::make_shared<LoadFileTask>(m_deps.batch ? BackgroundTask::BATCH : BackgroundTask::INTERACTIVE, page,
                                        m_deps.thumbnailCache, m_deps.pages, fixOrientationTask);
}  // StageChain::taskFor

std::shared_ptr<CompositeCacheDrivenTask> StageChain::cacheDrivenTaskFor(const int endStage) const {
  const int last = std::min(endStage, m_stages->count() - 1);
  assert(last >= 0);

  std::shared_ptr<fix_orientation::CacheDrivenTask> fixOrientationTask;
  std::shared_ptr<page_split::CacheDrivenTask> pageSplitTask;
  std::shared_ptr<deskew::CacheDrivenTask> deskewTask;
  std::shared_ptr<select_content::CacheDrivenTask> selectContentTask;
  std::shared_ptr<page_layout::CacheDrivenTask> pageLayoutTask;
  std::shared_ptr<output::CacheDrivenTask> outputTask;

  if (last >= m_stages->outputFilterIdx()) {
    outputTask = m_stages->outputFilter()->createCacheDrivenTask(m_deps.outFileNameGen);
  }
  if (last >= m_stages->pageLayoutFilterIdx()) {
    pageLayoutTask = m_stages->pageLayoutFilter()->createCacheDrivenTask(outputTask);
  }
  if (last >= m_stages->selectContentFilterIdx()) {
    selectContentTask = m_stages->selectContentFilter()->createCacheDrivenTask(pageLayoutTask);
  }
  if (last >= m_stages->deskewFilterIdx()) {
    deskewTask = m_stages->deskewFilter()->createCacheDrivenTask(selectContentTask);
  }
  if (last >= m_stages->pageSplitFilterIdx()) {
    pageSplitTask = m_stages->pageSplitFilter()->createCacheDrivenTask(deskewTask);
  }
  if (last >= m_stages->fixOrientationFilterIdx()) {
    fixOrientationTask = m_stages->fixOrientationFilter()->createCacheDrivenTask(pageSplitTask);
  }

  assert(fixOrientationTask);
  return fixOrientationTask;
}  // StageChain::cacheDrivenTaskFor
