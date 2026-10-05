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
#include "discord_activity.hpp"

#include <QObject>

#include "base/string.hpp"

namespace taiga {

discord::Presence makePresence(const NowPlaying& playing, const PresenceOptions& options,
                               const ServiceInfo& service) {
  discord::Presence presence;
  presence.details = playing.title;

  if (playing.episodes) {
    const auto [first, last] = *playing.episodes;
    const auto number = first == last ? QString::number(first) : u"%1-%2"_s.arg(first).arg(last);
    presence.state = playing.episodeCount > 0
                         ? QObject::tr("Episode %1 of %2").arg(number).arg(playing.episodeCount)
                         : QObject::tr("Episode %1").arg(number);
  }
  if (options.showGroup && !playing.group.isEmpty()) {
    presence.state = presence.state.isEmpty()
                         ? QObject::tr("by %1").arg(playing.group)
                         : QObject::tr("%1 by %2").arg(presence.state).arg(playing.group);
  }

  // Discord can show posters from HTTPS URLs. Otherwise, use the app's own image.
  presence.largeImage = playing.imageUrl.startsWith(u"https://") ? playing.imageUrl : u"default"_s;
  presence.largeText = playing.title;

  presence.smallImage = service.slug;
  presence.smallText = options.showUsername && !service.username.isEmpty()
                           ? QObject::tr("%1 at %2").arg(service.username).arg(service.name)
                           : service.name;

  if (options.showTime) presence.start = playing.start;

  if (options.showButton && !service.animePageUrl.isEmpty()) {
    presence.buttonLabel = QObject::tr("View anime");
    presence.buttonUrl = service.animePageUrl;
  }

  return presence;
}

}  // namespace taiga
