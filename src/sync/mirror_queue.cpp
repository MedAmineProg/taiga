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

#include "mirror_queue.hpp"

#include <algorithm>
#include <chrono>

#include "sync/retry.hpp"

namespace taiga::sync {

MirrorItem& mergeMirrorItem(QList<MirrorItem>& items, const QString& service, const int animeId,
                            const anime::list::Fields dirty, const bool remove,
                            const std::time_t now) {
  const auto it = std::ranges::find_if(items, [&](const MirrorItem& item) {
    return item.service == service && item.anime_id == animeId;
  });

  if (it == items.end()) {
    items.append(MirrorItem{
        .service = service,
        .anime_id = animeId,
        .dirty = remove ? anime::list::Fields{} : dirty,
        .remove = remove,
        .time = now,
    });
    return items.last();
  }

  if (remove) {
    it->remove = true;
    it->dirty = {};
  } else if (it->remove) {
    // Added back before the removal was sent, so the whole entry has to be sent again.
    it->remove = false;
    it->dirty = kAllListFields;
  } else {
    it->dirty |= dirty;
  }

  it->time = now;
  it->retry_count = 0;
  it->last_error.clear();
  it->next_attempt = 0;

  return *it;
}

const MirrorItem* nextMirrorItem(const QList<MirrorItem>& items, const std::time_t now) {
  const MirrorItem* next = nullptr;
  for (const auto& item : items) {
    if (item.gaveUp() || item.next_attempt > now) continue;
    if (!next || item.time < next->time) next = &item;
  }
  return next;
}

std::optional<std::time_t> nextMirrorAttempt(const QList<MirrorItem>& items) {
  std::optional<std::time_t> next;
  for (const auto& item : items) {
    if (item.gaveUp()) continue;
    if (!next || item.next_attempt < *next) next = item.next_attempt;
  }
  return next;
}

MirrorStatus mirrorStatus(const QList<MirrorItem>& items, const QString& service) {
  MirrorStatus status;
  std::time_t lastErrorTime = 0;
  for (const auto& item : items) {
    if (item.service != service) continue;
    if (item.gaveUp()) {
      ++status.failed;
    } else {
      ++status.pending;
    }
    if (!item.last_error.isEmpty() && item.time >= lastErrorTime) {
      lastErrorTime = item.time;
      status.lastError = item.last_error;
    }
  }
  return status;
}

void failMirrorItem(MirrorItem& item, const QString& error, const bool permanent,
                    const std::time_t now) {
  item.last_error = error;
  item.retry_count = permanent ? kMirrorMaxAttempts : item.retry_count + 1;
  item.next_attempt = now + retryDelay(item.retry_count).count();
}

bool isMappingFresh(const bool found, const std::time_t checked, const std::time_t now) {
  constexpr auto kRecheckMissing = std::chrono::days{7};
  if (found) return true;
  return now - checked < std::chrono::duration_cast<std::chrono::seconds>(kRecheckMissing).count();
}

}  // namespace taiga::sync
