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

#include "application.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMouseEvent>
#include <QTimer>
#include <QTranslator>
#include <chrono>
#include <format>

#include "base/log.hpp"
#include "base/secret_store.hpp"
#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "sync/queue.hpp"
#include "taiga/accounts.hpp"
#include "taiga/config.h"
#include "taiga/discord_presence.hpp"
#include "taiga/path.hpp"
#include "taiga/settings.hpp"
#include "taiga/version.hpp"
#include "track/media.hpp"
#include "track/update_session.hpp"

namespace taiga {

Application::Application(int argc, char* argv[]) : QApplication(argc, argv) {
  setApplicationName("taiga");
  setApplicationDisplayName("Taiga");
  setApplicationVersion(QString::fromStdString(taiga::version().to_string()));
  setOrganizationDomain("taiga.moe");
  setOrganizationName("erengy");
}

Application::~Application() {
  if (window_) {
    window_->hide();
  }
}

int Application::run() {
  parseCommandLine();

  initLogger();

  const auto version = taiga::version().to_string();
  const auto fileInfo = QFileInfo{QCoreApplication::applicationFilePath()};
  const auto lastModified = fileInfo.lastModified().toString(Qt::DateFormat::ISODate);
  qDebug() << u"Version %1 (%2)"_s.arg(version).arg(lastModified);
  if (!parser_.optionNames().isEmpty()) {
    qDebug() << "Options:" << parser_.optionNames().join(", ");
  }

  if (hasPreviousInstance()) {
    activatePreviousInstance();
    qDebug() << "Another instance of Taiga is running.";
    return 0;
  }

  connect(&local_server_, &QLocalServer::newConnection, this, &Application::onNewConnection);
  // A server left behind by a crash would keep this one from listening (on Unix).
  QLocalServer::removeServer(TAIGA_APP_NAME);
  local_server_.listen(TAIGA_APP_NAME);

  taiga::settings.init();
  initSecrets();
  anime::db.init();
  anime::history.init();
  taiga::sync::queue.init();
  track::media::detection()->init();
  track::updateSession()->init();
  gui::imageProvider.init();

  gui::theme.initStyle();
  setWindowIcon(gui::theme.getIcon("taiga", "png"));

  QTranslator translator;
  if (translator.load(QLocale::system(), "taiga", "_", ":/i18n")) {
    installTranslator(&translator);
  }

  window_ = new gui::MainWindow();
  window_->init();

  discordPresence()->init();

#ifdef Q_OS_WINDOWS
  // Delay showing the window to avoid a white flash.
  window_->setWindowOpacity(0.0);
  window_->show();
  QTimer::singleShot(std::chrono::milliseconds(50), window_, [this]() {
    if (window_) {
      window_->setWindowOpacity(1.0);
    }
  });
#else
  window_->show();
#endif

  return QApplication::exec();
}

bool Application::isDebug() const {
  return options_.debug;
}

bool Application::isVerbose() const {
  return options_.verbose;
}

gui::MainWindow* Application::mainWindow() const {
  return window_.get();
}

bool Application::notify(QObject* receiver, QEvent* event) {
  // Restrict double-click events to left mouse button.
  if (event->type() == QEvent::MouseButtonDblClick &&
      static_cast<QMouseEvent*>(event)->button() != Qt::LeftButton) {
    return true;
  }

  return QApplication::notify(receiver, event);
}

bool Application::hasPreviousInstance() {
  // Unlike shared memory, a lock file left behind by a crashed instance is detected as stale
  // (its process is gone), so Taiga can start again.
  const auto directory = QString::fromStdString(get_data_path());
  QDir().mkpath(directory);
  instance_lock_ = std::make_unique<QLockFile>(u"%1/taiga.lock"_s.arg(directory));
  instance_lock_->setStaleLockTime(0);  // only stale if its process is gone
  return !instance_lock_->tryLock(0);
}

void Application::activatePreviousInstance() {
  QLocalSocket socket;
  socket.connectToServer(TAIGA_APP_NAME);
  socket.waitForConnected(std::chrono::milliseconds(1000).count());
}

void Application::initLogger() const {
  const auto directory = u"%1/logs"_s.arg(get_data_path());
  QDir().mkpath(directory);

  const auto date = QDate::currentDate().toString(Qt::DateFormat::ISODate);
  const auto path = u"%1/%2_%3.log"_s.arg(directory).arg(TAIGA_APP_NAME).arg(date);

  base::initLogging(path, options_.debug ? QtDebugMsg : QtWarningMsg);
}

void Application::initSecrets() const {
  // Entries are scoped to the data directory, so that portable installations don't share them.
  const auto dataPath = QDir::cleanPath(QString::fromStdString(get_data_path()));
  const auto prefix =
      QCryptographicHash::hash(dataPath.toUtf8(), QCryptographicHash::Sha1).toHex().left(8);

  base::secrets.init(TAIGA_APP_NAME, QString::fromLatin1(prefix),
                     taiga::Settings::secretKeys() + taiga::Accounts::secretKeys());

  taiga::settings.initSecrets();
  taiga::accounts.initSecrets();
}

void Application::onNewConnection() {
  while (auto socket = local_server_.nextPendingConnection()) {
    socket->deleteLater();
  }
  if (window_) {
    window_->displayWindow();
  }
}

void Application::parseCommandLine() {
  parser_.addOptions({
      {"debug", QCoreApplication::translate("main", "Enable debug mode")},
      {"verbose", QCoreApplication::translate("main", "Enable verbose output")},
  });

  // This stops the current process in case of an error (e.g. an unknown option was passed).
  parser_.process(QApplication::arguments());

#ifdef _DEBUG
  options_.debug = true;
#else
  options_.debug = parser_.isSet("debug");
#endif
  options_.verbose = parser_.isSet("verbose");
}

}  // namespace taiga
