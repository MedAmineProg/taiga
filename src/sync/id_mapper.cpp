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

#include "id_mapper.hpp"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <chrono>

#include "sync/anilist/anilist.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/mirror_queue.hpp"

namespace taiga::sync {

namespace {

// AniList accepts up to 50 IDs per page.
constexpr int kAnilistBatchSize = 50;

// Kitsu takes one request per anime, so space them out.
constexpr std::chrono::milliseconds kKitsuLookupDelay{1000};

}  // namespace

void IdMapper::init() {
  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  QSqlQuery q{db};
  q.exec(
      "CREATE TABLE IF NOT EXISTS id_map("
      "from_service TEXT NOT NULL, from_id INTEGER NOT NULL, to_service TEXT NOT NULL, "
      "to_id INTEGER NOT NULL, checked INTEGER NOT NULL, "
      "PRIMARY KEY (from_service, from_id, to_service))");

  if (q.exec("SELECT * FROM id_map")) {
    while (q.next()) {
      const Key key{
          .from = serviceIdFromSlug(q.value("from_service").toString()),
          .id = q.value("from_id").toInt(),
          .to = serviceIdFromSlug(q.value("to_service").toString()),
      };
      mappings_.insert(key,
                       Mapping{
                           .id = q.value("to_id").toInt(),
                           .checked = static_cast<std::time_t>(q.value("checked").toLongLong()),
                       });
    }
  }

  db.close();
}

std::optional<int> IdMapper::cached(const ServiceId from, const int id, const ServiceId to) const {
  if (from == to) return id;
  if (from == ServiceId::MyAnimeList || to == ServiceId::MyAnimeList) return lookup(from, id, to);

  const auto malId = lookup(from, id, ServiceId::MyAnimeList);
  if (!malId || *malId == 0) return malId;
  return lookup(ServiceId::MyAnimeList, *malId, to);
}

std::optional<int> IdMapper::lookup(const ServiceId from, const int id, const ServiceId to) const {
  const auto it = mappings_.constFind(Key{from, id, to});
  if (it == mappings_.cend()) return std::nullopt;
  if (!isMappingFresh(it->id != 0, it->checked, std::time(nullptr))) return std::nullopt;
  return it->id;
}

QList<int> IdMapper::missing(const ServiceId from, const QList<int>& ids,
                             const ServiceId to) const {
  QList<int> result;
  for (const auto id : ids) {
    if (!lookup(from, id, to) && !result.contains(id)) result.append(id);
  }
  return result;
}

void IdMapper::store(const ServiceId from, const QHash<int, int>& found,
                     const QList<int>& requested, const ServiceId to) {
  const auto now = std::time(nullptr);

  auto db = QSqlDatabase::database();
  const bool open = db.open();
  if (open) db.transaction();

  for (const auto id : requested) {
    const Mapping mapping{.id = found.value(id, 0), .checked = now};
    mappings_.insert(Key{from, id, to}, mapping);

    if (!open) continue;
    QSqlQuery q{db};
    q.prepare(
        "INSERT OR REPLACE INTO id_map(from_service, from_id, to_service, to_id, checked) "
        "VALUES(:from_service, :from_id, :to_service, :to_id, :checked)");
    q.bindValue(":from_service", serviceSlug(from));
    q.bindValue(":from_id", id);
    q.bindValue(":to_service", serviceSlug(to));
    q.bindValue(":to_id", mapping.id);
    q.bindValue(":checked", static_cast<qlonglong>(now));
    q.exec();
  }

  if (open) {
    db.commit();
    db.close();
  }
}

void IdMapper::resolve(const ServiceId from, const QList<int>& ids, const ServiceId to,
                       std::function<void(bool)> done) {
  if (from == to || from == ServiceId::MyAnimeList || to == ServiceId::MyAnimeList) {
    resolveDirect(from, ids, to, std::move(done));
    return;
  }

  // Through MyAnimeList: first find the MyAnimeList IDs, then map those to the target service.
  resolveDirect(from, ids, ServiceId::MyAnimeList, [this, from, ids, to, done](bool success) {
    if (!success) {
      done(false);
      return;
    }
    QList<int> malIds;
    for (const auto id : ids) {
      if (const auto malId = lookup(from, id, ServiceId::MyAnimeList); malId && *malId) {
        malIds.append(*malId);
      }
    }
    resolveDirect(ServiceId::MyAnimeList, malIds, to, done);
  });
}

void IdMapper::resolveDirect(const ServiceId from, const QList<int>& ids, const ServiceId to,
                             std::function<void(bool)> done) {
  const auto pending = missing(from, ids, to);
  if (from == to || pending.isEmpty()) {
    done(true);
    return;
  }

  // AniList knows MyAnimeList IDs and answers for many anime at once.
  if (from == ServiceId::AniList || to == ServiceId::AniList) {
    const bool fromMal = from == ServiceId::MyAnimeList;
    const auto batch = pending.first(std::min<qsizetype>(pending.size(), kAnilistBatchSize));
    anilist::Service::instance()->mapIds(
        batch, fromMal, [this, from, ids, to, batch, done](bool success, QHash<int, int> found) {
          if (!success) {
            done(false);
            return;
          }
          store(from, found, batch, to);
          resolveDirect(from, ids, to, done);  // the next batch, if any
        });
    return;
  }

  // Kitsu, one anime at a time.
  const int id = pending.first();
  const auto next = [this, from, id, ids, to, done](bool success, int mappedId) {
    if (!success) {
      done(false);
      return;
    }
    store(from, mappedId ? QHash<int, int>{{id, mappedId}} : QHash<int, int>{}, {id}, to);
    if (missing(from, ids, to).isEmpty()) {
      done(true);
      return;
    }
    QTimer::singleShot(kKitsuLookupDelay, this,
                       [this, from, ids, to, done] { resolveDirect(from, ids, to, done); });
  };

  if (from == ServiceId::Kitsu) {
    kitsu::Service::instance()->findMalId(id, next);
  } else {
    kitsu::Service::instance()->findIdFromMal(id, next);
  }
}

}  // namespace taiga::sync
