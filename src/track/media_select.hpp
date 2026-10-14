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

#include <anisthesia/media.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Chooses what is being played out of everything the media players report. Kept free of
// platform code so that it can be unit-tested.

namespace track::media {

// One player window and the media information found for it.
struct WindowMedia {
  std::string player;
  bool webBrowser = false;
  const void* window = nullptr;
  std::vector<anisthesia::MediaInfo> information;
};

struct SelectedMedia {
  std::size_t index = 0;  // in the given list
  std::string file;       // a video file, if one was found
  std::string title;
  std::string url;
};

using VideoPathPredicate = std::function<bool(const std::string& path)>;

// Players can have many files open (settings, subtitles, fonts...), so the first video file is
// used. The window title is only used for web browsers, and for players that report no files.
// The current window is tried first, so that the same player keeps being followed.
std::optional<SelectedMedia> selectMedia(const std::vector<WindowMedia>& candidates,
                                         const void* currentWindow, const bool streamingEnabled,
                                         const VideoPathPredicate& isVideoPath);

}  // namespace track::media
