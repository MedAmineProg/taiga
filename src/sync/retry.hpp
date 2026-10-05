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
#pragma once

#include <algorithm>
#include <chrono>

namespace sync {

// Delay before retrying a failed request, doubling after each consecutive failure:
// 30s, 1m, 2m, 4m... up to 6h.
constexpr std::chrono::seconds retryDelay(const int retryCount) {
  constexpr std::chrono::seconds kBase{30};
  constexpr std::chrono::seconds kMax{std::chrono::hours{6}};
  const int exponent = std::clamp(retryCount - 1, 0, 16);
  return std::min<std::chrono::seconds>(kBase * (1 << exponent), kMax);
}

}  // namespace sync
