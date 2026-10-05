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
#include <QTest>

#include "media/airing_schedule.hpp"

using namespace anime;

namespace {

constexpr std::time_t kDay = 24 * 60 * 60;
constexpr std::time_t kWeek = 7 * kDay;

Details makeItem(int id, int episodeCount, int lastAired, std::time_t nextEpisode) {
  Details item;
  item.id = id;
  item.episode_count = episodeCount;
  item.last_aired_episode = lastAired;
  item.next_episode_time = nextEpisode;
  return item;
}

list::Entry makeEntry(int id, list::Status status) {
  list::Entry entry;
  entry.anime_id = id;
  entry.status = status;
  entry.last_updated = 0;
  return entry;
}

const QSet<list::Status> kWatchingOrPlanned{list::Status::Watching, list::Status::PlanToWatch};

}  // namespace

class TestAiringSchedule final : public QObject {
  Q_OBJECT

private slots:
  void placesNextEpisodeInItsWeek() {
    const std::time_t weekStart = 1'000'000;
    QMap<int, Details> items{{1, makeItem(1, 12, 4, weekStart + 2 * kDay)}};
    QMap<int, list::Entry> entries{{1, makeEntry(1, list::Status::Watching)}};

    const auto episodes =
        schedule::episodesBetween(items, entries, kWatchingOrPlanned, weekStart, weekStart + kWeek);
    QCOMPARE(episodes.size(), 1u);
    QCOMPARE(episodes[0].number, 5);
    QCOMPARE(episodes[0].time, weekStart + 2 * kDay);
    QVERIFY(!episodes[0].estimated);
  }

  void projectsWeeklyForwardAndBack() {
    const std::time_t next = 10'000'000;
    QMap<int, Details> items{{1, makeItem(1, 0, 4, next)}};  // unknown episode count
    QMap<int, list::Entry> entries{{1, makeEntry(1, list::Status::Watching)}};

    // The week before: episode 4, estimated
    auto episodes = schedule::episodesBetween(items, entries, kWatchingOrPlanned,
                                              next - kWeek - kDay, next - kDay);
    QCOMPARE(episodes.size(), 1u);
    QCOMPARE(episodes[0].number, 4);
    QCOMPARE(episodes[0].time, next - kWeek);
    QVERIFY(episodes[0].estimated);

    // Two weeks later: episode 7
    episodes = schedule::episodesBetween(items, entries, kWatchingOrPlanned, next + 2 * kWeek,
                                         next + 3 * kWeek);
    QCOMPARE(episodes.size(), 1u);
    QCOMPARE(episodes[0].number, 7);
    QVERIFY(episodes[0].estimated);

    // A range spanning three weeks has three episodes
    episodes = schedule::episodesBetween(items, entries, kWatchingOrPlanned, next - kDay,
                                         next + 3 * kWeek - kDay);
    QCOMPARE(episodes.size(), 3u);
  }

  void rangeEndIsExclusive() {
    const std::time_t next = 10'000'000;
    QMap<int, Details> items{{1, makeItem(1, 12, 4, next)}};
    QMap<int, list::Entry> entries{{1, makeEntry(1, list::Status::Watching)}};

    QVERIFY(schedule::episodesBetween(items, entries, kWatchingOrPlanned, next - kWeek + 1, next)
                .empty());
    QCOMPARE(schedule::episodesBetween(items, entries, kWatchingOrPlanned, next, next + 1).size(),
             1u);
  }

  void staysWithinEpisodeCount() {
    const std::time_t next = 10'000'000;
    QMap<int, Details> items{{1, makeItem(1, 12, 11, next)}};  // the finale is next
    QMap<int, list::Entry> entries{{1, makeEntry(1, list::Status::Watching)}};

    const auto later = schedule::episodesBetween(items, entries, kWatchingOrPlanned, next + kDay,
                                                 next + 4 * kWeek);
    QVERIFY(later.empty());

    QMap<int, Details> premiere{{1, makeItem(1, 12, 0, next)}};  // the premiere is next
    const auto earlier = schedule::episodesBetween(premiere, entries, kWatchingOrPlanned,
                                                   next - 4 * kWeek, next - kDay);
    QVERIFY(earlier.empty());
  }

  void filtersByStatusAndSkipsUnknownTimes() {
    const std::time_t next = 10'000'000;
    QMap<int, Details> items{
        {1, makeItem(1, 12, 1, next + 3600)},
        {2, makeItem(2, 12, 1, next + 60)},
        {3, makeItem(3, 12, 1, next + 120)},  // dropped
        {4, makeItem(4, 12, 1, 0)},           // no known time
    };
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching)},
        {2, makeEntry(2, list::Status::PlanToWatch)},
        {3, makeEntry(3, list::Status::Dropped)},
        {4, makeEntry(4, list::Status::Watching)},
    };

    const auto episodes =
        schedule::episodesBetween(items, entries, kWatchingOrPlanned, next, next + kDay);
    QCOMPARE(episodes.size(), 2u);
    QCOMPARE(episodes[0].anime_id, 2);  // sorted by time
    QCOMPARE(episodes[1].anime_id, 1);

    const auto watching =
        schedule::episodesBetween(items, entries, {list::Status::Watching}, next, next + kDay);
    QCOMPARE(watching.size(), 1u);
    QCOMPARE(watching[0].anime_id, 1);
  }

  void airedBetweenUsesKnownTimesOnly() {
    const std::time_t now = 10'000'000;
    QMap<int, Details> items{
        {1, makeItem(1, 12, 4, now - 60)},     // aired a minute ago
        {2, makeItem(2, 12, 4, now + 60)},     // not yet
        {3, makeItem(3, 12, 4, now - kWeek)},  // long ago
    };
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching)},
        {2, makeEntry(2, list::Status::Watching)},
        {3, makeEntry(3, list::Status::Watching)},
    };

    const auto aired =
        schedule::airedBetween(items, entries, {list::Status::Watching}, now - 300, now);
    QCOMPARE(aired.size(), 1u);
    QCOMPARE(aired[0].anime_id, 1);
    QCOMPARE(aired[0].number, 5);
    QVERIFY(!aired[0].estimated);
  }
};

QTEST_APPLESS_MAIN(TestAiringSchedule)
#include "test_airing_schedule.moc"
