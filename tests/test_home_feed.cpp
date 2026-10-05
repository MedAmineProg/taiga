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

#include "media/home_feed.hpp"

using namespace anime;
using namespace std::chrono_literals;

namespace {

Details makeItem(int id, int episodeCount, int lastAired = 0, std::time_t nextEpisode = 0) {
  Details item;
  item.id = id;
  item.episode_count = episodeCount;
  item.last_aired_episode = lastAired;
  item.next_episode_time = nextEpisode;
  item.status = lastAired > 0 ? Status::Airing : Status::FinishedAiring;
  return item;
}

list::Entry makeEntry(int id, list::Status status, int watched, std::time_t updated = 0) {
  list::Entry entry;
  entry.anime_id = id;
  entry.status = status;
  entry.watched_episodes = watched;
  entry.last_updated = updated;
  return entry;
}

}  // namespace

class TestHomeFeed final : public QObject {
  Q_OBJECT

private slots:
  void continueWatchingIncludesOnlyUnfinishedWatching() {
    QMap<int, Details> items{
        {1, makeItem(1, 12)}, {2, makeItem(2, 12)}, {3, makeItem(3, 12)}, {4, makeItem(4, 12)}};
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 3)},
        {2, makeEntry(2, list::Status::Completed, 12)},
        {3, makeEntry(3, list::Status::PlanToWatch, 0)},
        {4, makeEntry(4, list::Status::Watching, 12)},  // watched everything
    };

    const auto result = home::continueWatching(items, entries, {}, 10);
    QCOMPARE(result.size(), 1u);
    QCOMPARE(result[0].anime_id, 1);
    QCOMPARE(result[0].next_episode, 4);
  }

  void continueWatchingIncludesRewatching() {
    QMap<int, Details> items{{1, makeItem(1, 12)}};
    auto entry = makeEntry(1, list::Status::Completed, 12);
    entry.rewatching = true;
    entry.rewatching_ep = 5;

    const auto result = home::continueWatching(items, {{1, entry}}, {}, 10);
    QCOMPARE(result.size(), 1u);
    QCOMPARE(result[0].watched_episodes, 5);
    QCOMPARE(result[0].next_episode, 6);
  }

  void continueWatchingPutsAvailableEpisodesFirst() {
    QMap<int, Details> items{{1, makeItem(1, 12)}, {2, makeItem(2, 12)}};
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 3, 2000)},
        {2, makeEntry(2, list::Status::Watching, 5, 1000)},
    };
    const home::AvailableEpisodes available{{2, {4, 5, 6, 7}}};

    const auto result = home::continueWatching(items, entries, available, 10);
    QCOMPARE(result.size(), 2u);
    QCOMPARE(result[0].anime_id, 2);
    QVERIFY(result[0].next_episode_available);
    QCOMPARE(result[0].available_count, 2);  // episodes 6 and 7
    QCOMPARE(result[1].anime_id, 1);
    QVERIFY(!result[1].next_episode_available);
  }

  void continueWatchingSortsByLastUpdatedAndLimits() {
    QMap<int, Details> items{{1, makeItem(1, 12)}, {2, makeItem(2, 12)}, {3, makeItem(3, 12)}};
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 1, 100)},
        {2, makeEntry(2, list::Status::Watching, 1, 300)},
        {3, makeEntry(3, list::Status::Watching, 1, 200)},
    };

    const auto result = home::continueWatching(items, entries, {}, 2);
    QCOMPARE(result.size(), 2u);
    QCOMPARE(result[0].anime_id, 2);
    QCOMPARE(result[1].anime_id, 3);
  }

  void continueWatchingCountsAiredEpisodes() {
    QMap<int, Details> items{{1, makeItem(1, 24, 10)}, {2, makeItem(2, 12)}};
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 7)},
        {2, makeEntry(2, list::Status::Watching, 4)},
    };

    const auto result = home::continueWatching(items, entries, {}, 10);
    QCOMPARE(result.size(), 2u);
    for (const auto& item : result) {
      if (item.anime_id == 1) QCOMPARE(item.aired_unwatched, 3);
      if (item.anime_id == 2) QCOMPARE(item.aired_unwatched, 8);  // finished airing
    }
  }

  void airingSoonFiltersAndSorts() {
    const std::time_t now = 1'000'000;
    QMap<int, Details> items{
        {1, makeItem(1, 12, 3, now + 3600)},       // in an hour
        {2, makeItem(2, 12, 5, now + 600)},        // in ten minutes
        {3, makeItem(3, 12, 1, now - 60)},         // already aired
        {4, makeItem(4, 12, 1, now + 8 * 86400)},  // outside the window
        {5, makeItem(5, 12, 1, now + 1200)},       // dropped
        {6, makeItem(6, 12, 0, now + 1800)},       // planned, not yet aired
    };
    QMap<int, list::Entry> entries{
        {1, makeEntry(1, list::Status::Watching, 3)},
        {2, makeEntry(2, list::Status::Watching, 5)},
        {3, makeEntry(3, list::Status::Watching, 1)},
        {4, makeEntry(4, list::Status::Watching, 1)},
        {5, makeEntry(5, list::Status::Dropped, 1)},
        {6, makeEntry(6, list::Status::PlanToWatch, 0)},
    };

    const auto result = home::airingSoon(items, entries, now, 7 * 24h, 10);
    QCOMPARE(result.size(), 3u);
    QCOMPARE(result[0].anime_id, 2);
    QCOMPARE(result[0].episode, 6);
    QCOMPARE(result[1].anime_id, 6);
    QCOMPARE(result[1].episode, 1);
    QCOMPARE(result[2].anime_id, 1);
  }

  void skipsEntriesPendingDeletion() {
    QMap<int, Details> items{{1, makeItem(1, 12, 3, 2000)}};
    auto entry = makeEntry(1, list::Status::Watching, 3);
    entry.pending_delete = true;

    QVERIFY(home::continueWatching(items, {{1, entry}}, {}, 10).empty());
    QVERIFY(home::airingSoon(items, {{1, entry}}, 1000, 24h, 10).empty());
  }
};

QTEST_APPLESS_MAIN(TestHomeFeed)
#include "test_home_feed.moc"
