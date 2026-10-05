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
#include "http.hpp"

#include <QString>

#include "base/string.hpp"

namespace base {

std::optional<QDateTime> parseRetryAfter(QByteArrayView value, const QDateTime& now) {
  value = value.trimmed();
  if (value.isEmpty()) return std::nullopt;

  bool ok = false;
  if (const auto seconds = value.toLongLong(&ok); ok) {
    if (seconds < 0) return std::nullopt;
    return now.addSecs(seconds);
  }

  // HTTP dates always end with "GMT", which Qt's RFC 2822 parser doesn't accept.
  auto text = QString::fromLatin1(value);
  if (text.endsWith(u" GMT")) text.replace(text.size() - 3, 3, u"+0000"_s);

  if (const auto date = QDateTime::fromString(text, Qt::RFC2822Date); date.isValid()) {
    return date.toUTC();
  }

  return std::nullopt;
}

}  // namespace base
