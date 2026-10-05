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
#include "command_palette.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>
#include <ranges>

#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/fuzzy.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"

namespace gui {

namespace {

constexpr int kMaxResults = 50;
constexpr int kMaxVisibleRows = 10;
constexpr int kDetailRole = Qt::UserRole + 1;
constexpr int kCommandIndexRole = Qt::UserRole + 2;

// Draws the detail text (e.g. a shortcut or list status) dimmed and right-aligned.
class CommandItemDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    QStyledItemDelegate::paint(painter, option, index);

    const auto detail = index.data(kDetailRole).toString();
    if (detail.isEmpty()) return;

    painter->save();
    painter->setPen(option.palette.color(QPalette::PlaceholderText));
    const auto rect = option.rect.adjusted(0, 0, -8, 0);
    painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, detail);
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(size.height(), option.fontMetrics.height() + 12));
    return size;
  }
};

}  // namespace

CommandPalette::CommandPalette(MainWindow* mainWindow)
    : QDialog(mainWindow, Qt::Popup | Qt::FramelessWindowHint), m_mainWindow(mainWindow) {
  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  m_lineEdit = new QLineEdit(this);
  m_lineEdit->setPlaceholderText(tr("Search for anime, pages and actions..."));
  m_lineEdit->setClearButtonEnabled(true);
  m_lineEdit->installEventFilter(this);
  layout->addWidget(m_lineEdit);

  m_listWidget = new QListWidget(this);
  m_listWidget->setFrameShape(QFrame::NoFrame);
  m_listWidget->setUniformItemSizes(true);
  m_listWidget->setIconSize({16, 16});
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setItemDelegate(new CommandItemDelegate(m_listWidget));
  m_listWidget->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_listWidget);

  connect(m_lineEdit, &QLineEdit::textChanged, this, &CommandPalette::filter);
  connect(m_lineEdit, &QLineEdit::returnPressed, this, &CommandPalette::runCurrent);
  connect(m_listWidget, &QListWidget::itemActivated, this, &CommandPalette::runCurrent);
  connect(m_listWidget, &QListWidget::itemClicked, this, &CommandPalette::runCurrent);
}

void CommandPalette::popup() {
  buildCommands();

  m_lineEdit->clear();
  filter({});

  const auto windowRect = m_mainWindow->geometry();
  const int width = std::min(640, windowRect.width() - 48);
  setFixedWidth(width);
  move(windowRect.left() + (windowRect.width() - width) / 2, windowRect.top() + 64);

  show();
  raise();
  activateWindow();
  m_lineEdit->setFocus();
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_lineEdit && event->type() == QEvent::KeyPress) {
    const auto keyEvent = static_cast<QKeyEvent*>(event);
    switch (keyEvent->key()) {
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        // Let the user move through the results without leaving the search box.
        QApplication::sendEvent(m_listWidget, event);
        return true;
      case Qt::Key_Escape:
        close();
        return true;
      default:
        break;
    }
  }

  return QDialog::eventFilter(watched, event);
}

void CommandPalette::buildCommands() {
  m_commands.clear();

  const auto goTo = tr("Go to");

  // Pages
  const auto addPage = [&](const QString& text, const QString& icon, auto&& run) {
    m_commands.append({
        .text = text,
        .detail = goTo,
        .icon = theme.getIcon(icon),
        .priority = 3,
        .run = run,
    });
  };
  addPage(tr("Home"), u"home"_s, [this] { m_mainWindow->navigateTo(MainWindowPage::Home); });
  addPage(tr("Search"), u"search"_s, [this] { m_mainWindow->navigateTo(MainWindowPage::Search); });
  for (const auto status : anime::list::kStatuses) {
    addPage(formatListStatus(status), u"list_alt"_s,
            [this, status] { m_mainWindow->navigateToListStatus(status); });
  }
  addPage(tr("History"), u"history"_s,
          [this] { m_mainWindow->navigateTo(MainWindowPage::History); });
  addPage(tr("Library"), u"folder"_s,
          [this] { m_mainWindow->navigateTo(MainWindowPage::Library); });

  // Actions
  for (const auto action : m_mainWindow->findChildren<QAction*>(Qt::FindDirectChildrenOnly)) {
    if (action->isSeparator() || action->menu() || !action->isEnabled()) continue;
    if (action->iconText().isEmpty()) continue;

    QString detail = action->shortcut().toString(QKeySequence::NativeText);
    if (action->isCheckable()) detail = action->isChecked() ? tr("On") : tr("Off");

    m_commands.append({
        .text = action->iconText(),
        .detail = detail,
        .keywords = {action->toolTip()},
        .icon = action->icon(),
        .priority = 2,
        .run = [action] { action->trigger(); },
    });
  }

  // Anime in the list
  for (const auto& entry : anime::db.entries()) {
    const auto item = anime::db.item(entry.anime_id);
    if (!item) continue;

    QStringList keywords;
    for (const auto& title : {item->titles.romaji, item->titles.english, item->titles.japanese}) {
      if (!title.empty()) keywords.append(QString::fromStdString(title));
    }
    for (const auto& synonym : item->titles.synonyms) {
      keywords.append(QString::fromStdString(synonym));
    }

    const int id = item->id;
    m_commands.append({
        .text = QString::fromStdString(anime::preferredTitle(*item)),
        .detail = formatListStatus(entry.status),
        .keywords = keywords,
        .priority = entry.status == anime::list::Status::Watching ? 1 : 0,
        .run =
            [this, id] {
              if (const auto item = anime::db.item(id)) {
                MediaDialog::show(m_mainWindow, MediaDialogPage::Details, *item);
              }
            },
    });
  }
}

void CommandPalette::filter(const QString& query) {
  struct Match {
    int score;
    qsizetype index;
  };

  QList<Match> matches;
  for (qsizetype i = 0; i < m_commands.size(); ++i) {
    const auto& command = m_commands[i];

    auto best = fuzzyScore(query, command.text);
    for (const auto& keyword : command.keywords) {
      // Matches in the displayed text are preferred over hidden keywords.
      if (const auto score = fuzzyScore(query, keyword); score && (!best || *score - 50 > *best)) {
        best = *score - 50;
      }
    }

    if (best) matches.append({*best, i});
  }

  std::ranges::stable_sort(matches, [this, &query](const Match& a, const Match& b) {
    if (query.trimmed().isEmpty()) {
      return m_commands[a.index].priority > m_commands[b.index].priority;
    }
    return a.score > b.score;
  });

  m_listWidget->clear();
  for (const auto& match : matches | std::views::take(kMaxResults)) {
    const auto& command = m_commands[match.index];
    auto item = new QListWidgetItem(command.icon, command.text, m_listWidget);
    item->setData(kDetailRole, command.detail);
    item->setData(kCommandIndexRole, static_cast<int>(match.index));
  }
  m_listWidget->setCurrentRow(0);

  const int rows = std::clamp(m_listWidget->count(), 1, kMaxVisibleRows);
  const int rowHeight = m_listWidget->count() > 0 ? m_listWidget->sizeHintForRow(0) : 0;
  m_listWidget->setFixedHeight(rows * rowHeight + 2 * m_listWidget->frameWidth());
  m_listWidget->setVisible(m_listWidget->count() > 0);
  adjustSize();
}

void CommandPalette::runCurrent() {
  const auto item = m_listWidget->currentItem();
  if (!item) return;

  const auto index = item->data(kCommandIndexRole).toInt();
  if (index < 0 || index >= m_commands.size()) return;

  // Close first, so that dialogs opened by the command get focus.
  const auto run = m_commands[index].run;
  close();
  if (run) run();
}

}  // namespace gui
