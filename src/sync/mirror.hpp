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
#include <QSet>
#include <QString>
#include <cstdint>
#include <optional>

#include "media/anime_list.hpp"
#include "sync/mirror_queue.hpp"
#include "sync/service.hpp"

class QTimer;

namespace taiga::sync {

// Sends the changes made to the list on the main service to the other services the user enabled
// ("mirrors"). The main service stays the source of truth; nothing is read back from mirrors.
class Mirror final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Mirror)

public:
  Mirror();
  ~Mirror() = default;

  void init();

  // The enabled mirrors, without the main service.
  QList<ServiceId> services() const;

  void push(const int animeId, const anime::list::Fields dirty);
  void pushRemove(const int animeId);
  // For an entry that's being removed before it was ever sent: drops changes that weren't sent
  // to mirrors either, and removes the entry from the mirrors that already have it.
  void dropUnsent(const int animeId);

  // Queues the whole list to be sent to the service.
  void copyAll(const ServiceId service);
  void retryFailed(const ServiceId service);
  void clear(const ServiceId service);

  void process();

  MirrorStatus status(const ServiceId service) const;
  bool isProcessing(const ServiceId service) const;

signals:
  void changed();

private:
  struct EntryKey {
    QString service;
    int animeId;

    bool operator==(const EntryKey&) const = default;
  };
  friend size_t qHash(const EntryKey& key, size_t seed) {
    return qHashMulti(seed, key.service, key.animeId);
  }

  void ensureSource();
  bool isReady(const ServiceId service);

  void send(const MirrorItem& item);
  void sendRemove(const ServiceId service, const int remoteId, const uint64_t generation);
  void sendSave(const ServiceId service, const ListEntry& entry, const anime::list::Fields dirty,
                const uint64_t generation);
  void finish(const uint64_t generation, const bool success, const QString& error = {},
              const bool permanent = false, const std::optional<int64_t> entryId = std::nullopt);
  void scheduleNext(const QList<MirrorItem>& candidates);

  QList<MirrorItem>::iterator findItem(const QString& service, const int animeId);

  void createTables();
  void readRows();
  void persistItems(const QList<MirrorItem>& items);
  void deleteItem(const QString& service, const int animeId);
  void persistEntryId(const QString& service, const int animeId, const std::optional<int64_t> id);

  QString source_;
  QList<MirrorItem> items_;
  QHash<EntryKey, int64_t> entryIds_;  // remote list entry IDs, or the anime ID when there's none
  QSet<ServiceId> authenticating_;

  std::optional<EntryKey> processing_;
  bool modifiedWhileProcessing_ = false;
  uint64_t generation_ = 0;

  QTimer* paceTimer_ = nullptr;
  QTimer* retryTimer_ = nullptr;
  QTimer* watchdogTimer_ = nullptr;
};

inline Mirror mirror;

}  // namespace taiga::sync
