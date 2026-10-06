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
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <string>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "sync/mirror.hpp"
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

  createMirrorGroup();
  connect(&taiga::sync::mirror, &taiga::sync::Mirror::changed, this,
          &SettingsPageAccounts::updateMirrorRows);
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

  ui_->anilistUsernameLineEdit->setText(toQString(accounts.anilistUsername()));
  ui_->anilistTokenLineEdit->setText(toQString(accounts.anilistToken()));

  ui_->kitsuEmailLineEdit->setText(toQString(accounts.kitsuEmail()));
  ui_->kitsuUsernameLineEdit->setText(toQString(accounts.kitsuUsername()));
  ui_->kitsuPasswordLineEdit->setText(toQString(accounts.kitsuPassword()));
  ui_->kitsuAccessTokenLineEdit->setText(toQString(accounts.kitsuAccessToken()));
  ui_->kitsuRefreshTokenLineEdit->setText(toQString(accounts.kitsuRefreshToken()));

  ui_->myanimelistUsernameLineEdit->setText(toQString(accounts.myanimelistUsername()));
  ui_->myanimelistAccessTokenLineEdit->setText(toQString(accounts.myanimelistAccessToken()));
  ui_->myanimelistRefreshTokenLineEdit->setText(toQString(accounts.myanimelistRefreshToken()));
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

  accounts.setAnilistUsername(toStdString(ui_->anilistUsernameLineEdit));
  accounts.setAnilistToken(toStdString(ui_->anilistTokenLineEdit));

  accounts.setKitsuEmail(toStdString(ui_->kitsuEmailLineEdit));
  accounts.setKitsuUsername(toStdString(ui_->kitsuUsernameLineEdit));
  accounts.setKitsuPassword(ui_->kitsuPasswordLineEdit->text().toStdString());
  accounts.setKitsuAccessToken(toStdString(ui_->kitsuAccessTokenLineEdit));
  accounts.setKitsuRefreshToken(toStdString(ui_->kitsuRefreshTokenLineEdit));

  accounts.setMyanimelistUsername(toStdString(ui_->myanimelistUsernameLineEdit));
  accounts.setMyanimelistAccessToken(toStdString(ui_->myanimelistAccessTokenLineEdit));
  accounts.setMyanimelistRefreshToken(toStdString(ui_->myanimelistRefreshTokenLineEdit));

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
