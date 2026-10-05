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

#include <QString>
#include <ctime>
#include <optional>
#include <utility>

#include "link/discord_frames.hpp"

namespace taiga {

// What's playing, as far as Discord is concerned.
struct NowPlaying {
  QString title;
  std::optional<std::pair<int, int>> episodes;  // first and last, for batch files
  int episodeCount = 0;                         // 0 if unknown
  QString group;                                // release group
  QString imageUrl;
  std::time_t start = 0;
};

struct PresenceOptions {
  bool showButton = true;
  bool showGroup = true;
  bool showTime = true;
  bool showUsername = true;
};

struct ServiceInfo {
  QString slug;  // matches the image assets of Taiga's Discord application
  QString name;
  QString username;
  QString animePageUrl;
};

discord::Presence makePresence(const NowPlaying& playing, const PresenceOptions& options,
                               const ServiceInfo& service);

}  // namespace taiga
