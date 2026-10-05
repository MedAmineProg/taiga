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

#include <QMap>
#include <QSet>
#include <ctime>
#include <vector>

#include "media/anime.hpp"
#include "media/anime_list.hpp"

namespace anime::schedule {

struct Episode {
  int anime_id = kUnknownId;
  int number = 0;
  std::time_t time = 0;
  bool estimated = false;  // projected from the next episode, assuming a weekly schedule
};

// Episodes of anime in the list with the given statuses that air in [from, to), in order.
//
// Only the next episode's air time is known, so other episodes in the range are estimated by
// repeating it weekly, within the episode count.
std::vector<Episode> episodesBetween(const QMap<int, Details>& items,
                                     const QMap<int, list::Entry>& entries,
                                     const QSet<list::Status>& statuses, std::time_t from,
                                     std::time_t to);

// Episodes with a known (not estimated) air time in (from, to], e.g. for notifications.
std::vector<Episode> airedBetween(const QMap<int, Details>& items,
                                  const QMap<int, list::Entry>& entries,
                                  const QSet<list::Status>& statuses, std::time_t from,
                                  std::time_t to);

}  // namespace anime::schedule
