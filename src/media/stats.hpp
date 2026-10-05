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

#include <QDate>
#include <QList>
#include <QMap>
#include <array>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "media/anime.hpp"
#include "media/anime_list.hpp"

namespace anime::stats {

constexpr int kAssumedEpisodeLength = 24;  // minutes, when unknown
constexpr int kActivityDays = 53 * 7;      // about a year, in whole weeks

struct Stats {
  int anime_count = 0;
  QMap<list::Status, int> status_counts;

  int episodes_watched = 0;  // including rewatches
  int minutes_watched = 0;

  int scored_count = 0;
  double mean_score = 0.0;              // 0-100, over scored entries
  std::array<int, 10> score_buckets{};  // 1-10, 11-20, ..., 91-100

  std::vector<std::pair<std::string, int>> top_genres;  // by number of anime
  std::vector<std::pair<std::string, int>> top_studios;

  QMap<QDate, int> activity;  // episodes per day, from history
  int active_days = 0;        // in the activity window
  int current_streak = 0;     // consecutive days ending today (or yesterday)
  int longest_streak = 0;
};

// `watchTimes` are the times episodes were watched (e.g. from history). Anime that are planned or
// dropped without progress don't count toward genres and studios.
Stats compute(const QMap<int, Details>& items, const QMap<int, list::Entry>& entries,
              const QList<std::time_t>& watchTimes, QDate today, std::size_t topCount = 8);

}  // namespace anime::stats
