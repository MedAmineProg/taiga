/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <QList>
#include <QObject>
#include <ctime>
#include <optional>

#include "media/anime_list.hpp"

namespace anime::list {

// A list entry before and after a change.
struct Change {
  int anime_id = kUnknownId;
  std::optional<Entry> previous;  // nullopt if the anime wasn't in the list
  Entry next;
  QList<std::time_t> history_times;  // watched episodes added to history by the change
};

// Changes made together (e.g. a batch edit), undone together.
struct ChangeGroup {
  int id = 0;
  QList<Change> changes;
};

// What a change did, for describing it to the user.
struct Summary {
  bool added = false;  // the anime wasn't in the list before
  Fields fields;       // fields that changed
};

Summary summarize(const Change& change);

// Remembers recent list changes so that they can be undone.
class UndoStack final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(UndoStack)

public:
  static constexpr qint64 kGroupWindowMs = 1000;
  static constexpr qsizetype kMaxGroups = 20;

  UndoStack() = default;
  ~UndoStack() = default;

  // `nowMs` is a monotonic time, so that changes made in quick succession are grouped.
  void record(const Change& change, qint64 nowMs);

  const ChangeGroup* group(int id) const;
  std::optional<ChangeGroup> take(int id);
  void clear();

  // Changes made while a guard exists (e.g. by undoing) aren't recorded.
  class Guard {
  public:
    Guard();
    ~Guard();
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
  };
  static bool isSuppressed();

signals:
  void recorded(int groupId);

private:
  QList<ChangeGroup> groups_;
  qint64 lastRecordMs_ = 0;
  int nextId_ = 1;
  static inline int suppressed_ = 0;
};

inline UndoStack undoStack;

}  // namespace anime::list
