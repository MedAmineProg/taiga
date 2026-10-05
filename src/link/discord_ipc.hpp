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
#include <QStringList>
#include <optional>

#include "link/discord_frames.hpp"

class QLocalSocket;
class QTimer;

namespace discord {

// A client for the Discord desktop app's local RPC socket, for setting Rich Presence.
class Client final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Client)

public:
  explicit Client(QObject* parent = nullptr);
  ~Client() override;

  void setClientId(const QString& clientId);
  // Overrides where to look for Discord (e.g. for tests).
  void setSocketNames(const QStringList& names);

  void start();
  void stop();
  bool isReady() const;

  // Kept and sent again after reconnecting. `std::nullopt` clears it.
  void setPresence(const std::optional<Presence>& presence);

  static QStringList defaultSocketNames();

signals:
  void ready();
  void disconnected();

private:
  void connectToNext();
  void handleConnected();
  void handleReadyRead();
  void handleDisconnected();
  void send(Opcode opcode, const QJsonObject& payload);
  void sendPresence();

  QString clientId_;
  QStringList socketNames_;
  qsizetype socketIndex_ = 0;
  QLocalSocket* socket_ = nullptr;
  QTimer* reconnectTimer_ = nullptr;
  FrameReader reader_;
  std::optional<Presence> presence_;
  bool running_ = false;
  bool ready_ = false;
  int nonce_ = 0;
};

}  // namespace discord
