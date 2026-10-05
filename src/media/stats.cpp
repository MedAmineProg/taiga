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
#include "stats.hpp"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace anime::stats {

namespace {

std::vector<std::pair<std::string, int>> topOf(const QHash<QString, int>& counts,
                                               const std::size_t count) {
  std::vector<std::pair<std::string, int>> result;
  for (const auto& [name, n] : counts.asKeyValueRange()) {
    result.emplace_back(name.toStdString(), n);
  }
  std::ranges::sort(result, [](const auto& a, const auto& b) {
    if (a.second != b.second) return a.second > b.second;
    return a.first < b.first;
  });
  if (result.size() > count) result.resize(count);
  return result;
}

}  // namespace

Stats compute(const QMap<int, Details>& items, const QMap<int, list::Entry>& entries,
              const QList<std::time_t>& watchTimes, const QDate today, const std::size_t topCount) {
  Stats stats;

  QHash<QString, int> genres;
  QHash<QString, int> studios;
  double scoreSum = 0;

  for (const auto& entry : entries) {
    if (entry.pending_delete || entry.status == list::Status::NotInList) continue;

    ++stats.anime_count;
    ++stats.status_counts[entry.status];

    const auto item = items.find(entry.anime_id);
    const int episodeCount = item != items.end() ? item->episode_count : kUnknownEpisodeCount;
    const int episodeLength = item != items.end() && item->episode_length > 0
                                  ? item->episode_length
                                  : kAssumedEpisodeLength;

    int episodes = entry.watched_episodes;
    if (episodeCount > 0) episodes += entry.rewatched_times * episodeCount;
    if (entry.rewatching) episodes += entry.rewatching_ep;
    stats.episodes_watched += episodes;
    stats.minutes_watched += episodes * episodeLength;

    if (entry.score > 0) {
      ++stats.scored_count;
      scoreSum += entry.score;
      const int bucket = std::clamp((entry.score - 1) / 10, 0, 9);
      ++stats.score_buckets[bucket];
    }

    const bool started = entry.watched_episodes > 0 || entry.status == list::Status::Completed ||
                         entry.status == list::Status::Watching;
    if (started && item != items.end()) {
      for (const auto& genre : item->genres) ++genres[QString::fromStdString(genre)];
      for (const auto& studio : item->studios) ++studios[QString::fromStdString(studio)];
    }
  }

  if (stats.scored_count > 0) stats.mean_score = scoreSum / stats.scored_count;

  stats.top_genres = topOf(genres, topCount);
  stats.top_studios = topOf(studios, topCount);

  // Activity
  const auto firstDay = today.addDays(-(kActivityDays - 1));
  QSet<QDate> watchedDays;
  for (const auto time : watchTimes) {
    const auto date = QDateTime::fromSecsSinceEpoch(time).date();
    if (!date.isValid() || date > today) continue;
    watchedDays.insert(date);
    if (date >= firstDay) ++stats.activity[date];
  }
  stats.active_days = static_cast<int>(stats.activity.size());

  // Streaks, over all history. Today not having been watched yet doesn't break a streak.
  auto day = watchedDays.contains(today) ? today : today.addDays(-1);
  while (watchedDays.contains(day)) {
    ++stats.current_streak;
    day = day.addDays(-1);
  }

  auto days = QList<QDate>(watchedDays.begin(), watchedDays.end());
  std::ranges::sort(days);
  int run = 0;
  for (qsizetype i = 0; i < days.size(); ++i) {
    run = (i > 0 && days[i - 1].daysTo(days[i]) == 1) ? run + 1 : 1;
    stats.longest_streak = std::max(stats.longest_streak, run);
  }

  return stats;
}

}  // namespace anime::stats
