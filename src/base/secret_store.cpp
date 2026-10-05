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
#include "secret_store.hpp"

#include <qtkeychain/keychain.h>

#include <QEventLoop>
#include <QTimer>
#include <chrono>

#include "base/log.hpp"
#include "base/string.hpp"

namespace base {

void SecretStore::init(const QString& service, const QString& prefix, const QStringList& keys) {
  service_ = service;
  prefix_ = prefix;
  values_.clear();
  available_ = QKeychain::isAvailable();

  if (!available_) {
    qWarning() << "Credential store is not available.";
    return;
  }

  QEventLoop loop;
  qsizetype pending = keys.size();

  for (const auto& key : keys) {
    auto job = new QKeychain::ReadPasswordJob(service_, this);
    job->setKey(qualifiedKey(key));
    connect(job, &QKeychain::Job::finished, this,
            [this, &loop, &pending, key](QKeychain::Job* job) {
              switch (job->error()) {
                case QKeychain::NoError:
                  values_[key] = static_cast<QKeychain::ReadPasswordJob*>(job)->textData();
                  break;
                case QKeychain::EntryNotFound:
                  break;
                default:
                  qWarning() << u"Could not read \"%1\" from credential store: %2"_s.arg(key).arg(
                      job->errorString());
                  available_ = false;  // avoid losing data to a store that can't be read
                  break;
              }
              if (--pending == 0) loop.quit();
            });
    job->start();
  }

  if (pending > 0) {
    // Some backends (e.g. D-Bus services) may never respond.
    QTimer::singleShot(std::chrono::seconds{10}, &loop, [this, &loop]() {
      qWarning() << "Credential store timed out.";
      available_ = false;
      loop.quit();
    });
    loop.exec();
  }

  // Disconnect pending jobs, as their handlers refer to local variables.
  for (auto job : findChildren<QKeychain::Job*>(Qt::FindDirectChildrenOnly)) {
    job->disconnect(this);
  }
}

bool SecretStore::isAvailable() const {
  return available_;
}

std::optional<QString> SecretStore::value(const QString& key) const {
  const auto it = values_.find(key);
  return it != values_.end() ? std::optional{*it} : std::nullopt;
}

void SecretStore::setValue(const QString& key, const QString& value, Callback callback) {
  // The cache always holds the latest value, even if it ends up being stored elsewhere.
  values_[key] = value;

  if (!available_) {
    if (callback) callback(false);
    return;
  }

  auto job = new QKeychain::WritePasswordJob(service_, this);
  job->setKey(qualifiedKey(key));
  job->setTextData(value);
  connect(job, &QKeychain::Job::finished, this, [key, callback](QKeychain::Job* job) {
    const bool success = job->error() == QKeychain::NoError;
    if (!success) {
      qWarning() << u"Could not write \"%1\" to credential store: %2"_s.arg(key).arg(
          job->errorString());
    }
    if (callback) callback(success);
  });
  job->start();
}

QString SecretStore::qualifiedKey(const QString& key) const {
  return prefix_.isEmpty() ? key : u"%1/%2"_s.arg(prefix_).arg(key);
}

}  // namespace base
