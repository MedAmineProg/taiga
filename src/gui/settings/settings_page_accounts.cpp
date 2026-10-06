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
#include "settings_page_accounts.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <string>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/anilist/anilist_utils.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/mirror.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/myanimelist/myanimelist_utils.hpp"
#include "sync/service.hpp"
#include "taiga/accounts.hpp"
#include "taiga/settings.hpp"
#include "ui_settings_dialog.h"

namespace gui {

namespace {

QString toQString(const std::string& s) {
  return QString::fromStdString(s);
}

std::string toStdString(const QLineEdit* lineEdit) {
  return lineEdit->text().trimmed().toStdString();
}

}  // namespace

SettingsPageAccounts::SettingsPageAccounts(Ui::SettingsDialog* ui, QDialog* dialog)
    : SettingsPage(ui, dialog) {
  using taiga::sync::ServiceId;

  for (const auto id : {ServiceId::AniList, ServiceId::Kitsu, ServiceId::MyAnimeList}) {
    ui_->serviceComboBox->addItem(taiga::sync::serviceName(id), taiga::sync::serviceSlug(id));
  }

  connect(ui_->serviceComboBox, &QComboBox::currentIndexChanged, this,
          [this]() { updateVisibleGroup(); });

  createLoginRows();
  createMirrorGroup();
  connect(&taiga::sync::mirror, &taiga::sync::Mirror::changed, this,
          &SettingsPageAccounts::updateMirrorRows);
}

void SettingsPageAccounts::createLoginRows() {
  using taiga::sync::ServiceId;

  // Tokens come from logging in, so they aren't edited by hand. AniList and MyAnimeList also
  // provide the username when logging in.
  for (auto* widget : std::initializer_list<QWidget*>{
           ui_->anilistUsernameLineEdit, ui_->anilistTokenLineEdit, ui_->kitsuAccessTokenLineEdit,
           ui_->kitsuRefreshTokenLineEdit, ui_->myanimelistUsernameLineEdit,
           ui_->myanimelistAccessTokenLineEdit, ui_->myanimelistRefreshTokenLineEdit}) {
    if (auto* layout = qobject_cast<QFormLayout*>(widget->parentWidget()->layout())) {
      layout->setRowVisible(widget, false);
    }
  }

  const QList<std::pair<ServiceId, QFormLayout*>> groups{
      {ServiceId::AniList, ui_->anilistFormLayout},
      {ServiceId::Kitsu, ui_->kitsuFormLayout},
      {ServiceId::MyAnimeList, ui_->myanimelistFormLayout},
  };

  for (const auto& [service, formLayout] : groups) {
    LoginRow row{.service = service};

    auto* widget = new QWidget(formLayout->parentWidget());
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    row.statusLabel = new QLabel(widget);
    row.logInButton = new QPushButton(widget);
    row.logOutButton = new QPushButton(tr("Log out"), widget);

    layout->addWidget(row.statusLabel, 1);
    layout->addWidget(row.logInButton);
    layout->addWidget(row.logOutButton);

    if (service == ServiceId::Kitsu) {
      formLayout->addRow(widget);  // below the email and password
    } else {
      formLayout->insertRow(0, tr("Account:"), widget);
    }

    connect(row.logInButton, &QPushButton::clicked, this, [this, service] { logIn(service); });
    connect(row.logOutButton, &QPushButton::clicked, this, [this, service] { logOut(service); });

    loginRows_.append(row);
  }

  const QList<std::pair<ServiceId, taiga::sync::Service*>> services{
      {ServiceId::AniList, taiga::sync::anilist::Service::instance()},
      {ServiceId::Kitsu, taiga::sync::kitsu::Service::instance()},
      {ServiceId::MyAnimeList, taiga::sync::myanimelist::Service::instance()},
  };
  for (const auto& [service, instance] : services) {
    connect(instance, &taiga::sync::Service::authenticationCompleted, this,
            [this, service](bool authenticated) {
              if (auto* row = loginRow(service)) {
                row->pending = false;
                row->failed = !authenticated;
              }
              updateLoginRows();
            });
  }

  updateLoginRows();
}

SettingsPageAccounts::LoginRow* SettingsPageAccounts::loginRow(
    const taiga::sync::ServiceId service) {
  const auto it = std::ranges::find(loginRows_, service, &LoginRow::service);
  return it != loginRows_.end() ? &*it : nullptr;
}

void SettingsPageAccounts::updateLoginRows() {
  using taiga::sync::ServiceId;

  const auto& accounts = taiga::accounts;

  for (auto& row : loginRows_) {
    bool hasToken = false;
    std::string username;
    switch (row.service) {
      case ServiceId::AniList:
        hasToken = !accounts.anilistToken().empty();
        username = accounts.anilistUsername();
        break;
      case ServiceId::Kitsu:
        hasToken = !accounts.kitsuAccessToken().empty();
        username = accounts.kitsuUsername();
        break;
      case ServiceId::MyAnimeList:
        hasToken = !accounts.myanimelistAccessToken().empty();
        username = accounts.myanimelistUsername();
        break;
      case ServiceId::Unknown:
        break;
    }

    QString status;
    if (row.pending) {
      status = tr("Logging in...");
    } else if (hasToken && row.failed) {
      status = tr("Couldn't log in. Please log in again.");
    } else if (hasToken) {
      status = username.empty() ? tr("Logged in.")
                                : tr("Logged in as %1.").arg(QString::fromStdString(username));
    } else if (row.failed) {
      status = tr("Couldn't log in.");
    } else {
      status = tr("Not logged in.");
    }

    row.statusLabel->setText(status);
    row.logInButton->setText(hasToken ? tr("Log in again...") : tr("Log in..."));
    row.logInButton->setEnabled(!row.pending);
    row.logOutButton->setVisible(hasToken && !row.pending);
  }
}

void SettingsPageAccounts::logIn(const taiga::sync::ServiceId service) {
  using taiga::sync::ServiceId;

  auto* row = loginRow(service);
  if (!row) return;

  const auto name = taiga::sync::serviceName(service);

  switch (service) {
    case ServiceId::AniList: {
      QDesktopServices::openUrl(
          QUrl{QString::fromStdString(taiga::sync::anilist::requestTokenUrl())});
      bool ok = false;
      const auto token =
          QInputDialog::getText(dialog_, tr("Log in to %1").arg(name),
                                tr("AniList has opened in your browser. Log in, select "
                                   "\"Authorize\", then copy the token shown on the page and "
                                   "paste it here:"),
                                QLineEdit::Normal, {}, &ok)
              .trimmed();
      if (!ok || token.isEmpty()) return;
      taiga::accounts.setAnilistToken(token.toStdString());
      taiga::accounts.setAnilistAuthenticated(false);
      row->pending = true;
      updateLoginRows();
      taiga::sync::anilist::Service::instance()->authenticateUser();
      break;
    }

    case ServiceId::MyAnimeList: {
      std::string codeVerifier;
      const auto url = taiga::sync::myanimelist::authorizationCodeUrl(codeVerifier);
      QDesktopServices::openUrl(QUrl{QString::fromStdString(url)});
      bool ok = false;
      const auto code =
          QInputDialog::getText(dialog_, tr("Log in to %1").arg(name),
                                tr("MyAnimeList has opened in your browser. Log in, select "
                                   "\"Allow\", then copy the code shown on the page and paste it "
                                   "here:"),
                                QLineEdit::Normal, {}, &ok)
              .trimmed();
      if (!ok || code.isEmpty()) return;
      taiga::accounts.setMyanimelistAuthenticated(false);
      row->pending = true;
      updateLoginRows();
      taiga::sync::myanimelist::Service::instance()->requestAccessToken(
          code, QString::fromStdString(codeVerifier));
      break;
    }

    case ServiceId::Kitsu: {
      // Kitsu logs in with the email (or username) and password above.
      const auto email = toStdString(ui_->kitsuEmailLineEdit);
      const auto username = toStdString(ui_->kitsuUsernameLineEdit);
      const auto password = ui_->kitsuPasswordLineEdit->text().toStdString();
      if ((email.empty() && username.empty()) || password.empty()) {
        QMessageBox::information(dialog_, tr("Log in to %1").arg(name),
                                 tr("Enter your Kitsu email or username, and your password."));
        return;
      }
      taiga::accounts.setKitsuEmail(email);
      taiga::accounts.setKitsuUsername(username);
      taiga::accounts.setKitsuPassword(password);
      taiga::accounts.setKitsuAuthenticated(false);
      row->pending = true;
      updateLoginRows();
      taiga::sync::kitsu::Service::instance()->authenticateUser();
      break;
    }

    case ServiceId::Unknown:
      break;
  }
}

void SettingsPageAccounts::logOut(const taiga::sync::ServiceId service) {
  using taiga::sync::ServiceId;

  auto& accounts = taiga::accounts;

  switch (service) {
    case ServiceId::AniList:
      accounts.setAnilistToken({});
      accounts.setAnilistUsername({});
      accounts.setAnilistAuthenticated(false);
      break;
    case ServiceId::Kitsu:
      accounts.setKitsuAccessToken({});
      accounts.setKitsuRefreshToken({});
      accounts.setKitsuAuthenticated(false);
      break;
    case ServiceId::MyAnimeList:
      accounts.setMyanimelistAccessToken({});
      accounts.setMyanimelistRefreshToken({});
      accounts.setMyanimelistUsername({});
      accounts.setMyanimelistAuthenticated(false);
      break;
    case ServiceId::Unknown:
      break;
  }

  if (auto* row = loginRow(service)) row->failed = false;
  updateLoginRows();
}

void SettingsPageAccounts::createMirrorGroup() {
  using taiga::sync::ServiceId;

  auto* group = new QGroupBox(tr("Mirror updates to other services"), ui_->accountsPage);
  auto* layout = new QVBoxLayout(group);

  auto* description =
      new QLabel(tr("Changes to your list are also sent to these services. Your main service "
                    "stays the source of truth, and changes made on the others aren't read back."),
                 group);
  description->setWordWrap(true);
  layout->addWidget(description);

  for (const auto service : {ServiceId::AniList, ServiceId::Kitsu, ServiceId::MyAnimeList}) {
    MirrorRow row{.service = service};

    row.widget = new QWidget(group);
    auto* rowLayout = new QHBoxLayout(row.widget);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    row.checkBox =
        new QCheckBox(tr("Also update %1").arg(taiga::sync::serviceName(service)), row.widget);
    row.statusLabel = new QLabel(row.widget);
    row.statusLabel->setEnabled(false);  // secondary text
    row.copyButton = new QPushButton(tr("Copy my list"), row.widget);
    row.retryButton = new QPushButton(tr("Retry failed"), row.widget);

    rowLayout->addWidget(row.checkBox);
    rowLayout->addWidget(row.statusLabel, 1);
    rowLayout->addWidget(row.retryButton);
    rowLayout->addWidget(row.copyButton);

    connect(row.checkBox, &QCheckBox::toggled, this, [this] {
      updateVisibleGroup();
      updateMirrorRows();
    });
    connect(row.copyButton, &QPushButton::clicked, this, [this, service] { copyList(service); });
    connect(row.retryButton, &QPushButton::clicked, this,
            [service] { taiga::sync::mirror.retryFailed(service); });

    layout->addWidget(row.widget);
    mirrorRows_.append(row);
  }

  // Right below the main service, above the account details.
  ui_->verticalLayout_13->insertWidget(1, group);
}

void SettingsPageAccounts::updateMirrorRows() {
  const auto mainSlug = ui_->serviceComboBox->currentData().toString();
  const auto savedMirrors = taiga::sync::mirror.services();

  for (const auto& row : mirrorRows_) {
    row.widget->setVisible(taiga::sync::serviceSlug(row.service) != mainSlug);

    // Copying needs the saved settings, so that the changes are actually sent.
    const bool saved = savedMirrors.contains(row.service) &&
                       mainSlug == taiga::sync::serviceSlug(taiga::sync::currentServiceId());
    const auto status = taiga::sync::mirror.status(row.service);

    QStringList parts;
    if (row.checkBox->isChecked() && saved) {
      if (taiga::sync::mirror.isProcessing(row.service) || status.pending > 0) {
        parts.append(tr("%n change(s) to send", nullptr, status.pending));
      } else if (status.failed == 0) {
        parts.append(tr("No changes to send"));
      }
      if (status.failed > 0) parts.append(tr("%n failed", nullptr, status.failed));
      if (!taiga::sync::isUserAuthenticated(row.service) &&
          !taiga::sync::willAuthenticate(row.service)) {
        parts.append(tr("Not logged in"));
      }
    }
    row.statusLabel->setText(parts.join(u" · "_s));
    row.statusLabel->setToolTip(status.lastError);

    row.copyButton->setEnabled(row.checkBox->isChecked() && saved);
    row.copyButton->setToolTip(saved ? QString{} : tr("Save the settings first."));
    row.retryButton->setVisible(row.checkBox->isChecked() && status.failed > 0);
  }
}

void SettingsPageAccounts::copyList(const taiga::sync::ServiceId service) {
  const auto name = taiga::sync::serviceName(service);
  const auto count = static_cast<int>(std::ranges::count_if(anime::db.entries(), [](const auto& e) {
    return !e.pending_delete && e.status != anime::list::Status::NotInList;
  }));

  const auto answer = QMessageBox::question(
      dialog_, tr("Copy my list to %1").arg(name),
      tr("Send all %n anime in your list to %1? Matching entries there will be updated to match "
         "your list. This runs in the background and can take a while for large lists.",
         nullptr, count)
          .arg(name));

  if (answer == QMessageBox::Yes) taiga::sync::mirror.copyAll(service);
}

void SettingsPageAccounts::load() {
  const auto slug = QString::fromStdString(taiga::settings.service());
  ui_->serviceComboBox->setCurrentIndex(std::max(0, ui_->serviceComboBox->findData(slug)));
  updateVisibleGroup();

  ui_->syncEnabledCheckBox->setChecked(taiga::settings.syncEnabled());

  const auto mirrors = taiga::settings.mirrorServices();
  for (const auto& row : mirrorRows_) {
    const auto slug = taiga::sync::serviceSlug(row.service).toStdString();
    row.checkBox->setChecked(std::ranges::contains(mirrors, slug));
  }
  updateMirrorRows();

  const auto& accounts = taiga::accounts;

  ui_->kitsuEmailLineEdit->setText(toQString(accounts.kitsuEmail()));
  ui_->kitsuUsernameLineEdit->setText(toQString(accounts.kitsuUsername()));
  ui_->kitsuPasswordLineEdit->setText(toQString(accounts.kitsuPassword()));

  updateLoginRows();
}

void SettingsPageAccounts::apply() const {
  taiga::settings.setService(ui_->serviceComboBox->currentData().toString().toStdString());
  taiga::settings.setSyncEnabled(ui_->syncEnabledCheckBox->isChecked());

  const auto mainSlug = ui_->serviceComboBox->currentData().toString();
  std::vector<std::string> mirrors;
  for (const auto& row : mirrorRows_) {
    const auto slug = taiga::sync::serviceSlug(row.service);
    if (row.checkBox->isChecked() && slug != mainSlug) {
      mirrors.push_back(slug.toStdString());
    } else {
      taiga::sync::mirror.clear(row.service);
    }
  }
  taiga::settings.setMirrorServices(mirrors);

  auto& accounts = taiga::accounts;

  accounts.setKitsuEmail(toStdString(ui_->kitsuEmailLineEdit));
  accounts.setKitsuUsername(toStdString(ui_->kitsuUsernameLineEdit));
  accounts.setKitsuPassword(ui_->kitsuPasswordLineEdit->text().toStdString());

  taiga::sync::mirror.process();
}

void SettingsPageAccounts::updateVisibleGroup() {
  using taiga::sync::ServiceId;

  // The main service, and the services that changes are mirrored to.
  const auto slug = ui_->serviceComboBox->currentData().toString();
  const auto isShown = [this, &slug](const ServiceId service) {
    if (slug == taiga::sync::serviceSlug(service)) return true;
    return std::ranges::any_of(mirrorRows_, [service](const MirrorRow& row) {
      return row.service == service && row.checkBox->isChecked();
    });
  };

  ui_->anilistGroupBox->setVisible(isShown(ServiceId::AniList));
  ui_->kitsuGroupBox->setVisible(isShown(ServiceId::Kitsu));
  ui_->myanimelistGroupBox->setVisible(isShown(ServiceId::MyAnimeList));

  updateMirrorRows();
}

}  // namespace gui
