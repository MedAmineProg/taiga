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

#include <QList>
#include <QString>
#include <ctime>
#include <optional>

#include "media/anime_list.hpp"

// Pure logic of the mirror queue, which sends list changes made on the main service to the other
// services the user enabled. Kept separate from the network code so that it can be unit-tested.

namespace taiga::sync {

// After this many failed attempts, an item stays in the queue with its error but isn't retried
// until it changes again or the user asks for it.
constexpr int kMirrorMaxAttempts = 5;

constexpr anime::list::Fields kAllListFields =
    anime::list::Field::Episode | anime::list::Field::Score | anime::list::Field::Status |
    anime::list::Field::Rewatching | anime::list::Field::RewatchedTimes |
    anime::list::Field::DateStarted | anime::list::Field::DateCompleted | anime::list::Field::Notes;

struct MirrorItem {
  QString service;   // slug of the target service
  int anime_id = 0;  // ID on the main service
  anime::list::Fields dirty;
  bool remove = false;
  std::time_t time = 0;
  int retry_count = 0;
  QString last_error;
  std::time_t next_attempt = 0;  // not persisted

  bool gaveUp() const {
    return retry_count >= kMirrorMaxAttempts;
  }
};

struct MirrorStatus {
  int pending = 0;
  int failed = 0;
  QString lastError;
};

// Adds a change to the queue, or merges it into the queued change for the same anime. Returns the
// item, which stays valid until the list changes.
MirrorItem& mergeMirrorItem(QList<MirrorItem>& items, const QString& service, const int animeId,
                            const anime::list::Fields dirty, const bool remove,
                            const std::time_t now);

// The oldest item that can be sent now, if any.
const MirrorItem* nextMirrorItem(const QList<MirrorItem>& items, const std::time_t now);

// When the next item that's waiting for a retry can be sent, if any is waiting.
std::optional<std::time_t> nextMirrorAttempt(const QList<MirrorItem>& items);

MirrorStatus mirrorStatus(const QList<MirrorItem>& items, const QString& service);

// Records a failed attempt. Permanent failures (e.g. the anime doesn't exist on the target
// service) aren't retried.
void failMirrorItem(MirrorItem& item, const QString& error, const bool permanent,
                    const std::time_t now);

// Whether a cached ID mapping can be used. Missing anime are looked up again after a while, as
// services add them over time.
bool isMappingFresh(const bool found, const std::time_t checked, const std::time_t now);

}  // namespace taiga::sync
