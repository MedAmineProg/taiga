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
#include "home_feed.hpp"

#include <algorithm>

namespace anime::home {

std::vector<ContinueItem> continueWatching(const QMap<int, Details>& items,
                                           const QMap<int, list::Entry>& entries,
                                           const AvailableEpisodes& available,
                                           const std::size_t limit) {
  std::vector<ContinueItem> result;

  for (const auto& entry : entries) {
    if (entry.pending_delete) continue;
    if (entry.status != list::Status::Watching && !entry.rewatching) continue;

    const auto item = items.find(entry.anime_id);
    if (item == items.end()) continue;

    const int watched = entry.rewatching ? entry.rewatching_ep : entry.watched_episodes;
    const bool hasKnownCount = item->episode_count > 0;
    if (hasKnownCount && watched >= item->episode_count) continue;  // nothing left to watch

    ContinueItem result_item{
        .anime_id = entry.anime_id,
        .watched_episodes = watched,
        .episode_count = item->episode_count,
        .next_episode = watched + 1,
        .last_updated = entry.last_updated,
    };

    if (const auto episodes = available.find(entry.anime_id); episodes != available.end()) {
      result_item.next_episode_available = episodes->contains(watched + 1);
      result_item.available_count = static_cast<int>(
          std::ranges::count_if(*episodes, [watched](int number) { return number > watched; }));
    }

    if (item->last_aired_episode > watched) {
      result_item.aired_unwatched = item->last_aired_episode - watched;
    } else if (hasKnownCount && item->status == Status::FinishedAiring) {
      result_item.aired_unwatched = item->episode_count - watched;
    }

    result.push_back(result_item);
  }

  std::ranges::sort(result, [](const ContinueItem& a, const ContinueItem& b) {
    if (a.next_episode_available != b.next_episode_available) return a.next_episode_available;
    if (a.last_updated != b.last_updated) return a.last_updated > b.last_updated;
    return a.anime_id < b.anime_id;
  });

  if (result.size() > limit) result.resize(limit);

  return result;
}

std::vector<AiringItem> airingSoon(const QMap<int, Details>& items,
                                   const QMap<int, list::Entry>& entries, const std::time_t now,
                                   const std::chrono::seconds window, const std::size_t limit) {
  std::vector<AiringItem> result;

  for (const auto& entry : entries) {
    if (entry.pending_delete) continue;
    if (entry.status != list::Status::Watching && entry.status != list::Status::PlanToWatch) {
      continue;
    }

    const auto item = items.find(entry.anime_id);
    if (item == items.end()) continue;

    const auto time = item->next_episode_time;
    if (time <= now || time > now + window.count()) continue;

    result.push_back({
        .anime_id = entry.anime_id,
        .episode = item->last_aired_episode + 1,
        .time = time,
        .status = entry.status,
    });
  }

  std::ranges::sort(result, [](const AiringItem& a, const AiringItem& b) {
    if (a.time != b.time) return a.time < b.time;
    return a.anime_id < b.anime_id;
  });

  if (result.size() > limit) result.resize(limit);

  return result;
}

}  // namespace anime::home
