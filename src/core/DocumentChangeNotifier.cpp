// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "DocumentChangeNotifier.h"

#include <QMutexLocker>

void DocumentChangeNotifier::setHandler(Handler handler) {
  QMutexLocker lock(&m_mutex);
  m_handler = std::move(handler);
  m_hasHandler.store(m_handler != nullptr, std::memory_order_release);
}

void DocumentChangeNotifier::clearHandler() {
  QMutexLocker lock(&m_mutex);
  m_handler = nullptr;
  m_hasHandler.store(false, std::memory_order_release);
}

void DocumentChangeNotifier::notify() {
  // The command line and the tests never install a handler; they pay one atomic load per
  // write instead of a lock.
  if (!m_hasHandler.load(std::memory_order_acquire)) {
    return;
  }

  QMutexLocker lock(&m_mutex);
  if (m_handler) {
    m_handler();
  }
}
