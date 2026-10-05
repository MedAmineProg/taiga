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
#include "discord_ipc.hpp"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QTimer>
#include <chrono>

#include "base/string.hpp"

namespace discord {

namespace {

constexpr int kMaxPipes = 10;

}  // namespace

Client::Client(QObject* parent) : QObject(parent) {
  socket_ = new QLocalSocket(this);
  connect(socket_, &QLocalSocket::connected, this, &Client::handleConnected);
  connect(socket_, &QLocalSocket::readyRead, this, &Client::handleReadyRead);
  connect(socket_, &QLocalSocket::disconnected, this, &Client::handleDisconnected);
  connect(socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
    if (!ready_ && running_) {
      // Not this one; try the next candidate.
      ++socketIndex_;
      QTimer::singleShot(0, this, &Client::connectToNext);
    }
  });

  reconnectTimer_ = new QTimer(this);
  reconnectTimer_->setSingleShot(true);
  reconnectTimer_->setInterval(std::chrono::seconds{30});
  connect(reconnectTimer_, &QTimer::timeout, this, [this]() {
    socketIndex_ = 0;
    connectToNext();
  });

  socketNames_ = defaultSocketNames();
}

Client::~Client() {
  // Don't leave a stale activity behind.
  if (ready_) {
    presence_.reset();
    sendPresence();
    socket_->flush();
  }
}

void Client::setClientId(const QString& clientId) {
  if (clientId == clientId_) return;
  clientId_ = clientId;
  if (running_) {
    stop();
    start();
  }
}

void Client::setSocketNames(const QStringList& names) {
  socketNames_ = names;
}

void Client::start() {
  if (running_ || clientId_.isEmpty()) return;
  running_ = true;
  socketIndex_ = 0;
  connectToNext();
}

void Client::stop() {
  running_ = false;
  reconnectTimer_->stop();
  if (ready_) {
    const auto presence = presence_;
    presence_.reset();
    sendPresence();
    socket_->flush();
    presence_ = presence;
  }
  ready_ = false;
  socket_->abort();
}

bool Client::isReady() const {
  return ready_;
}

void Client::setPresence(const std::optional<Presence>& presence) {
  presence_ = presence;
  if (ready_) sendPresence();
}

QStringList Client::defaultSocketNames() {
  QStringList names;
#ifdef Q_OS_WINDOWS
  // Named pipes: \\.\pipe\discord-ipc-N
  for (int i = 0; i < kMaxPipes; ++i) names.append(u"discord-ipc-%1"_s.arg(i));
#else
  QStringList directories;
  for (const auto variable : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
    if (const auto value = qEnvironmentVariable(variable); !value.isEmpty()) {
      directories.append(value);
    }
  }
  directories.append(u"/tmp"_s);
  directories.removeDuplicates();

  // Flatpak and Snap versions of Discord use subdirectories.
  for (const auto& directory : directories) {
    for (const auto subdirectory : {"", "/app/com.discordapp.Discord", "/snap.discord"}) {
      for (int i = 0; i < kMaxPipes; ++i) {
        names.append(
            u"%1%2/discord-ipc-%3"_s.arg(directory).arg(QLatin1StringView{subdirectory}).arg(i));
      }
    }
  }
#endif
  return names;
}

void Client::connectToNext() {
  if (!running_ || ready_) return;
  if (socketIndex_ >= socketNames_.size()) {
    // Discord isn't running; try again later.
    reconnectTimer_->start();
    return;
  }
  socket_->abort();
  reader_ = {};
  socket_->connectToServer(socketNames_[socketIndex_]);
}

void Client::handleConnected() {
  send(Opcode::Handshake, handshakePayload(clientId_));
}

void Client::handleReadyRead() {
  reader_.append(socket_->readAll());

  while (const auto frame = reader_.next()) {
    switch (frame->opcode) {
      case Opcode::Frame:
        if (frame->payload["evt"].toString() == u"READY" && !ready_) {
          ready_ = true;
          emit ready();
          sendPresence();
        }
        break;
      case Opcode::Ping:
        send(Opcode::Pong, frame->payload);
        break;
      case Opcode::Close:
        socket_->disconnectFromServer();
        break;
      default:
        break;
    }
  }

  if (reader_.hasError()) socket_->abort();
}

void Client::handleDisconnected() {
  const bool wasReady = ready_;
  ready_ = false;
  if (wasReady) emit disconnected();
  if (running_ && wasReady) {
    socketIndex_ = 0;
    reconnectTimer_->start();
  }
}

void Client::send(const Opcode opcode, const QJsonObject& payload) {
  if (socket_->state() != QLocalSocket::ConnectedState) return;
  socket_->write(encodeFrame(opcode, payload));
}

void Client::sendPresence() {
  send(Opcode::Frame, setActivityPayload(QCoreApplication::applicationPid(), presence_,
                                         QString::number(++nonce_)));
}

}  // namespace discord
