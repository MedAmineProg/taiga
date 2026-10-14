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

#include "media_select.hpp"

#include <algorithm>
#include <numeric>

namespace track::media {

namespace {

std::optional<SelectedMedia> selectFrom(const WindowMedia& candidate, const std::size_t index,
                                        const bool streamingEnabled,
                                        const VideoPathPredicate& isVideoPath) {
  if (candidate.webBrowser && !streamingEnabled) return std::nullopt;

  SelectedMedia selected{.index = index};
  bool hasFiles = false;

  for (const auto& information : candidate.information) {
    switch (information.type) {
      case anisthesia::MediaInfoType::File:
        hasFiles = true;
        if (isVideoPath(information.value)) {
          selected.file = information.value;
          return selected;
        }
        break;
      case anisthesia::MediaInfoType::Title:
        if (selected.title.empty()) selected.title = information.value;
        break;
      case anisthesia::MediaInfoType::Unknown:
        // Window titles are preferred over page titles reported via UI Automation.
        selected.title = information.value;
        break;
      case anisthesia::MediaInfoType::Url:
        selected.url = information.value;
        break;
      case anisthesia::MediaInfoType::Tab:
        break;
    }
  }

  // A player with only other files open (e.g. its settings) isn't playing a video.
  if (hasFiles && !candidate.webBrowser) return std::nullopt;
  if (selected.title.empty()) return std::nullopt;

  return selected;
}

}  // namespace

std::optional<SelectedMedia> selectMedia(const std::vector<WindowMedia>& candidates,
                                         const void* currentWindow, const bool streamingEnabled,
                                         const VideoPathPredicate& isVideoPath) {
  std::vector<std::size_t> order(candidates.size());
  std::iota(order.begin(), order.end(), std::size_t{0});

  if (currentWindow) {
    // std::ranges::stable_partition isn't in every standard library yet (e.g. libc++).
    std::stable_partition(order.begin(), order.end(), [&](const std::size_t i) {
      return candidates[i].window == currentWindow;
    });
  }

  for (const auto i : order) {
    if (auto selected = selectFrom(candidates[i], i, streamingEnabled, isVideoPath)) {
      return selected;
    }
  }

  return std::nullopt;
}

}  // namespace track::media
