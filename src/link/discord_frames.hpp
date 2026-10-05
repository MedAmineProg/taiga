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
#include <QJsonObject>
#include <QString>
#include <ctime>
#include <optional>

// Discord's local RPC protocol: each frame is an opcode and a payload length (both 32-bit
// little-endian integers), followed by a JSON payload.
namespace discord {

enum class Opcode : quint32 {
  Handshake = 0,
  Frame = 1,
  Close = 2,
  Ping = 3,
  Pong = 4,
};

struct Frame {
  Opcode opcode = Opcode::Frame;
  QJsonObject payload;
};

QByteArray encodeFrame(Opcode opcode, const QJsonObject& payload);

// Collects incoming bytes and returns complete frames.
class FrameReader {
public:
  void append(const QByteArray& data);
  std::optional<Frame> next();
  bool hasError() const;

private:
  QByteArray buffer_;
  bool error_ = false;
};

// What to show on the user's profile.
struct Presence {
  QString details;     // first line (e.g. the title)
  QString state;       // second line (e.g. the episode)
  QString largeImage;  // asset key or https URL
  QString largeText;
  QString smallImage;
  QString smallText;
  std::time_t start = 0;  // shows elapsed time if set
  QString buttonLabel;
  QString buttonUrl;
};

QJsonObject activityJson(const Presence& presence);

QJsonObject handshakePayload(const QString& clientId);

// `presence` being empty clears the activity.
QJsonObject setActivityPayload(qint64 pid, const std::optional<Presence>& presence,
                               const QString& nonce);

}  // namespace discord
