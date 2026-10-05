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
#include "airing_schedule.hpp"

#include <algorithm>
#include <cmath>

namespace anime::schedule {

namespace {

constexpr std::time_t kWeek = 7 * 24 * 60 * 60;

template <typename Fn>
void forEachListed(const QMap<int, Details>& items, const QMap<int, list::Entry>& entries,
                   const QSet<list::Status>& statuses, Fn&& fn) {
  for (const auto& entry : entries) {
    if (entry.pending_delete || !statuses.contains(entry.status)) continue;
    const auto item = items.find(entry.anime_id);
    if (item == items.end() || item->next_episode_time <= 0) continue;
    fn(*item);
  }
}

void sortByTime(std::vector<Episode>& episodes) {
  std::ranges::sort(episodes, [](const Episode& a, const Episode& b) {
    if (a.time != b.time) return a.time < b.time;
    return a.anime_id < b.anime_id;
  });
}

}  // namespace

std::vector<Episode> episodesBetween(const QMap<int, Details>& items,
                                     const QMap<int, list::Entry>& entries,
                                     const QSet<list::Status>& statuses, const std::time_t from,
                                     const std::time_t to) {
  std::vector<Episode> result;
  if (to <= from) return result;

  forEachListed(items, entries, statuses, [&](const Details& item) {
    const auto next = item.next_episode_time;
    const int nextNumber = item.last_aired_episode + 1;

    // Weeks relative to the next episode that fall in the range
    const auto first = static_cast<long long>(std::ceil(double(from - next) / kWeek));
    const auto last = static_cast<long long>(std::ceil(double(to - next) / kWeek)) - 1;

    for (auto week = first; week <= last; ++week) {
      const auto number = nextNumber + week;
      if (number < 1) continue;
      if (item.episode_count > 0 && number > item.episode_count) break;
      result.push_back({
          .anime_id = item.id,
          .number = static_cast<int>(number),
          .time = next + week * kWeek,
          .estimated = week != 0,
      });
    }
  });

  sortByTime(result);
  return result;
}

std::vector<Episode> airedBetween(const QMap<int, Details>& items,
                                  const QMap<int, list::Entry>& entries,
                                  const QSet<list::Status>& statuses, const std::time_t from,
                                  const std::time_t to) {
  std::vector<Episode> result;

  forEachListed(items, entries, statuses, [&](const Details& item) {
    if (item.next_episode_time > from && item.next_episode_time <= to) {
      result.push_back({
          .anime_id = item.id,
          .number = item.last_aired_episode + 1,
          .time = item.next_episode_time,
      });
    }
  });

  sortByTime(result);
  return result;
}

}  // namespace anime::schedule
