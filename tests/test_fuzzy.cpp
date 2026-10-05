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

#include "gui/utils/fuzzy.hpp"

using gui::fuzzyScore;

class TestFuzzy final : public QObject {
  Q_OBJECT

private slots:
  void emptyQueryMatchesEverything() {
    QVERIFY(fuzzyScore(u"", u"Anything").has_value());
    QVERIFY(fuzzyScore(u"   ", u"Anything").has_value());
  }

  void matchesSubsequence() {
    QVERIFY(fuzzyScore(u"fma", u"Fullmetal Alchemist").has_value());
    QVERIFY(fuzzyScore(u"sgate", u"Steins;Gate").has_value());
    QVERIFY(!fuzzyScore(u"amf", u"Fullmetal Alchemist").has_value());
    QVERIFY(!fuzzyScore(u"xyz", u"Fullmetal Alchemist").has_value());
  }

  void isCaseInsensitive() {
    QCOMPARE(fuzzyScore(u"NARUTO", u"naruto"), fuzzyScore(u"naruto", u"naruto"));
  }

  void ignoresSpacesInQuery() {
    QVERIFY(fuzzyScore(u"one piece", u"OnePiece").has_value());
  }

  void prefersPrefixAndWordStarts() {
    // "ev" at the start of a word beats "ev" inside one
    QVERIFY(*fuzzyScore(u"ev", u"Evangelion") > *fuzzyScore(u"ev", u"Clevatess"));
    QVERIFY(*fuzzyScore(u"ga", u"Steins;Gate") > *fuzzyScore(u"ga", u"Mangaka"));
  }

  void prefersConsecutiveMatches() {
    QVERIFY(*fuzzyScore(u"sync", u"Synchronize") > *fuzzyScore(u"sync", u"Show your nice cat"));
  }

  void prefersShorterCandidates() {
    QVERIFY(*fuzzyScore(u"death", u"Death Note") >
            *fuzzyScore(u"death", u"Death Note: Rewrite - Genshisuru Kami"));
  }
};

QTEST_APPLESS_MAIN(TestFuzzy)
#include "test_fuzzy.moc"
