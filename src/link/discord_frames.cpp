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
#include "discord_frames.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>

namespace discord {

namespace {

constexpr qsizetype kHeaderSize = 8;
constexpr quint32 kMaxPayloadSize = 64 * 1024;

// Discord rejects strings outside these limits.
constexpr qsizetype kMaxTextLength = 128;
constexpr qsizetype kMinTextLength = 2;
constexpr qsizetype kMaxButtonLabelLength = 32;
constexpr qsizetype kMaxUrlLength = 512;

QString fitText(QString text, const qsizetype maxLength = kMaxTextLength) {
  text = text.trimmed();
  if (text.size() > maxLength) text = text.left(maxLength - 1) + QChar(0x2026);  // ellipsis
  if (!text.isEmpty() && text.size() < kMinTextLength) text += QChar(0x3000);  // ideographic space
  return text;
}

}  // namespace

QByteArray encodeFrame(const Opcode opcode, const QJsonObject& payload) {
  const auto json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
  QByteArray frame(kHeaderSize, Qt::Uninitialized);
  qToLittleEndian(static_cast<quint32>(opcode), frame.data());
  qToLittleEndian(static_cast<quint32>(json.size()), frame.data() + 4);
  return frame + json;
}

void FrameReader::append(const QByteArray& data) {
  buffer_.append(data);
}

std::optional<Frame> FrameReader::next() {
  if (error_ || buffer_.size() < kHeaderSize) return std::nullopt;

  const auto opcode = qFromLittleEndian<quint32>(buffer_.constData());
  const auto length = qFromLittleEndian<quint32>(buffer_.constData() + 4);
  if (opcode > static_cast<quint32>(Opcode::Pong) || length > kMaxPayloadSize) {
    error_ = true;
    return std::nullopt;
  }
  if (buffer_.size() < kHeaderSize + length) return std::nullopt;

  const auto json = buffer_.mid(kHeaderSize, length);
  buffer_.remove(0, kHeaderSize + length);

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    error_ = true;
    return std::nullopt;
  }

  return Frame{.opcode = static_cast<Opcode>(opcode), .payload = document.object()};
}

bool FrameReader::hasError() const {
  return error_;
}

QJsonObject activityJson(const Presence& presence) {
  QJsonObject activity;

  if (const auto details = fitText(presence.details); !details.isEmpty()) {
    activity["details"] = details;
  }
  if (const auto state = fitText(presence.state); !state.isEmpty()) activity["state"] = state;

  if (presence.start > 0) {
    activity["timestamps"] = QJsonObject{{"start", static_cast<qint64>(presence.start)}};
  }

  QJsonObject assets;
  if (!presence.largeImage.isEmpty()) {
    assets["large_image"] = presence.largeImage;
    if (const auto text = fitText(presence.largeText); !text.isEmpty()) {
      assets["large_text"] = text;
    }
  }
  if (!presence.smallImage.isEmpty()) {
    assets["small_image"] = presence.smallImage;
    if (const auto text = fitText(presence.smallText); !text.isEmpty()) {
      assets["small_text"] = text;
    }
  }
  if (!assets.isEmpty()) activity["assets"] = assets;

  const auto label = fitText(presence.buttonLabel, kMaxButtonLabelLength);
  const bool validUrl =
      presence.buttonUrl.size() <= kMaxUrlLength &&
      (presence.buttonUrl.startsWith(u"https://") || presence.buttonUrl.startsWith(u"http://"));
  if (!label.isEmpty() && validUrl) {
    activity["buttons"] = QJsonArray{QJsonObject{{"label", label}, {"url", presence.buttonUrl}}};
  }

  return activity;
}

QJsonObject handshakePayload(const QString& clientId) {
  return {{"v", 1}, {"client_id", clientId}};
}

QJsonObject setActivityPayload(const qint64 pid, const std::optional<Presence>& presence,
                               const QString& nonce) {
  QJsonObject args{{"pid", pid}};
  args["activity"] = presence ? QJsonValue(activityJson(*presence)) : QJsonValue(QJsonValue::Null);
  return {{"cmd", "SET_ACTIVITY"}, {"args", args}, {"nonce", nonce}};
}

}  // namespace discord
