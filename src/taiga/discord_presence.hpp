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

#include <QObject>
#include <QString>
#include <ctime>
#include <optional>
#include <utility>

#include "taiga/discord_activity.hpp"

namespace discord {
class Client;
}

namespace taiga {

// Shows the anime being watched on the user's Discord profile.
class DiscordPresence final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(DiscordPresence)

public:
  DiscordPresence() = default;
  ~DiscordPresence() override = default;

  void init();

public slots:
  // Applies settings and the current episode.
  void update();

private:
  discord::Client* client_ = nullptr;
  std::pair<int, int> currentKey_{};  // anime ID and episode, to keep the start time
  std::time_t start_ = 0;
};

DiscordPresence* discordPresence();

}  // namespace taiga
