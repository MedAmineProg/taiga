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
#include "fuzzy.hpp"

namespace gui {

namespace {

bool isWordStart(QStringView text, const qsizetype i) {
  if (i == 0) return true;
  const auto prev = text[i - 1];
  const auto curr = text[i];
  if (!prev.isLetterOrNumber()) return true;
  return prev.isLower() && curr.isUpper();  // camelCase
}

}  // namespace

std::optional<int> fuzzyScore(QStringView query, QStringView text) {
  constexpr int kMatch = 1;
  constexpr int kConsecutive = 5;
  constexpr int kWordStart = 8;
  constexpr int kPrefix = 10;
  constexpr int kGapPenalty = 1;

  query = query.trimmed();
  if (query.isEmpty()) return 0;

  int score = 0;
  qsizetype q = 0;
  qsizetype last = -1;

  for (qsizetype i = 0; i < text.size() && q < query.size(); ++i) {
    if (query[q].isSpace()) {
      ++q;  // spaces in the query match anything
      --i;
      continue;
    }
    if (text[i].toCaseFolded() != query[q].toCaseFolded()) continue;

    score += kMatch;
    if (i == 0) score += kPrefix;
    if (isWordStart(text, i)) score += kWordStart;
    if (last >= 0) {
      if (i == last + 1) {
        score += kConsecutive;
      } else {
        score -= kGapPenalty * static_cast<int>(std::min<qsizetype>(i - last - 1, 5));
      }
    }

    last = i;
    ++q;
  }

  if (q < query.size()) return std::nullopt;

  // Prefer shorter candidates when everything else is equal.
  return score * 100 - static_cast<int>(std::min<qsizetype>(text.size(), 99));
}

}  // namespace gui
