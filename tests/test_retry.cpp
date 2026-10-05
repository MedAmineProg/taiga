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

#include "sync/retry.hpp"

using namespace std::chrono_literals;

class TestRetry final : public QObject {
  Q_OBJECT

private slots:
  void doublesAfterEachFailure() {
    QCOMPARE(taiga::sync::retryDelay(1), 30s);
    QCOMPARE(taiga::sync::retryDelay(2), 60s);
    QCOMPARE(taiga::sync::retryDelay(3), 120s);
    QCOMPARE(taiga::sync::retryDelay(5), 480s);
  }

  void isCapped() {
    QCOMPARE(taiga::sync::retryDelay(10), 15360s);  // 4h16m
    QCOMPARE(taiga::sync::retryDelay(11), std::chrono::seconds{6h});
    QCOMPARE(taiga::sync::retryDelay(1000), std::chrono::seconds{6h});
  }

  void handlesNonPositiveCounts() {
    QCOMPARE(taiga::sync::retryDelay(0), 30s);
    QCOMPARE(taiga::sync::retryDelay(-1), 30s);
  }
};

QTEST_APPLESS_MAIN(TestRetry)
#include "test_retry.moc"
