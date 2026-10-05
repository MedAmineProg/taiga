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
#include "discord_presence.hpp"

#include "base/log.hpp"
#include "base/string.hpp"
#include "link/discord_ipc.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "taiga/accounts.hpp"
#include "taiga/settings.hpp"
#include "track/episode.hpp"
#include "track/media.hpp"

namespace taiga {

void DiscordPresence::init() {
  client_ = new discord::Client(this);
  connect(client_, &discord::Client::ready, this, [] { qDebug() << "Discord: ready"; });
  connect(client_, &discord::Client::disconnected, this,
          [] { qDebug() << "Discord: disconnected"; });

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &DiscordPresence::update);

  update();
}

void DiscordPresence::update() {
  if (!client_) return;

  if (!settings.sharingEnabled() || !settings.discordEnabled()) {
    client_->stop();
    return;
  }

  client_->setClientId(settings.discordApplicationId());
  client_->start();

  const auto episode = track::media::detection()->getCurrentEpisode();
  const auto item = episode ? anime::db.item(episode->animeId()) : nullptr;
  const auto entry = item ? anime::db.entry(item->id) : nullptr;

  // Private entries aren't shared.
  if (!item || (entry && entry->is_private)) {
    currentKey_ = {};
    client_->setPresence(std::nullopt);
    return;
  }

  const auto range = episode->episodeNumberRange();
  const std::pair key{item->id, range ? range->second : 0};
  if (key != currentKey_) {
    currentKey_ = key;
    start_ = std::time(nullptr);
  }

  const auto serviceId = sync::currentServiceId();
  const auto slug = sync::serviceSlug(serviceId);

  client_->setPresence(makePresence(
      {
          .title = QString::fromStdString(anime::preferredTitle(*item)),
          .episodes = range,
          .episodeCount = std::max(item->episode_count, 0),
          .group = QString::fromStdString(episode->element(anitomy::ElementKind::ReleaseGroup)),
          .imageUrl = QString::fromStdString(item->image_url),
          .start = start_,
      },
      {
          .showButton = settings.discordShowButton(),
          .showGroup = settings.discordShowGroup(),
          .showTime = settings.discordShowTime(),
          .showUsername = settings.discordShowUsername(),
      },
      {
          .slug = slug,
          .name = sync::serviceName(serviceId),
          .username = QString::fromStdString(accounts.serviceUsername(slug.toStdString())),
          .animePageUrl = sync::animePageUrl(item->id),
      }));
}

DiscordPresence* discordPresence() {
  static auto presence = new DiscordPresence();
  return presence;
}

}  // namespace taiga
