// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_DOCUMENTCHANGENOTIFIER_H_
#define SCANTAILOR_CORE_DOCUMENTCHANGENOTIFIER_H_

#include <QMutex>
#include <atomic>
#include <functional>
#include <utility>

#include "NonCopyable.h"

/**
 * \brief Tells the document's one observer that its persisted state may have changed.
 *
 * Every owner of persisted state (the filter settings, the image settings, the page list)
 * calls notify() after a write. Being told that something changed is not the same as being
 * told the document is dirty: a write may store the value that was already there, so the
 * document decides by comparing its serialized state.
 *
 * notify() is callable from any thread, including the worker threads that write computed
 * results. The handler therefore has to be cheap and thread-safe; the editor's handler only
 * posts an event.
 */
class DocumentChangeNotifier {
  DECLARE_NON_COPYABLE(DocumentChangeNotifier)

 public:
  using Handler = std::function<void()>;

  DocumentChangeNotifier() = default;

  /** Installs the handler; call it from the thread that owns the observer. */
  void setHandler(Handler handler);

  /** Removes the handler, and returns once a call already running has finished. */
  void clearHandler();

  /** Notifies the handler, unless there is none. Never blocks on the caller's own locks. */
  void notify();

 private:
  QMutex m_mutex;
  Handler m_handler;
  std::atomic<bool> m_hasHandler{false};
};

/**
 * \brief Notifies on scope exit, so every early return of a writer still notifies.
 *
 * It stores a raw pointer because the notifier is owned by the object being written: nothing
 * can destroy it while the guard is alive, and the guard costs no reference count traffic.
 */
class DocumentChangeGuard {
 public:
  explicit DocumentChangeGuard(DocumentChangeNotifier* notifier) : m_notifier(notifier) {}

  ~DocumentChangeGuard() {
    if (m_notifier) {
      m_notifier->notify();
    }
  }

  DocumentChangeGuard(const DocumentChangeGuard&) = delete;
  DocumentChangeGuard& operator=(const DocumentChangeGuard&) = delete;

 private:
  DocumentChangeNotifier* m_notifier;
};

#endif  // ifndef SCANTAILOR_CORE_DOCUMENTCHANGENOTIFIER_H_
