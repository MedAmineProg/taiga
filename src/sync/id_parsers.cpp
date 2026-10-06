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

#include "id_parsers.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

namespace taiga::sync::anilist {

int64_t parseMediaListId(const QJsonObject& root) {
  return static_cast<int64_t>(root["data"]["MediaList"]["id"].toInteger());
}

QHash<int, int> parseIdMappings(const QJsonObject& root, const bool fromMal) {
  QHash<int, int> ids;
  for (const auto& value : root["data"]["Page"]["media"].toArray()) {
    const auto media = value.toObject();
    const int id = media["id"].toInt();
    const int malId = media["idMal"].toInt();
    if (!id || !malId) continue;
    if (fromMal) {
      ids.insert(malId, id);
    } else {
      ids.insert(id, malId);
    }
  }
  return ids;
}

}  // namespace taiga::sync::anilist

namespace taiga::sync::kitsu {

int64_t parseFirstResourceId(const QJsonObject& root) {
  const auto data = root["data"].toArray();
  if (data.isEmpty()) return 0;
  return data.first().toObject()["id"].toVariant().toLongLong();
}

int parseMalIdFromMappings(const QJsonObject& root) {
  for (const auto& mapping : root["data"].toArray()) {
    const auto attributes = mapping.toObject().value("attributes").toObject();
    if (attributes["externalSite"].toString() == u"myanimelist/anime") {
      return attributes["externalId"].toVariant().toInt();
    }
  }
  return 0;
}

int parseAnimeIdFromMappings(const QJsonObject& root) {
  for (const auto& mapping : root["data"].toArray()) {
    const auto item = mapping.toObject()
                          .value("relationships")
                          .toObject()
                          .value("item")
                          .toObject()
                          .value("data")
                          .toObject();
    if (item["type"].toString() == u"anime") {
      return item["id"].toVariant().toInt();
    }
  }
  return 0;
}

}  // namespace taiga::sync::kitsu
