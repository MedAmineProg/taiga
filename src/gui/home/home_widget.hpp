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
#include <QList>
#include <ctime>

#include "gui/common/page_widget.hpp"

class QLabel;
class QTimer;
class QVBoxLayout;

namespace gui {

class PosterWidget;

// The home page: what to watch next, and what's airing soon.
class HomeWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(HomeWidget)

public:
  explicit HomeWidget(QWidget* parent);
  ~HomeWidget() = default;

public slots:
  void refresh();

protected:
  void showEvent(QShowEvent* event) override;

private:
  struct Countdown {
    QLabel* label = nullptr;
    std::time_t time = 0;
  };

  void scheduleRefresh();
  void updateCountdowns();
  void updatePoster(int animeId);
  QWidget* createContinueSection();
  QWidget* createAiringSection();
  QLabel* createSectionTitle(const QString& text);
  QLabel* createEmptyLabel(const QString& text);
  PosterWidget* createPoster(int animeId, QSize size);

  QVBoxLayout* m_contentLayout = nullptr;
  QWidget* m_content = nullptr;
  QLabel* m_summaryLabel = nullptr;
  QTimer* m_refreshTimer = nullptr;
  QTimer* m_countdownTimer = nullptr;
  QHash<int, QList<PosterWidget*>> m_posters;
  QList<Countdown> m_countdowns;
};

}  // namespace gui
