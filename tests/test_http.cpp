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

#include "base/http.hpp"

using base::parseRetryAfter;

class TestHttp final : public QObject {
  Q_OBJECT

private slots:
  void parsesSeconds() {
    const auto now = QDateTime::currentDateTimeUtc();
    QCOMPARE(parseRetryAfter("120", now), now.addSecs(120));
    QCOMPARE(parseRetryAfter("  0 ", now), now);
  }

  void parsesHttpDate() {
    const auto now = QDateTime::currentDateTimeUtc();
    const auto date = parseRetryAfter("Wed, 21 Oct 2015 07:28:00 GMT", now);
    QVERIFY(date.has_value());
    QCOMPARE(*date, QDateTime(QDate(2015, 10, 21), QTime(7, 28), QTimeZone::UTC));

    const auto withOffset = parseRetryAfter("Wed, 21 Oct 2015 09:28:00 +0200", now);
    QVERIFY(withOffset.has_value());
    QCOMPARE(*withOffset, *date);
  }

  void rejectsInvalidValues() {
    const auto now = QDateTime::currentDateTimeUtc();
    QVERIFY(!parseRetryAfter("", now).has_value());
    QVERIFY(!parseRetryAfter("-5", now).has_value());
    QVERIFY(!parseRetryAfter("soon", now).has_value());
  }
};

QTEST_APPLESS_MAIN(TestHttp)
#include "test_http.moc"
