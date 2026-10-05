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
#include <QSignalSpy>
#include <QTest>

#include "media/list_undo.hpp"

using namespace anime::list;

namespace {

Entry makeEntry(int id, Status status, int watched, int score = 0) {
  Entry entry;
  entry.anime_id = id;
  entry.status = status;
  entry.watched_episodes = watched;
  entry.score = score;
  entry.last_updated = 0;
  return entry;
}

Change makeChange(int id, std::optional<Entry> previous, Entry next) {
  return {.anime_id = id, .previous = std::move(previous), .next = std::move(next)};
}

}  // namespace

class TestListUndo final : public QObject {
  Q_OBJECT

private slots:
  void summarizesAddedAnime() {
    QVERIFY(summarize(makeChange(1, std::nullopt, makeEntry(1, Status::PlanToWatch, 0))).added);

    auto removed = makeEntry(1, Status::Watching, 3);
    removed.pending_delete = true;
    QVERIFY(summarize(makeChange(1, removed, makeEntry(1, Status::Watching, 3))).added);
  }

  void summarizesChangedFields() {
    const auto summary = summarize(
        makeChange(1, makeEntry(1, Status::Watching, 11), makeEntry(1, Status::Completed, 12, 80)));
    QVERIFY(!summary.added);
    QVERIFY(summary.fields.testFlag(Field::Episode));
    QVERIFY(summary.fields.testFlag(Field::Status));
    QVERIFY(summary.fields.testFlag(Field::Score));
    QVERIFY(!summary.fields.testFlag(Field::Notes));
  }

  void groupsChangesMadeTogether() {
    UndoStack stack;
    QSignalSpy spy(&stack, &UndoStack::recorded);

    stack.record(makeChange(1, makeEntry(1, Status::Watching, 1), makeEntry(1, Status::Dropped, 1)),
                 1000);
    stack.record(makeChange(2, makeEntry(2, Status::Watching, 1), makeEntry(2, Status::Dropped, 1)),
                 1500);
    stack.record(makeChange(3, makeEntry(3, Status::Watching, 1), makeEntry(3, Status::Dropped, 1)),
                 5000);  // much later

    QCOMPARE(spy.count(), 3);
    const int first = spy.at(0).at(0).toInt();
    const int last = spy.at(2).at(0).toInt();
    QCOMPARE(spy.at(1).at(0).toInt(), first);
    QVERIFY(last != first);
    QCOMPARE(stack.group(first)->changes.size(), 2);
    QCOMPARE(stack.group(last)->changes.size(), 1);
  }

  void keepsOriginalStateWhenMerging() {
    UndoStack stack;
    QSignalSpy spy(&stack, &UndoStack::recorded);

    auto first =
        makeChange(1, makeEntry(1, Status::Watching, 4), makeEntry(1, Status::Watching, 5));
    first.history_times = {100};
    auto second =
        makeChange(1, makeEntry(1, Status::Watching, 5), makeEntry(1, Status::Watching, 6));
    second.history_times = {101};
    stack.record(first, 0);
    stack.record(second, 200);

    const auto group = stack.group(spy.at(0).at(0).toInt());
    QCOMPARE(group->changes.size(), 1);
    QCOMPARE(group->changes[0].previous->watched_episodes, 4);
    QCOMPARE(group->changes[0].next.watched_episodes, 6);
    QCOMPARE(group->changes[0].history_times, (QList<std::time_t>{100, 101}));
  }

  void takeRemovesGroup() {
    UndoStack stack;
    QSignalSpy spy(&stack, &UndoStack::recorded);
    stack.record(makeChange(1, std::nullopt, makeEntry(1, Status::Watching, 1)), 0);

    const int id = spy.at(0).at(0).toInt();
    QVERIFY(stack.take(id).has_value());
    QVERIFY(!stack.take(id).has_value());
    QVERIFY(stack.group(id) == nullptr);
  }

  void limitsHistory() {
    UndoStack stack;
    QSignalSpy spy(&stack, &UndoStack::recorded);
    for (int i = 0; i < UndoStack::kMaxGroups + 5; ++i) {
      stack.record(makeChange(i, std::nullopt, makeEntry(i, Status::Watching, 1)),
                   i * 10 * UndoStack::kGroupWindowMs);
    }
    QVERIFY(stack.group(spy.first().at(0).toInt()) == nullptr);  // dropped
    QVERIFY(stack.group(spy.last().at(0).toInt()) != nullptr);
  }

  void guardSuppressesRecording() {
    QVERIFY(!UndoStack::isSuppressed());
    {
      const UndoStack::Guard guard;
      QVERIFY(UndoStack::isSuppressed());
      {
        const UndoStack::Guard nested;
        QVERIFY(UndoStack::isSuppressed());
      }
      QVERIFY(UndoStack::isSuppressed());
    }
    QVERIFY(!UndoStack::isSuppressed());
  }
};

QTEST_APPLESS_MAIN(TestListUndo)
#include "test_list_undo.moc"
