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
#include "list_undo.hpp"

#include <algorithm>

namespace anime::list {

Summary summarize(const Change& change) {
  Summary summary;

  const bool wasInList = change.previous && change.previous->status != Status::NotInList &&
                         !change.previous->pending_delete;
  if (!wasInList) {
    summary.added = true;
    return summary;
  }

  const auto& before = *change.previous;
  const auto& after = change.next;
  if (before.watched_episodes != after.watched_episodes) summary.fields |= Field::Episode;
  if (before.score != after.score) summary.fields |= Field::Score;
  if (before.status != after.status) summary.fields |= Field::Status;
  if (before.rewatching != after.rewatching) summary.fields |= Field::Rewatching;
  if (before.rewatched_times != after.rewatched_times) summary.fields |= Field::RewatchedTimes;
  if (before.date_started != after.date_started) summary.fields |= Field::DateStarted;
  if (before.date_completed != after.date_completed) summary.fields |= Field::DateCompleted;
  if (before.notes != after.notes) summary.fields |= Field::Notes;

  return summary;
}

void UndoStack::record(const Change& change, const qint64 nowMs) {
  const bool groupWithLast =
      !groups_.isEmpty() && nowMs - lastRecordMs_ >= 0 && nowMs - lastRecordMs_ <= kGroupWindowMs;
  lastRecordMs_ = nowMs;

  if (!groupWithLast) {
    groups_.append(ChangeGroup{.id = nextId_++});
    while (groups_.size() > kMaxGroups) groups_.removeFirst();
  }

  auto& group = groups_.last();
  const auto it = std::ranges::find(group.changes, change.anime_id, &Change::anime_id);
  if (it != group.changes.end()) {
    // Keep the original state, so that undoing restores what was there before the group.
    it->next = change.next;
    it->history_times.append(change.history_times);
  } else {
    group.changes.append(change);
  }

  emit recorded(group.id);
}

const ChangeGroup* UndoStack::group(const int id) const {
  const auto it = std::ranges::find(groups_, id, &ChangeGroup::id);
  return it != groups_.end() ? &(*it) : nullptr;
}

std::optional<ChangeGroup> UndoStack::take(const int id) {
  const auto it = std::ranges::find(groups_, id, &ChangeGroup::id);
  if (it == groups_.end()) return std::nullopt;
  auto group = *it;
  groups_.erase(it);
  return group;
}

void UndoStack::clear() {
  groups_.clear();
}

UndoStack::Guard::Guard() {
  ++suppressed_;
}

UndoStack::Guard::~Guard() {
  --suppressed_;
}

bool UndoStack::isSuppressed() {
  return suppressed_ > 0;
}

}  // namespace anime::list
