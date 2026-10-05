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
#include <QFileInfo>
#include <QSet>
#include <algorithm>

#include "base/string.hpp"
#include "track/player_media.hpp"

namespace track::media {

const std::vector<QString>& knownPlayerProcesses() {
  static const std::vector<QString> processes{
      u"mpv"_s,      u"vlc"_s,   u"VLC"_s,      u"IINA"_s,    u"celluloid"_s,
      u"haruna"_s,   u"totem"_s, u"smplayer"_s, u"mplayer"_s, u"mpc-qt"_s,
      u"kodi.bin"_s, u"Kodi"_s,  u"Movist"_s,   u"Infuse"_s,  u"QuickTime Player"_s,
  };
  return processes;
}

bool isVideoFilePath(QStringView path) {
  static const QSet<QString> extensions{
      u"mkv"_s, u"mp4"_s,  u"m4v"_s, u"avi"_s, u"webm"_s, u"mov"_s, u"wmv"_s,  u"flv"_s,
      u"ts"_s,  u"m2ts"_s, u"ogm"_s, u"ogv"_s, u"rmvb"_s, u"mpg"_s, u"mpeg"_s,
  };
  const auto dot = path.lastIndexOf(u'.');
  if (dot < 0 || dot == path.size() - 1) return false;
  return extensions.contains(path.sliced(dot + 1).toString().toLower());
}

bool isWebBrowser(QStringView playerName) {
  static const std::vector<QString> browsers{
      u"firefox"_s, u"chrome"_s, u"chromium"_s, u"brave"_s,     u"edge"_s,
      u"vivaldi"_s, u"opera"_s,  u"safari"_s,   u"librewolf"_s, u"zen"_s,
  };
  return std::ranges::any_of(browsers, [playerName](const QString& browser) {
    return playerName.contains(browser, Qt::CaseInsensitive);
  });
}

std::vector<OpenFile> parseLsofOutput(const QByteArray& output) {
  std::vector<OpenFile> files;
  qint64 pid = 0;
  QString command;

  for (const auto& line : output.split('\n')) {
    if (line.isEmpty()) continue;
    const auto value = QString::fromUtf8(line.sliced(1));
    switch (line.front()) {
      case 'p':
        pid = value.toLongLong();
        command.clear();
        break;
      case 'c':
        command = value;
        break;
      case 'n':
        if (pid && isVideoFilePath(value)) {
          files.push_back({.pid = pid, .command = command, .path = value});
        }
        break;
      default:
        break;
    }
  }

  return files;
}

const PlayerMedia* pickMedia(const std::vector<PlayerMedia>& candidates,
                             const bool allowWebBrowsers) {
  const auto score = [](const PlayerMedia& media) {
    int score = 0;
    if (media.playing) score += 4;
    if (!media.file.isEmpty()) score += 2;
    if (!media.webBrowser) score += 1;
    return score;
  };

  const PlayerMedia* best = nullptr;
  for (const auto& media : candidates) {
    if (media.webBrowser && !allowWebBrowsers) continue;
    if (media.file.isEmpty() && media.title.isEmpty()) continue;
    if (!best || score(media) > score(*best)) best = &media;
  }
  return best;
}

}  // namespace track::media
