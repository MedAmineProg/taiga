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
#include "toast_widget.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <chrono>

#include "base/string.hpp"
#include "gui/utils/theme.hpp"

namespace gui {

namespace {

constexpr int kTickMs = 250;
constexpr int kMargin = 24;
constexpr int kMaxWidth = 560;

}  // namespace

ToastWidget::ToastWidget(QWidget* parent) : QFrame(parent) {
  setAttribute(Qt::WA_StyledBackground, false);
  hide();

  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 8, 8, 8);
  layout->setSpacing(8);

  m_label = new QLabel(this);
  m_label->setTextFormat(Qt::PlainText);
  m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  layout->addWidget(m_label, 1);

  m_actionButton = new QPushButton(this);
  m_actionButton->setFlat(true);
  m_actionButton->setCursor(Qt::PointingHandCursor);
  connect(m_actionButton, &QPushButton::clicked, this, [this]() {
    const auto action = m_action;
    dismiss();
    if (action) action();
  });
  layout->addWidget(m_actionButton);

  auto closeButton = new QToolButton(this);
  closeButton->setText(u"✕"_s);
  closeButton->setAutoRaise(true);
  closeButton->setToolTip(tr("Dismiss"));
  connect(closeButton, &QToolButton::clicked, this, &ToastWidget::dismiss);
  layout->addWidget(closeButton);

  m_timer = new QTimer(this);
  m_timer->setInterval(std::chrono::milliseconds{kTickMs});
  connect(m_timer, &QTimer::timeout, this, &ToastWidget::tick);

  parent->installEventFilter(this);
}

void ToastWidget::showMessage(const QString& text, const QString& actionText,
                              std::function<void()> action) {
  // Inverted colors stand out from the window, like a tooltip.
  const bool dark = theme.isDark();
  auto palette = this->palette();
  palette.setColor(QPalette::WindowText, dark ? QColor(0x20, 0x21, 0x24) : Qt::white);
  palette.setColor(QPalette::ButtonText,
                   dark ? QColor(0x1c, 0x5c, 0xab) : QColor(0x86, 0xb6, 0xef));
  setPalette(palette);
  m_label->setPalette(palette);
  m_actionButton->setPalette(palette);

  m_label->setText(text);
  m_label->setToolTip(text);
  m_action = std::move(action);
  m_actionButton->setText(actionText);
  m_actionButton->setVisible(!actionText.isEmpty() && m_action);

  m_remainingMs = kDurationMs;
  m_timer->start();

  reposition();
  show();
  raise();
}

void ToastWidget::dismiss() {
  m_timer->stop();
  m_action = {};
  hide();
}

bool ToastWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == parent() && event->type() == QEvent::Resize && isVisible()) reposition();
  return QFrame::eventFilter(watched, event);
}

void ToastWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(theme.isDark() ? QColor(0xe8, 0xea, 0xed, 245) : QColor(0x20, 0x21, 0x24, 240));
  painter.drawRoundedRect(rect(), 8, 8);
}

void ToastWidget::reposition() {
  const auto parent = parentWidget();
  if (!parent) return;
  const int width = std::min(kMaxWidth, parent->width() - 2 * kMargin);
  const int height = sizeHint().height();
  setGeometry((parent->width() - width) / 2, parent->height() - height - kMargin, width, height);
}

void ToastWidget::tick() {
  // Count down only while the message can be seen and isn't being read.
  if (!window()->isActiveWindow() || underMouse()) return;
  m_remainingMs -= kTickMs;
  if (m_remainingMs <= 0) dismiss();
}

}  // namespace gui
