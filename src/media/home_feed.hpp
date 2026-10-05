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

#include <QHash>
#include <QMap>
#include <QSet>
#include <chrono>
#include <ctime>
#include <vector>

#include "media/anime.hpp"
#include "media/anime_list.hpp"

namespace anime::home {

// Episode numbers found in library folders, keyed by anime ID.
using AvailableEpisodes = QHash<int, QSet<int>>;

struct ContinueItem {
  int anime_id = kUnknownId;
  int watched_episodes = 0;
  int episode_count = kUnknownEpisodeCount;
  int next_episode = 1;
  bool next_episode_available = false;  // the next episode is in a library folder
  int available_count = 0;              // unwatched episodes in library folders
  int aired_unwatched = 0;              // episodes that aired but haven't been watched
  std::time_t last_updated = 0;
};

struct AiringItem {
  int anime_id = kUnknownId;
  int episode = 0;
  std::time_t time = 0;
  list::Status status = list::Status::NotInList;
};

// Anime being watched, with those that have their next episode ready first, then the most
// recently updated.
std::vector<ContinueItem> continueWatching(const QMap<int, Details>& items,
                                           const QMap<int, list::Entry>& entries,
                                           const AvailableEpisodes& available, std::size_t limit);

// Upcoming episodes of anime being watched or planned, in the order they air.
std::vector<AiringItem> airingSoon(const QMap<int, Details>& items,
                                   const QMap<int, list::Entry>& entries, std::time_t now,
                                   std::chrono::seconds window, std::size_t limit);

}  // namespace anime::home
