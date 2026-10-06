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

#include <QList>

#include "gui/settings/settings_page.hpp"
#include "sync/service.hpp"

class QCheckBox;
class QLabel;
class QPushButton;

namespace gui {

class SettingsPageAccounts final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SettingsPageAccounts)

public:
  SettingsPageAccounts(Ui::SettingsDialog* ui, QDialog* dialog);
  ~SettingsPageAccounts() override = default;

  void load() override;
  void apply() const override;

private:
  struct MirrorRow {
    taiga::sync::ServiceId service;
    QWidget* widget = nullptr;
    QCheckBox* checkBox = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* copyButton = nullptr;
    QPushButton* retryButton = nullptr;
  };

  struct LoginRow {
    taiga::sync::ServiceId service;
    QLabel* statusLabel = nullptr;
    QPushButton* logInButton = nullptr;
    QPushButton* logOutButton = nullptr;
    bool pending = false;
    bool failed = false;
  };

  void createLoginRows();
  void updateLoginRows();
  void logIn(const taiga::sync::ServiceId service);
  void logOut(const taiga::sync::ServiceId service);
  LoginRow* loginRow(const taiga::sync::ServiceId service);

  void createMirrorGroup();
  void updateMirrorRows();
  void copyList(const taiga::sync::ServiceId service);
  void updateVisibleGroup();

  QList<LoginRow> loginRows_;
  QList<MirrorRow> mirrorRows_;
};

}  // namespace gui
