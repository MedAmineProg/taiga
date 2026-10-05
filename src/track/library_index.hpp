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

#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <vector>

#include "media/home_feed.hpp"
#include "track/episode.hpp"

namespace track {

// Knows which episodes are available in library folders, so that they can be offered without
// searching the disk each time. Folders are scanned in the background.
class LibraryIndex final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(LibraryIndex)

public:
  LibraryIndex() = default;
  ~LibraryIndex() = default;

  const anime::home::AvailableEpisodes& episodes() const;
  bool isScanning() const;
  QDateTime lastScanned() const;

public slots:
  void scan();
  void scanIfStale();

signals:
  void scanStarted();
  void updated();

private:
  void handleResults(const std::vector<Episode>& episodes);

  anime::home::AvailableEpisodes episodes_;
  QFutureWatcher<std::vector<Episode>>* watcher_ = nullptr;
  QDateTime lastScanned_;
};

LibraryIndex* libraryIndex();

}  // namespace track
