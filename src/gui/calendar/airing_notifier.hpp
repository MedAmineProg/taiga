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

#include <QObject>
#include <ctime>

class QTimer;

namespace gui {

class TrayIcon;

// Notifies via the tray icon when episodes of anime being watched air.
class AiringNotifier final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(AiringNotifier)

public:
  AiringNotifier(QObject* parent, TrayIcon* trayIcon);
  ~AiringNotifier() = default;

signals:
  void notified(int animeId);

private:
  void check();

  TrayIcon* m_trayIcon = nullptr;
  QTimer* m_timer = nullptr;
  std::time_t m_lastCheck = 0;
};

}  // namespace gui
