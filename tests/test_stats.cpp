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
#include <QDateTime>
#include <QTest>

#include "media/stats.hpp"

using namespace anime;

namespace {

Details makeItem(int id, int episodes, int length, std::vector<std::string> genres = {},
                 std::vector<std::string> studios = {}) {
  Details item;
  item.id = id;
  item.episode_count = episodes;
  item.episode_length = length;
  item.genres = std::move(genres);
  item.studios = std::move(studios);
  return item;
}

list::Entry makeEntry(int id, list::Status status, int watched, int score = 0) {
  list::Entry entry;
  entry.anime_id = id;
  entry.status = status;
  entry.watched_episodes = watched;
  entry.score = score;
  entry.last_updated = 0;
  return entry;
}

std::time_t at(QDate date, int hour = 20) {
  return QDateTime(date, QTime(hour, 0)).toSecsSinceEpoch();
}

}  // namespace

class TestStats final : public QObject {
  Q_OBJECT

private slots:
  void countsAnimeAndStatuses() {
    QMap<int, Details> items{{1, makeItem(1, 12, 24)}, {2, makeItem(2, 12, 24)}};
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 3)},
        {2, makeEntry(2, list::Status::Completed, 12)},
        {3, makeEntry(3, list::Status::PlanToWatch, 0)},
    };
    auto deleted = makeEntry(4, list::Status::Completed, 12);
    deleted.pending_delete = true;
    entries[4] = deleted;

    const auto stats = stats::compute(items, entries, {}, QDate(2026, 10, 5));
    QCOMPARE(stats.anime_count, 3);
    QCOMPARE(stats.status_counts.value(list::Status::Watching), 1);
    QCOMPARE(stats.status_counts.value(list::Status::Completed), 1);
    QCOMPARE(stats.status_counts.value(list::Status::PlanToWatch), 1);
  }

  void countsEpisodesAndTimeIncludingRewatches() {
    QMap<int, Details> items{{1, makeItem(1, 12, 24)}, {2, makeItem(2, 10, 0)}};
    auto rewatched = makeEntry(1, list::Status::Completed, 12);
    rewatched.rewatched_times = 1;
    auto rewatching = makeEntry(2, list::Status::Completed, 10);
    rewatching.rewatching = true;
    rewatching.rewatching_ep = 4;

    const auto stats =
        stats::compute(items, {{1, rewatched}, {2, rewatching}}, {}, QDate(2026, 10, 5));
    QCOMPARE(stats.episodes_watched, 24 + 14);
    // Unknown length (the second anime) counts as 24 minutes.
    QCOMPARE(stats.minutes_watched, 24 * 24 + 14 * stats::kAssumedEpisodeLength);
  }

  void computesScores() {
    QMap<int, Details> items;
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Completed, 1, 80)},
        {2, makeEntry(2, list::Status::Completed, 1, 90)},
        {3, makeEntry(3, list::Status::Completed, 1, 100)},
        {4, makeEntry(4, list::Status::Completed, 1, 0)},  // not scored
        {5, makeEntry(5, list::Status::Completed, 1, 5)},
    };

    const auto stats = stats::compute(items, entries, {}, QDate(2026, 10, 5));
    QCOMPARE(stats.scored_count, 4);
    QCOMPARE(stats.mean_score, (80 + 90 + 100 + 5) / 4.0);
    QCOMPARE(stats.score_buckets[0], 1);  // 5
    QCOMPARE(stats.score_buckets[7], 1);  // 80
    QCOMPARE(stats.score_buckets[8], 1);  // 90
    QCOMPARE(stats.score_buckets[9], 1);  // 100
  }

  void ranksGenresAndStudiosOfStartedAnime() {
    QMap<int, Details> items{
        {1, makeItem(1, 12, 24, {"Action", "Drama"}, {"Bones"})},
        {2, makeItem(2, 12, 24, {"Action", "Comedy"}, {"Bones"})},
        {3, makeItem(3, 12, 24, {"Romance"}, {"Kyoto Animation"})},  // only planned
    };
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Completed, 12)},
        {2, makeEntry(2, list::Status::Watching, 2)},
        {3, makeEntry(3, list::Status::PlanToWatch, 0)},
    };

    const auto stats = stats::compute(items, entries, {}, QDate(2026, 10, 5), 2);
    QCOMPARE(stats.top_genres.size(), 2u);
    QCOMPARE(stats.top_genres[0], std::make_pair(std::string{"Action"}, 2));
    QCOMPARE(stats.top_genres[1], std::make_pair(std::string{"Comedy"}, 1));  // ties by name
    QCOMPARE(stats.top_studios.size(), 1u);
    QCOMPARE(stats.top_studios[0], std::make_pair(std::string{"Bones"}, 2));
  }

  void tracksActivityAndStreaks() {
    const QDate today(2026, 10, 5);
    const QList<std::time_t> times{
        // A 3-day streak ending yesterday, with two episodes on one day
        at(today.addDays(-1)),
        at(today.addDays(-2)),
        at(today.addDays(-2), 21),
        at(today.addDays(-3)),
        // A 4-day streak last month
        at(today.addDays(-30)),
        at(today.addDays(-31)),
        at(today.addDays(-32)),
        at(today.addDays(-33)),
        // Outside the activity window
        at(today.addDays(-400)),
    };

    const auto stats = stats::compute({}, {}, times, today);
    QCOMPARE(stats.current_streak, 3);
    QCOMPARE(stats.longest_streak, 4);
    QCOMPARE(stats.active_days, 7);
    QCOMPARE(stats.activity.value(today.addDays(-2)), 2);
    QVERIFY(!stats.activity.contains(today.addDays(-400)));
  }

  void streakIncludesToday() {
    const QDate today(2026, 10, 5);
    const auto stats = stats::compute({}, {}, {at(today, 9), at(today.addDays(-1))}, today);
    QCOMPARE(stats.current_streak, 2);
  }

  void brokenStreakIsZero() {
    const QDate today(2026, 10, 5);
    const auto stats = stats::compute({}, {}, {at(today.addDays(-2))}, today);
    QCOMPARE(stats.current_streak, 0);
    QCOMPARE(stats.longest_streak, 1);
  }
};

QTEST_APPLESS_MAIN(TestStats)
#include "test_stats.moc"
