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

#include <QHash>
#include <QJsonObject>
#include <cstdint>

// Parsers for the responses used to map anime IDs between services and to find list entries.

namespace taiga::sync::anilist {

// Reads the result of the MediaListId query (0 if missing).
int64_t parseMediaListId(const QJsonObject& root);

// Reads the result of the MediaIds or MediaIdsByMal query. With `fromMal`, the result maps
// MyAnimeList IDs to AniList IDs; otherwise AniList IDs to MyAnimeList IDs.
QHash<int, int> parseIdMappings(const QJsonObject& root, const bool fromMal);

}  // namespace taiga::sync::anilist

namespace taiga::sync::kitsu {

// Reads the ID of the first resource in a JSON:API collection (0 if empty).
int64_t parseFirstResourceId(const QJsonObject& root);

// Reads the MyAnimeList ID from an anime's mappings (0 if missing).
int parseMalIdFromMappings(const QJsonObject& root);

// Reads the Kitsu anime ID from a mappings search that includes the mapped item (0 if missing).
int parseAnimeIdFromMappings(const QJsonObject& root);

}  // namespace taiga::sync::kitsu
