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

#include <QByteArray>
#include <QString>
#include <QStringView>
#include <vector>

// Finding what media players are playing on Linux and macOS, where Anisthesia doesn't work yet.
namespace track::media {

struct PlayerMedia {
  QString player;        // e.g. "VLC media player"
  QString file;          // local path, if known
  QString title;         // e.g. a window or stream title, if there's no file
  bool playing = false;  // known to be playing (not paused or stopped)
  bool webBrowser = false;
};

// Players whose open files are checked when they don't report what they play.
const std::vector<QString>& knownPlayerProcesses();

bool isVideoFilePath(QStringView path);
bool isWebBrowser(QStringView playerName);

struct OpenFile {
  qint64 pid = 0;
  QString command;
  QString path;
};

// Parses `lsof -F pcn` output.
std::vector<OpenFile> parseLsofOutput(const QByteArray& output);

// Picks what to recognize: playing media first, then local files, then titles; web browsers only
// if streaming media detection is enabled.
const PlayerMedia* pickMedia(const std::vector<PlayerMedia>& candidates, bool allowWebBrowsers);

// What's playing right now, by platform-specific means.
std::vector<PlayerMedia> findPlayerMedia();

}  // namespace track::media
