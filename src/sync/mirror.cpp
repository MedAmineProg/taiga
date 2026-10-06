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

#include "mirror.hpp"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <algorithm>
#include <chrono>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/id_mapper.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "taiga/network.hpp"
#include "taiga/settings.hpp"

namespace taiga::sync {

namespace {

// Wait for changes to settle, so that edits made in a row are sent together.
constexpr std::chrono::seconds kSettleDelay{10};

// Space out requests, so that copying a whole list doesn't hit rate limits.
constexpr std::chrono::milliseconds kPaceDelay{1500};

// Give up on a request that never completes (e.g. a lost reply).
constexpr std::chrono::seconds kRequestTimeout{90};

// AniList maps up to this many anime per request, so look up the next ones in advance.
constexpr int kMappingBatchSize = 50;

}  // namespace

Mirror::Mirror() : QObject{} {}

void Mirror::init() {
  paceTimer_ = new QTimer(this);
  paceTimer_->setSingleShot(true);
  connect(paceTimer_, &QTimer::timeout, this, &Mirror::process);

  retryTimer_ = new QTimer(this);
  retryTimer_->setSingleShot(true);
  connect(retryTimer_, &QTimer::timeout, this, &Mirror::process);

  watchdogTimer_ = new QTimer(this);
  watchdogTimer_->setSingleShot(true);
  watchdogTimer_->setInterval(kRequestTimeout);
  connect(watchdogTimer_, &QTimer::timeout, this,
          [this] { finish(generation_, false, tr("The request timed out.")); });

  const auto onAuthenticationCompleted = [this](ServiceId service) {
    return [this, service](bool) {
      authenticating_.remove(service);
      emit changed();
    };
  };
  connect(anilist::Service::instance(), &Service::authenticationCompleted, this,
          onAuthenticationCompleted(ServiceId::AniList));
  connect(kitsu::Service::instance(), &Service::authenticationCompleted, this,
          onAuthenticationCompleted(ServiceId::Kitsu));
  connect(myanimelist::Service::instance(), &Service::authenticationCompleted, this,
          onAuthenticationCompleted(ServiceId::MyAnimeList));

  source_ = serviceSlug(currentServiceId());

  createTables();
  readRows();
  idMapper.init();

  if (!items_.isEmpty()) paceTimer_->start(kSettleDelay);
}

QList<ServiceId> Mirror::services() const {
  QList<ServiceId> services;
  const auto current = currentServiceId();
  for (const auto& slug : taiga::settings.mirrorServices()) {
    const auto service = serviceIdFromSlug(QString::fromStdString(slug));
    if (service == ServiceId::Unknown || service == current) continue;
    if (!services.contains(service)) services.append(service);
  }
  return services;
}

void Mirror::push(const int animeId, const anime::list::Fields dirty) {
  ensureSource();

  QList<MirrorItem> changedItems;
  for (const auto service : services()) {
    const auto slug = serviceSlug(service);
    if (processing_ == EntryKey{slug, animeId}) modifiedWhileProcessing_ = true;
    changedItems.append(mergeMirrorItem(items_, slug, animeId, dirty, false, std::time(nullptr)));
  }
  if (changedItems.isEmpty()) return;

  persistItems(changedItems);
  emit changed();

  if (!processing_) paceTimer_->start(kSettleDelay);
}

void Mirror::pushRemove(const int animeId) {
  ensureSource();

  QList<MirrorItem> changedItems;
  for (const auto service : services()) {
    const auto slug = serviceSlug(service);
    if (processing_ == EntryKey{slug, animeId}) modifiedWhileProcessing_ = true;
    changedItems.append(mergeMirrorItem(items_, slug, animeId, {}, true, std::time(nullptr)));
  }
  if (changedItems.isEmpty()) return;

  persistItems(changedItems);
  emit changed();

  if (!processing_) paceTimer_->start(kSettleDelay);
}

void Mirror::dropUnsent(const int animeId) {
  ensureSource();

  QList<MirrorItem> changedItems;
  for (const auto service : services()) {
    const auto slug = serviceSlug(service);
    const EntryKey key{slug, animeId};

    if (entryIds_.contains(key) || processing_ == key) {
      if (processing_ == key) modifiedWhileProcessing_ = true;
      changedItems.append(mergeMirrorItem(items_, slug, animeId, {}, true, std::time(nullptr)));
    } else if (const auto it = findItem(slug, animeId); it != items_.end()) {
      items_.erase(it);
      deleteItem(slug, animeId);
    }
  }

  persistItems(changedItems);
  emit changed();

  if (!changedItems.isEmpty() && !processing_) paceTimer_->start(kSettleDelay);
}

void Mirror::copyAll(const ServiceId service) {
  ensureSource();

  const auto slug = serviceSlug(service);
  const auto now = std::time(nullptr);

  QList<MirrorItem> changedItems;
  for (const auto& entry : anime::db.entries()) {
    if (entry.pending_delete || entry.status == anime::list::Status::NotInList) continue;
    if (processing_ == EntryKey{slug, entry.anime_id}) modifiedWhileProcessing_ = true;
    changedItems.append(mergeMirrorItem(items_, slug, entry.anime_id, kAllListFields, false, now));
  }

  persistItems(changedItems);
  emit changed();

  if (!processing_) process();
}

void Mirror::retryFailed(const ServiceId service) {
  const auto slug = serviceSlug(service);

  QList<MirrorItem> changedItems;
  for (auto& item : items_) {
    if (item.service != slug || !item.gaveUp()) continue;
    item.retry_count = 0;
    item.next_attempt = 0;
    changedItems.append(item);
  }

  persistItems(changedItems);
  emit changed();

  if (!processing_) process();
}

void Mirror::clear(const ServiceId service) {
  const auto slug = serviceSlug(service);

  items_.removeIf([&slug](const MirrorItem& item) { return item.service == slug; });

  auto db = QSqlDatabase::database();
  if (db.open()) {
    QSqlQuery q{db};
    q.prepare("DELETE FROM mirror_queue WHERE service = :service");
    q.bindValue(":service", slug);
    q.exec();
    db.close();
  }

  emit changed();
}

MirrorStatus Mirror::status(const ServiceId service) const {
  return mirrorStatus(items_, serviceSlug(service));
}

bool Mirror::isProcessing(const ServiceId service) const {
  return processing_ && processing_->service == serviceSlug(service);
}

////////////////////////////////////////////////////////////////////////////////

void Mirror::process() {
  if (processing_ || items_.isEmpty()) return;
  if (!taiga::settings.syncEnabled()) return;

  ensureSource();

  if (taiga::network()->isPaused()) {
    scheduleNext(items_);
    return;
  }

  // Only send to services that are enabled and logged in.
  const auto enabled = services();
  QList<MirrorItem> ready;
  for (const auto& item : items_) {
    const auto service = serviceIdFromSlug(item.service);
    if (!enabled.contains(service)) continue;
    if (!isReady(service)) continue;
    ready.append(item);
  }

  const auto item = nextMirrorItem(ready, std::time(nullptr));
  if (!item) {
    scheduleNext(ready);
    return;
  }

  send(*item);
}

bool Mirror::isReady(const ServiceId service) {
  if (isUserAuthenticated(service)) return true;

  if (willAuthenticate(service) && !authenticating_.contains(service)) {
    authenticating_.insert(service);
    authenticateUser(service);
  }

  return false;
}

void Mirror::send(const MirrorItem& item) {
  const auto service = serviceIdFromSlug(item.service);
  const auto source = currentServiceId();
  const int animeId = item.anime_id;

  processing_ = EntryKey{item.service, animeId};
  modifiedWhileProcessing_ = false;
  const auto generation = ++generation_;
  watchdogTimer_->start();
  emit changed();

  // Look up this anime on the target service, along with the next ones in line.
  QList<int> ids{animeId};
  for (const auto& other : items_) {
    if (ids.size() >= kMappingBatchSize) break;
    if (other.service != item.service || ids.contains(other.anime_id)) continue;
    if (!idMapper.cached(source, other.anime_id, service)) ids.append(other.anime_id);
  }

  const auto remove = item.remove;
  const auto dirty = item.dirty;
  const auto slug = item.service;

  idMapper.resolve(source, ids, service, [=, this](bool success) {
    if (generation != generation_) return;

    if (!success) {
      finish(generation, false, tr("Couldn't look up this anime on %1.").arg(serviceName(service)));
      return;
    }

    const auto remoteId = idMapper.cached(source, animeId, service);
    if (!remoteId || *remoteId == 0) {
      finish(generation, false, tr("This anime isn't on %1.").arg(serviceName(service)), true);
      return;
    }

    const auto* local = anime::db.entry(animeId);
    const bool inList =
        local && !local->pending_delete && local->status != anime::list::Status::NotInList;

    if (remove || !inList) {
      sendRemove(service, *remoteId, generation);
      return;
    }

    auto entry = *local;
    entry.anime_id = *remoteId;
    entry.id = entryIds_.value(EntryKey{slug, animeId}, anime::list::kUnknownId);

    // A new remote entry gets everything, as the service may have different values. Empty
    // dates and notes are left out, as there's nothing to send (and some services reject them).
    const bool known = entryIds_.contains(EntryKey{slug, animeId});
    auto fields = known ? dirty : kAllListFields;
    if (!known) {
      using anime::list::Field;
      if (!entry.date_started) fields &= ~anime::list::Fields{Field::DateStarted};
      if (!entry.date_completed) fields &= ~anime::list::Fields{Field::DateCompleted};
      if (entry.notes.empty()) fields &= ~anime::list::Fields{Field::Notes};
    }
    sendSave(service, entry, fields, generation);
  });
}

void Mirror::sendRemove(const ServiceId service, const int remoteId, const uint64_t generation) {
  const auto done = [this, generation](const EntryResult& result) {
    if (generation != generation_) return;
    finish(generation, result.success, result.error, false, std::optional<int64_t>{});
  };

  const auto entryId = processing_ ? entryIds_.value(*processing_, anime::list::kUnknownId)
                                   : anime::list::kUnknownId;

  switch (service) {
    case ServiceId::MyAnimeList:
      myanimelist::Service::instance()->deleteEntry(remoteId, done);
      break;

    case ServiceId::AniList:
    case ServiceId::Kitsu: {
      const auto deleteById = [service, done](int64_t id) {
        if (service == ServiceId::AniList) {
          anilist::Service::instance()->deleteEntry(id, done);
        } else {
          kitsu::Service::instance()->deleteEntry(id, done);
        }
      };
      if (entryId != anime::list::kUnknownId) {
        deleteById(entryId);
        break;
      }
      // The entry ID isn't known, e.g. the anime was already in the list there.
      const auto found = [this, generation, deleteById](bool success, int64_t id) {
        if (generation != generation_) return;
        if (!success) {
          finish(generation, false, tr("Failed to delete list entry."));
        } else if (id == anime::list::kUnknownId) {
          finish(generation, true);  // not in the list there
        } else {
          deleteById(id);
        }
      };
      if (service == ServiceId::AniList) {
        anilist::Service::instance()->findEntryId(remoteId, found);
      } else {
        kitsu::Service::instance()->findEntryId(remoteId, found);
      }
      break;
    }

    case ServiceId::Unknown:
      finish(generation, false, {}, true);
      break;
  }
}

void Mirror::sendSave(const ServiceId service, const ListEntry& entry,
                      const anime::list::Fields dirty, const uint64_t generation) {
  const auto done = [this, generation, service, entry](const EntryResult& result) {
    if (generation != generation_) return;
    // Remember that the entry exists there, with its ID. MyAnimeList identifies entries by
    // anime ID instead.
    std::optional<int64_t> entryId;
    if (result.remote && result.remote->id != anime::list::kUnknownId) {
      entryId = result.remote->id;
    } else if (service == ServiceId::MyAnimeList) {
      entryId = entry.anime_id;
    } else if (entry.id != anime::list::kUnknownId) {
      entryId = entry.id;
    }
    finish(generation, result.success, result.error, false, entryId);
  };

  switch (service) {
    case ServiceId::MyAnimeList:
      myanimelist::Service::instance()->saveEntry(entry, dirty, done);
      break;

    case ServiceId::AniList:
      anilist::Service::instance()->saveEntry(entry, dirty, done);
      break;

    case ServiceId::Kitsu: {
      auto* kitsu = kitsu::Service::instance();
      if (entry.id != anime::list::kUnknownId) {
        kitsu->patchEntry(entry, dirty, done);
        break;
      }
      // Kitsu needs the ID of an existing library entry to update it.
      kitsu->findEntryId(entry.anime_id,
                         [this, kitsu, entry, dirty, done, generation](bool success, int64_t id) {
                           if (generation != generation_) return;
                           if (!success) {
                             finish(generation, false, tr("Failed to update list entry."));
                             return;
                           }
                           auto updated = entry;
                           updated.id = id;
                           if (id != anime::list::kUnknownId) {
                             kitsu->patchEntry(updated, dirty, done);
                           } else {
                             kitsu->createEntry(updated, dirty, done);
                           }
                         });
      break;
    }

    case ServiceId::Unknown:
      finish(generation, false, {}, true);
      break;
  }
}

void Mirror::finish(const uint64_t generation, const bool success, const QString& error,
                    const bool permanent, const std::optional<int64_t> entryId) {
  if (generation != generation_ || !processing_) return;

  const auto key = *processing_;
  processing_.reset();
  ++generation_;  // ignore anything that still arrives for this request
  watchdogTimer_->stop();

  const auto it = findItem(key.service, key.animeId);

  if (!success) {
    if (it != items_.end()) {
      failMirrorItem(*it, error.isEmpty() ? tr("Unknown error.") : error, permanent,
                     std::time(nullptr));
      persistItems({*it});
    }
  } else {
    if (it != items_.end() && it->remove) {
      entryIds_.remove(key);
      persistEntryId(key.service, key.animeId, std::nullopt);
    } else if (entryId) {
      entryIds_.insert(key, *entryId);
      persistEntryId(key.service, key.animeId, entryId);
    }

    if (it != items_.end()) {
      if (modifiedWhileProcessing_) {
        it->retry_count = 0;
        it->last_error.clear();
        persistItems({*it});
      } else {
        items_.erase(it);
        deleteItem(key.service, key.animeId);
      }
    }
  }

  emit changed();

  paceTimer_->start(kPaceDelay);
}

void Mirror::scheduleNext(const QList<MirrorItem>& candidates) {
  // Items that can't be sent yet (e.g. while logged out) are checked again now and then. Logging
  // in also resumes sending.
  constexpr std::time_t kCheckInterval = 5 * 60;

  const auto now = std::time(nullptr);

  auto at = nextMirrorAttempt(candidates).value_or(now + kCheckInterval);
  at = std::max(at, now);
  if (const auto pausedUntil = taiga::network()->pausedUntil()) {
    at = std::max<std::time_t>(at, pausedUntil->toSecsSinceEpoch());
  }

  if (items_.isEmpty()) return;
  retryTimer_->start(std::chrono::seconds{std::clamp<std::time_t>(at - now, 1, kCheckInterval)});
}

void Mirror::ensureSource() {
  const auto source = serviceSlug(currentServiceId());
  if (source == source_) return;

  // The main service changed, so queued changes and known entries refer to other anime.
  source_ = source;
  items_.clear();
  entryIds_.clear();
  processing_.reset();
  ++generation_;

  auto db = QSqlDatabase::database();
  if (db.open()) {
    for (const auto* table : {"mirror_queue", "mirror_entries"}) {
      QSqlQuery q{db};
      q.prepare(u"DELETE FROM %1 WHERE source != :source"_s.arg(QLatin1StringView{table}));
      q.bindValue(":source", source_);
      q.exec();
    }
    db.close();
  }

  emit changed();
}

QList<MirrorItem>::iterator Mirror::findItem(const QString& service, const int animeId) {
  return std::ranges::find_if(items_, [&](const MirrorItem& item) {
    return item.service == service && item.anime_id == animeId;
  });
}

////////////////////////////////////////////////////////////////////////////////

void Mirror::createTables() {
  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  QSqlQuery q{db};
  q.exec(
      "CREATE TABLE IF NOT EXISTS mirror_queue("
      "source TEXT NOT NULL, service TEXT NOT NULL, anime_id INTEGER NOT NULL, "
      "dirty INTEGER NOT NULL, remove INTEGER NOT NULL DEFAULT 0, time INTEGER NOT NULL, "
      "retry_count INTEGER NOT NULL DEFAULT 0, last_error TEXT, "
      "PRIMARY KEY (source, service, anime_id))");
  q.exec(
      "CREATE TABLE IF NOT EXISTS mirror_entries("
      "source TEXT NOT NULL, service TEXT NOT NULL, anime_id INTEGER NOT NULL, "
      "entry_id INTEGER NOT NULL, PRIMARY KEY (source, service, anime_id))");

  db.close();
}

void Mirror::readRows() {
  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  QSqlQuery q{db};
  q.prepare("SELECT * FROM mirror_queue WHERE source = :source ORDER BY time ASC");
  q.bindValue(":source", source_);
  if (q.exec()) {
    while (q.next()) {
      items_.append(MirrorItem{
          .service = q.value("service").toString(),
          .anime_id = q.value("anime_id").toInt(),
          .dirty = anime::list::Fields::fromInt(q.value("dirty").toInt()),
          .remove = q.value("remove").toBool(),
          .time = static_cast<std::time_t>(q.value("time").toLongLong()),
          .retry_count = q.value("retry_count").toInt(),
          .last_error = q.value("last_error").toString(),
      });
    }
  }

  q.prepare("SELECT * FROM mirror_entries WHERE source = :source");
  q.bindValue(":source", source_);
  if (q.exec()) {
    while (q.next()) {
      entryIds_.insert(EntryKey{q.value("service").toString(), q.value("anime_id").toInt()},
                       q.value("entry_id").toLongLong());
    }
  }

  db.close();
}

void Mirror::persistItems(const QList<MirrorItem>& items) {
  if (items.isEmpty()) return;

  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  db.transaction();
  for (const auto& item : items) {
    QSqlQuery q{db};
    q.prepare(
        "INSERT OR REPLACE INTO mirror_queue"
        "(source, service, anime_id, dirty, remove, time, retry_count, last_error) "
        "VALUES(:source, :service, :anime_id, :dirty, :remove, :time, :retry_count, :last_error)");
    q.bindValue(":source", source_);
    q.bindValue(":service", item.service);
    q.bindValue(":anime_id", item.anime_id);
    q.bindValue(":dirty", item.dirty.toInt());
    q.bindValue(":remove", item.remove);
    q.bindValue(":time", static_cast<qlonglong>(item.time));
    q.bindValue(":retry_count", item.retry_count);
    q.bindValue(":last_error", item.last_error.isEmpty() ? QVariant() : QVariant(item.last_error));
    q.exec();
  }
  db.commit();

  db.close();
}

void Mirror::deleteItem(const QString& service, const int animeId) {
  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  QSqlQuery q{db};
  q.prepare(
      "DELETE FROM mirror_queue "
      "WHERE source = :source AND service = :service AND anime_id = :anime_id");
  q.bindValue(":source", source_);
  q.bindValue(":service", service);
  q.bindValue(":anime_id", animeId);
  q.exec();

  db.close();
}

void Mirror::persistEntryId(const QString& service, const int animeId,
                            const std::optional<int64_t> id) {
  auto db = QSqlDatabase::database();
  if (!db.open()) return;

  QSqlQuery q{db};
  if (id) {
    q.prepare(
        "INSERT OR REPLACE INTO mirror_entries(source, service, anime_id, entry_id) "
        "VALUES(:source, :service, :anime_id, :entry_id)");
    q.bindValue(":entry_id", static_cast<qlonglong>(*id));
  } else {
    q.prepare(
        "DELETE FROM mirror_entries "
        "WHERE source = :source AND service = :service AND anime_id = :anime_id");
  }
  q.bindValue(":source", source_);
  q.bindValue(":service", service);
  q.bindValue(":anime_id", animeId);
  q.exec();

  db.close();
}

}  // namespace taiga::sync
