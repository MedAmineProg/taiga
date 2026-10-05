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
#include "library_index.hpp"

#include <QDirIterator>
#include <QtConcurrentRun>
#include <chrono>

#include "base/log.hpp"
#include "base/string.hpp"
#include "taiga/settings.hpp"
#include "track/recognition.hpp"

namespace track {

namespace {

// Avoid treating a batch release (e.g. "01-1000") as a thousand episodes.
constexpr int kMaxEpisodeRange = 500;

constexpr auto kStaleAfter = std::chrono::minutes{10};

std::vector<Episode> parseVideoFiles(const QStringList& folders) {
  std::vector<Episode> episodes;

  for (const auto& folder : folders) {
    QDirIterator it{folder, QDir::Files, QDirIterator::Subdirectories};
    while (it.hasNext()) {
      auto episode = recognition::parseFileInfo(it.nextFileInfo());
      if (!recognition::isVideoFile(episode)) continue;
      if (!episode.episodeNumberRange()) continue;
      episodes.push_back(std::move(episode));
    }
  }

  return episodes;
}

}  // namespace

const anime::home::AvailableEpisodes& LibraryIndex::episodes() const {
  return episodes_;
}

bool LibraryIndex::isScanning() const {
  return watcher_ && watcher_->isRunning();
}

QDateTime LibraryIndex::lastScanned() const {
  return lastScanned_;
}

void LibraryIndex::scan() {
  if (isScanning()) return;

  QStringList folders;
  for (const auto& folder : taiga::settings.libraryFolders()) {
    folders.append(QString::fromStdString(folder));
  }

  if (!watcher_) {
    watcher_ = new QFutureWatcher<std::vector<Episode>>(this);
    connect(watcher_, &QFutureWatcherBase::finished, this,
            [this]() { handleResults(watcher_->result()); });
  }

  emit scanStarted();
  watcher_->setFuture(QtConcurrent::run(parseVideoFiles, folders));
}

void LibraryIndex::scanIfStale() {
  if (lastScanned_.isValid() && lastScanned_.secsTo(QDateTime::currentDateTimeUtc()) <
                                    std::chrono::seconds{kStaleAfter}.count()) {
    return;
  }
  scan();
}

void LibraryIndex::handleResults(const std::vector<Episode>& episodes) {
  // Identification reads the anime database, so it happens on the main thread.
  anime::home::AvailableEpisodes result;

  for (auto episode : episodes) {
    const int animeId = recognition::identify(episode);
    if (animeId == anime::kUnknownId) continue;

    const auto range = episode.episodeNumberRange();
    if (!range || range->first < 1) continue;

    const int last = std::min(range->second, range->first + kMaxEpisodeRange);
    for (int number = range->first; number <= last; ++number) {
      result[animeId].insert(number);
    }
  }

  episodes_ = std::move(result);
  lastScanned_ = QDateTime::currentDateTimeUtc();

  qDebug() << u"Library scan found episodes of %1 anime"_s.arg(episodes_.size());

  emit updated();
}

LibraryIndex* libraryIndex() {
  static auto index = new LibraryIndex();
  return index;
}

}  // namespace track
