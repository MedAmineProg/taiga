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

#include <QFrame>
#include <functional>

class QLabel;
class QPushButton;
class QTimer;

namespace gui {

// A brief message shown over the bottom of its parent, with an optional action (e.g. Undo).
class ToastWidget final : public QFrame {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ToastWidget)

public:
  static constexpr int kDurationMs = 8000;

  explicit ToastWidget(QWidget* parent);
  ~ToastWidget() = default;

  void showMessage(const QString& text, const QString& actionText = {},
                   std::function<void()> action = {});
  void dismiss();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private:
  void reposition();
  void tick();

  QLabel* m_label = nullptr;
  QPushButton* m_actionButton = nullptr;
  QTimer* m_timer = nullptr;
  std::function<void()> m_action;
  int m_remainingMs = 0;
};

}  // namespace gui
