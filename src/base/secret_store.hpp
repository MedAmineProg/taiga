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

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

namespace base {

// Keeps secrets (tokens, passwords) in the operating system's credential store.
//
// Values are read once at startup and cached, so that reads are synchronous and cheap. Writes are
// asynchronous and update the cache immediately. If no credential store is available, the store
// reports itself as unavailable and callers should fall back to their regular storage.
class SecretStore final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SecretStore)

public:
  SecretStore() = default;
  ~SecretStore() = default;

  // Blocks until the given keys are loaded, or the credential store fails to respond.
  void init(const QString& service, const QString& prefix, const QStringList& keys);

  bool isAvailable() const;

  // Returns `std::nullopt` if the value is neither in the credential store nor set since startup.
  std::optional<QString> value(const QString& key) const;

  // The callback receives `false` if the value could not be written to the credential store, in
  // which case the caller is responsible for storing it elsewhere.
  using Callback = std::function<void(bool success)>;
  void setValue(const QString& key, const QString& value, Callback callback = {});

private:
  QString qualifiedKey(const QString& key) const;

  bool available_ = false;
  QString service_;
  QString prefix_;
  QHash<QString, QString> values_;
};

inline SecretStore secrets;

}  // namespace base
