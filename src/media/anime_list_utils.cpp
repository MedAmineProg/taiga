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

#include "anime_list_utils.hpp"

#include <QDate>
#include <QElapsedTimer>
#include <ctime>
#include <optional>
#include <ranges>

#include "base/qdate.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/anime_list.hpp"
#include "media/list_undo.hpp"
#include "sync/queue.hpp"

namespace anime::list {

float getProgressRatio(const Details* item, const Entry* entry) {
  const auto progress = (entry ? entry->watched_episodes : 0);
  const auto total = (item ? item->episode_count : 0);
  if (!total) return 0.8f;
  return std::min(progress / static_cast<float>(total), 1.0f);
}

bool isInList(const Entry* entry) {
  // Pending removal counts as not being in list.
  return entry && entry->status != Status::NotInList && !entry->pending_delete;
}

Entry entryWithEpisodeWatched(const Details& item, const Entry* entry, const int number) {
  auto updated = isInList(entry) ? *entry : Entry{.anime_id = item.id};

  const bool isFinalEpisode = item.episode_count > 0 && number == item.episode_count;
  const FuzzyDate today{base::fromQDate(QDate::currentDate())};

  updated.watched_episodes = number;

  if (number == 1 && !updated.date_started) updated.date_started = today;
  if (isFinalEpisode && !updated.date_completed) updated.date_completed = today;

  if (updated.rewatching && isFinalEpisode) {
    updated.rewatching = false;
    updated.rewatched_times++;
  }

  if (isFinalEpisode) {
    updated.status = Status::Completed;
  } else if (!updated.rewatching) {
    updated.status = Status::Watching;
  }

  return updated;
}

void save(Entry entry) {
  const auto previousEntry = db.entry(entry.anime_id);
  // A copy, as the database is updated below.
  const auto previous = previousEntry ? std::optional{*previousEntry} : std::nullopt;
  const Entry baseline = previous ? *previous : Entry{.anime_id = entry.anime_id};

  Fields dirty;
  if (baseline.watched_episodes != entry.watched_episodes) dirty |= Field::Episode;
  if (baseline.score != entry.score) dirty |= Field::Score;
  if (baseline.status != entry.status) dirty |= Field::Status;
  if (baseline.rewatching != entry.rewatching) dirty |= Field::Rewatching;
  if (baseline.rewatched_times != entry.rewatched_times) dirty |= Field::RewatchedTimes;
  if (baseline.date_started != entry.date_started) dirty |= Field::DateStarted;
  if (baseline.date_completed != entry.date_completed) dirty |= Field::DateCompleted;
  if (baseline.notes != entry.notes) dirty |= Field::Notes;

  if (!dirty) return;

  entry.pending_delete = false;
  entry.last_updated = std::time(nullptr);
  db.updateEntry(entry);

  QList<std::time_t> historyTimes;
  // Undoing restores an earlier state, which isn't a newly watched episode.
  if ((dirty & Field::Episode) && entry.watched_episodes > 0 && !UndoStack::isSuppressed()) {
    history.add(entry.anime_id, entry.watched_episodes, entry.last_updated);
    historyTimes.append(entry.last_updated);
  }

  taiga::sync::queue.push(entry.anime_id, dirty);

  if (!UndoStack::isSuppressed()) {
    static QElapsedTimer clock;
    if (!clock.isValid()) clock.start();
    undoStack.record(
        Change{
            .anime_id = entry.anime_id,
            .previous = previous,
            .next = entry,
            .history_times = historyTimes,
        },
        clock.elapsed());
  }
}

void undo(const int groupId) {
  const auto group = undoStack.take(groupId);
  if (!group) return;

  const UndoStack::Guard guard;

  for (const auto& change : group->changes | std::views::reverse) {
    for (const auto time : change.history_times) {
      history.removeAt(change.anime_id, time);
    }

    if (change.previous && isInList(&*change.previous)) {
      save(*change.previous);
      continue;
    }

    // The anime wasn't in the list before.
    const auto current = db.entry(change.anime_id);
    if (current && current->id == kUnknownId) {
      // It was never sent to the service, so there's nothing to delete there.
      taiga::sync::queue.pop(change.anime_id);
      db.deleteEntry(change.anime_id);
    } else {
      remove(change.anime_id);
    }
  }
}

void remove(const int animeId) {
  const auto entry = db.entry(animeId);
  if (!entry) return;

  auto updated = *entry;
  updated.pending_delete = true;
  updated.last_updated = std::time(nullptr);

  db.updateEntry(updated);

  taiga::sync::queue.pushDelete(animeId);
}

}  // namespace anime::list
