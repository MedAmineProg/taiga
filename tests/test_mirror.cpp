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

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "sync/id_parsers.hpp"
#include "sync/mirror_queue.hpp"

using anime::list::Field;
using namespace taiga::sync;

namespace {

QJsonObject json(const char* text) {
  return QJsonDocument::fromJson(text).object();
}

}  // namespace

class TestMirror : public QObject {
  Q_OBJECT

private slots:
  void addsAndMergesChanges() {
    QList<MirrorItem> items;
    mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 100);
    mergeMirrorItem(items, "kitsu", 1, Field::Episode, false, 100);
    mergeMirrorItem(items, "anilist", 1, Field::Score, false, 110);

    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].dirty, Field::Episode | Field::Score);
    QCOMPARE(items[0].time, 110);
    QCOMPARE(items[1].dirty, anime::list::Fields{Field::Episode});
  }

  void removalReplacesChanges() {
    QList<MirrorItem> items;
    mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 100);
    mergeMirrorItem(items, "anilist", 1, {}, true, 110);

    QCOMPARE(items.size(), 1);
    QVERIFY(items[0].remove);
    QCOMPARE(items[0].dirty, anime::list::Fields{});

    // Added back before the removal was sent: everything has to be sent again.
    mergeMirrorItem(items, "anilist", 1, Field::Score, false, 120);
    QVERIFY(!items[0].remove);
    QCOMPARE(items[0].dirty, kAllListFields);
  }

  void newChangeResetsFailures() {
    QList<MirrorItem> items;
    auto& item = mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 100);
    failMirrorItem(item, "Offline", false, 100);
    QCOMPARE(items[0].retry_count, 1);
    QVERIFY(items[0].next_attempt > 100);

    mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 200);
    QCOMPARE(items[0].retry_count, 0);
    QCOMPARE(items[0].next_attempt, 0);
    QVERIFY(items[0].last_error.isEmpty());
  }

  void picksOldestReadyItem() {
    QList<MirrorItem> items;
    mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 300);
    mergeMirrorItem(items, "anilist", 2, Field::Episode, false, 100);
    mergeMirrorItem(items, "anilist", 3, Field::Episode, false, 200);

    QCOMPARE(nextMirrorItem(items, 1000)->anime_id, 2);

    // Waiting for a retry.
    failMirrorItem(items[1], "Offline", false, 1000);
    QCOMPARE(nextMirrorItem(items, 1000)->anime_id, 3);
    QCOMPARE(nextMirrorAttempt(items), 0);  // the others can go now

    // Given up.
    failMirrorItem(items[0], "Not found", true, 1000);
    failMirrorItem(items[2], "Not found", true, 1000);
    QVERIFY(items[0].gaveUp());
    QCOMPARE(nextMirrorItem(items, 1000), nullptr);
    QCOMPARE(nextMirrorAttempt(items), items[1].next_attempt);
  }

  void retriesWithBackoffUntilGivingUp() {
    QList<MirrorItem> items;
    auto& item = mergeMirrorItem(items, "kitsu", 1, Field::Episode, false, 0);
    std::time_t previousDelay = 0;
    for (int i = 1; i < kMirrorMaxAttempts; ++i) {
      failMirrorItem(item, "Offline", false, 0);
      QVERIFY(!item.gaveUp());
      QVERIFY(item.next_attempt > previousDelay);
      previousDelay = item.next_attempt;
    }
    failMirrorItem(item, "Offline", false, 0);
    QVERIFY(item.gaveUp());
  }

  void summarizesStatus() {
    QList<MirrorItem> items;
    mergeMirrorItem(items, "anilist", 1, Field::Episode, false, 100);
    mergeMirrorItem(items, "anilist", 2, Field::Episode, false, 200);
    mergeMirrorItem(items, "kitsu", 3, Field::Episode, false, 300);
    failMirrorItem(items[1], "This anime isn't on AniList.", true, 200);

    const auto status = mirrorStatus(items, "anilist");
    QCOMPARE(status.pending, 1);
    QCOMPARE(status.failed, 1);
    QCOMPARE(status.lastError, "This anime isn't on AniList.");
    QCOMPARE(mirrorStatus(items, "myanimelist").pending, 0);
  }

  void rechecksMissingMappingsLater() {
    constexpr std::time_t kDay = 24 * 60 * 60;
    QVERIFY(isMappingFresh(true, 0, 1000 * kDay));
    QVERIFY(isMappingFresh(false, 0, 6 * kDay));
    QVERIFY(!isMappingFresh(false, 0, 8 * kDay));
  }

  void parsesAnilistIds() {
    const auto root = json(R"({"data": {"Page": {"media": [
        {"id": 21, "idMal": 1},
        {"id": 154587, "idMal": 52991},
        {"id": 999, "idMal": null}
    ]}}})");

    const auto toMal = anilist::parseIdMappings(root, false);
    QCOMPARE(toMal.size(), 2);
    QCOMPARE(toMal.value(154587), 52991);

    const auto fromMal = anilist::parseIdMappings(root, true);
    QCOMPARE(fromMal.value(52991), 154587);
    QVERIFY(!fromMal.contains(0));

    QCOMPARE(anilist::parseMediaListId(json(R"({"data": {"MediaList": {"id": 412345678}}})")),
             412345678);
    QCOMPARE(anilist::parseMediaListId(json(R"({"data": {"MediaList": null}})")), 0);
  }

  void parsesKitsuMappings() {
    const auto mappings = json(R"({"data": [
        {"id": "1", "type": "mappings",
         "attributes": {"externalSite": "anidb", "externalId": "4563"}},
        {"id": "2", "type": "mappings",
         "attributes": {"externalSite": "myanimelist/anime", "externalId": "52991"}}
    ]})");
    QCOMPARE(kitsu::parseMalIdFromMappings(mappings), 52991);
    QCOMPARE(kitsu::parseMalIdFromMappings(json(R"({"data": []})")), 0);

    const auto search = json(R"({"data": [
        {"id": "9", "type": "mappings",
         "relationships": {"item": {"data": {"type": "anime", "id": "46474"}}}}
    ]})");
    QCOMPARE(kitsu::parseAnimeIdFromMappings(search), 46474);
    QCOMPARE(kitsu::parseAnimeIdFromMappings(json(R"({"data": []})")), 0);

    QCOMPARE(kitsu::parseFirstResourceId(json(R"({"data": [{"id": "73123456"}]})")), 73123456);
    QCOMPARE(kitsu::parseFirstResourceId(json(R"({"data": []})")), 0);
  }
};

QTEST_GUILESS_MAIN(TestMirror)
#include "test_mirror.moc"
