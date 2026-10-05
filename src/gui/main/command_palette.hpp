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

#include <QDialog>
#include <QStringList>
#include <functional>

class QLineEdit;
class QListWidget;

namespace gui {

class MainWindow;

// A quick launcher for pages, actions and anime in the list, opened with Ctrl+K.
class CommandPalette final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(CommandPalette)

public:
  explicit CommandPalette(MainWindow* mainWindow);
  ~CommandPalette() override = default;

  void popup();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  struct Command {
    QString text;
    QString detail;  // shown dimmed on the right (e.g. a shortcut)
    QStringList keywords;
    QIcon icon;
    int priority = 0;  // breaks ties with an empty query
    std::function<void()> run;
  };

  void buildCommands();
  void filter(const QString& query);
  void runCurrent();

  MainWindow* m_mainWindow = nullptr;
  QLineEdit* m_lineEdit = nullptr;
  QListWidget* m_listWidget = nullptr;
  QList<Command> m_commands;
};

}  // namespace gui
