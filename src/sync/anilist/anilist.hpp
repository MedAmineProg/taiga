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
#include <QList>
#include <functional>

#include "sync/service.hpp"

namespace taiga::sync::anilist {

class Service final : public taiga::sync::Service {
public:
  Service();
  ~Service() = default;

  static Service* instance();

  void authenticateUser();
  void fetchAnime(const int id);
  void search(const SearchParams& params, const int page = 1);
  void fetchListEntries();
  void addListEntry(const int id, const anime::list::Fields dirty);
  void deleteListEntry(const int id);
  void updateListEntry(const int id, const anime::list::Fields dirty);

  // These take the entry to send (with AniList IDs) instead of reading it from the database.
  void saveEntry(const ListEntry& entry, const anime::list::Fields dirty, EntryCallback done);
  void deleteEntry(const int64_t entryId, EntryCallback done);
  void findEntryId(const int mediaId, std::function<void(bool success, int64_t entryId)> done);

  // Maps up to 50 AniList IDs to MyAnimeList IDs, or MyAnimeList IDs to AniList IDs. The result
  // is keyed by the given IDs; IDs without a match are left out.
  void mapIds(const QList<int>& ids, const bool fromMal,
              std::function<void(bool success, QHash<int, int> ids)> done);
};

}  // namespace taiga::sync::anilist
