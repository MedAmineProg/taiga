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
#include "player_media.hpp"

#include <QtGlobal>
#include <algorithm>

#if defined(Q_OS_LINUX)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#elif defined(Q_OS_MACOS)
#include <QProcess>
#endif

#include "base/string.hpp"

namespace track::media {

#if defined(Q_OS_LINUX)

namespace {

constexpr int kDBusTimeoutMs = 300;

// Players that implement MPRIS (most of them, and web browsers).
void findMprisMedia(std::vector<PlayerMedia>& result) {
  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected() || !bus.interface()) return;

  const auto names = bus.interface()->registeredServiceNames().value();
  for (const auto& name : names) {
    if (!name.startsWith(u"org.mpris.MediaPlayer2.")) continue;

    QDBusInterface properties(name, u"/org/mpris/MediaPlayer2"_s,
                              u"org.freedesktop.DBus.Properties"_s, bus);
    properties.setTimeout(kDBusTimeoutMs);

    const auto get = [&properties](const QString& interface, const QString& property) {
      const QDBusReply<QDBusVariant> reply = properties.call(u"Get"_s, interface, property);
      return reply.isValid() ? reply.value().variant() : QVariant{};
    };

    const auto status = get(u"org.mpris.MediaPlayer2.Player"_s, u"PlaybackStatus"_s).toString();
    if (status.isEmpty() || status == u"Stopped") continue;

    const auto metadataValue = get(u"org.mpris.MediaPlayer2.Player"_s, u"Metadata"_s);
    const auto metadata = metadataValue.canConvert<QDBusArgument>()
                              ? qdbus_cast<QVariantMap>(metadataValue.value<QDBusArgument>())
                              : metadataValue.toMap();

    PlayerMedia media;
    media.player = get(u"org.mpris.MediaPlayer2"_s, u"Identity"_s).toString();
    if (media.player.isEmpty()) media.player = name.mid(name.lastIndexOf(u'.') + 1);
    media.webBrowser = isWebBrowser(media.player) || isWebBrowser(name);
    media.playing = status == u"Playing";

    const QUrl url{metadata.value(u"xesam:url"_s).toString()};
    if (url.isLocalFile() && isVideoFilePath(url.toLocalFile())) {
      media.file = url.toLocalFile();
    }
    media.title = metadata.value(u"xesam:title"_s).toString();

    result.push_back(media);
  }
}

// Players that don't implement MPRIS (e.g. mpv without a plugin): look at their open files.
void findOpenFiles(std::vector<PlayerMedia>& result) {
  const auto processes = knownPlayerProcesses();
  const QDir proc{u"/proc"_s};

  for (const auto& pid : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    bool isNumber = false;
    pid.toLongLong(&isNumber);
    if (!isNumber) continue;

    QFile commFile{u"/proc/%1/comm"_s.arg(pid)};
    if (!commFile.open(QIODevice::ReadOnly)) continue;
    const auto command = QString::fromUtf8(commFile.readAll()).trimmed();
    const bool known = std::ranges::any_of(processes, [&command](const QString& process) {
      return command.compare(process, Qt::CaseInsensitive) == 0;
    });
    if (!known) continue;

    const QDir fds{u"/proc/%1/fd"_s.arg(pid)};
    for (const auto& fd : fds.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot)) {
      const auto target = fd.symLinkTarget();
      if (!isVideoFilePath(target)) continue;
      const bool alreadyKnown = std::ranges::any_of(
          result, [&target](const PlayerMedia& media) { return media.file == target; });
      if (alreadyKnown) continue;
      result.push_back({.player = command, .file = target});
    }
  }
}

}  // namespace

std::vector<PlayerMedia> findPlayerMedia() {
  std::vector<PlayerMedia> result;
  findMprisMedia(result);
  findOpenFiles(result);
  return result;
}

#elif defined(Q_OS_MACOS)

std::vector<PlayerMedia> findPlayerMedia() {
  // `-c` matches the beginning of process names, and multiple ones are combined.
  QStringList arguments{u"-n"_s, u"-P"_s, u"-F"_s, u"pcn"_s};
  for (const auto& process : knownPlayerProcesses()) {
    arguments << u"-c"_s << process.section(u' ', 0, 0);
  }

  QProcess lsof;
  lsof.start(u"/usr/sbin/lsof"_s, arguments);
  if (!lsof.waitForFinished(2000)) {
    lsof.kill();
    return {};
  }

  std::vector<PlayerMedia> result;
  for (const auto& file : parseLsofOutput(lsof.readAllStandardOutput())) {
    result.push_back({.player = file.command, .file = file.path});
  }
  return result;
}

#else

std::vector<PlayerMedia> findPlayerMedia() {
  return {};  // Windows uses Anisthesia
}

#endif

}  // namespace track::media
