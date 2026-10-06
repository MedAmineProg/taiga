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
#include <QObject>
#include <ctime>
#include <functional>
#include <optional>

#include "sync/service.hpp"

namespace taiga::sync {

// Maps anime IDs between services. Every service links its anime to MyAnimeList, so IDs are
// mapped through MyAnimeList IDs. Results are cached in the database.
class IdMapper final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IdMapper)

public:
  IdMapper() = default;
  ~IdMapper() = default;

  void init();

  // The ID of the same anime on `to`: 0 if it doesn't exist there, nullopt if it isn't known yet.
  std::optional<int> cached(const ServiceId from, const int id, const ServiceId to) const;

  // Looks up the IDs that aren't known yet. `done` is called once they are, or with false if a
  // lookup failed.
  void resolve(const ServiceId from, const QList<int>& ids, const ServiceId to,
               std::function<void(bool success)> done);

private:
  struct Key {
    ServiceId from;
    int id;
    ServiceId to;

    bool operator==(const Key&) const = default;
  };
  friend size_t qHash(const Key& key, size_t seed) {
    return qHashMulti(seed, static_cast<int>(key.from), key.id, static_cast<int>(key.to));
  }

  struct Mapping {
    int id = 0;
    std::time_t checked = 0;
  };

  std::optional<int> lookup(const ServiceId from, const int id, const ServiceId to) const;
  void store(const ServiceId from, const QHash<int, int>& found, const QList<int>& requested,
             const ServiceId to);

  QList<int> missing(const ServiceId from, const QList<int>& ids, const ServiceId to) const;
  void resolveDirect(const ServiceId from, const QList<int>& ids, const ServiceId to,
                     std::function<void(bool success)> done);

  QHash<Key, Mapping> mappings_;
};

inline IdMapper idMapper;

}  // namespace taiga::sync
