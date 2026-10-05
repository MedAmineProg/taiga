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
#include "airing_notifier.hpp"

#include <QTimer>
#include <chrono>

#include "base/log.hpp"
#include "base/string.hpp"
#include "gui/utils/tray_icon.hpp"
#include "media/airing_schedule.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "taiga/settings.hpp"

namespace gui {

AiringNotifier::AiringNotifier(QObject* parent, TrayIcon* trayIcon)
    : QObject(parent), m_trayIcon(trayIcon) {
  // Episodes that aired before startup aren't announced.
  m_lastCheck = std::time(nullptr);

  m_timer = new QTimer(this);
  m_timer->setInterval(std::chrono::seconds{30});
  connect(m_timer, &QTimer::timeout, this, &AiringNotifier::check);
  m_timer->start();
}

void AiringNotifier::check() {
  const auto now = std::time(nullptr);
  const auto from = m_lastCheck;
  m_lastCheck = now;

  if (!taiga::settings.calendarNotificationsEnabled() || !m_trayIcon) return;

  const auto episodes = anime::schedule::airedBetween(anime::db.items(), anime::db.entries(),
                                                      {anime::list::Status::Watching}, from, now);
  if (episodes.empty()) return;

  qDebug() << u"Notifying about %1 aired episode(s)"_s.arg(episodes.size());

  const auto describe = [](const anime::schedule::Episode& episode) {
    const auto item = anime::db.item(episode.anime_id);
    const auto title = item ? QString::fromStdString(anime::preferredTitle(*item)) : QString{};
    return tr("%1 episode %2").arg(title).arg(episode.number);
  };

  if (episodes.size() == 1) {
    m_trayIcon->showMessage(tr("New episode aired"), describe(episodes.front()));
  } else {
    QStringList lines;
    for (const auto& episode : episodes) lines.append(describe(episode));
    m_trayIcon->showMessage(tr("%1 new episodes aired").arg(episodes.size()), lines.join(u'\n'));
  }

  for (const auto& episode : episodes) emit notified(episode.anime_id);
}

}  // namespace gui
