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

#include <QTest>

#include "track/media_select.hpp"

using anisthesia::MediaInfoType;
using namespace track::media;

namespace {

bool isVideoPath(const std::string& path) {
  for (const auto* extension : {".mkv", ".mp4", ".avi"}) {
    if (path.ends_with(extension)) return true;
  }
  return false;
}

const int kWindowA = 1;
const int kWindowB = 2;

WindowMedia mpcHc(const void* window, std::vector<anisthesia::MediaInfo> information) {
  return {.player = "MPC-HC", .window = window, .information = std::move(information)};
}

}  // namespace

class TestMediaSelect : public QObject {
  Q_OBJECT

private slots:
  void picksTheVideoAmongOtherOpenFiles() {
    // MPC-HC also has its settings, subtitles and fonts open, and the video isn't first.
    const std::vector<WindowMedia> candidates{
        mpcHc(&kWindowA, {
                             {MediaInfoType::File, R"(C:\Program Files\MPC-HC\mpc-hc64.ini)"},
                             {MediaInfoType::File, R"(D:\Anime\Fonts\Roboto.ttf)"},
                             {MediaInfoType::File, R"(D:\Anime\[SubsPlease] Dandadan - 07.ass)"},
                             {MediaInfoType::File, R"(D:\Anime\[SubsPlease] Dandadan - 07.mkv)"},
                             {MediaInfoType::Title, "[SubsPlease] Dandadan - 07.mkv"},
                         })};

    const auto selected = selectMedia(candidates, nullptr, false, isVideoPath);
    QVERIFY(selected);
    QCOMPARE(selected->index, 0);
    QCOMPARE(selected->file, R"(D:\Anime\[SubsPlease] Dandadan - 07.mkv)");
  }

  void skipsBrowsersUnlessStreamingIsEnabled() {
    const std::vector<WindowMedia> candidates{
        {.player = "Google Chrome",
         .webBrowser = true,
         .window = &kWindowB,
         .information = {{MediaInfoType::Title, "Inbox - Gmail"}}},
        mpcHc(&kWindowA, {{MediaInfoType::File, R"(D:\Anime\Frieren - 03.mkv)"}}),
    };

    auto selected = selectMedia(candidates, nullptr, false, isVideoPath);
    QVERIFY(selected);
    QCOMPARE(selected->index, 1);

    // With streaming detection, the browser comes first and its title is used.
    selected = selectMedia(candidates, nullptr, true, isVideoPath);
    QVERIFY(selected);
    QCOMPARE(selected->index, 0);
    QCOMPARE(selected->title, "Inbox - Gmail");
  }

  void followsTheCurrentWindow() {
    const std::vector<WindowMedia> candidates{
        mpcHc(&kWindowA, {{MediaInfoType::File, R"(D:\Anime\Frieren - 03.mkv)"}}),
        mpcHc(&kWindowB, {{MediaInfoType::File, R"(D:\Anime\Dandadan - 07.mkv)"}}),
    };

    QCOMPARE(selectMedia(candidates, nullptr, false, isVideoPath)->index, 0);
    QCOMPARE(selectMedia(candidates, &kWindowB, false, isVideoPath)->index, 1);
  }

  void ignoresPlayersWithoutAVideo() {
    // An idle player only has its settings open, and its title is just the player's name.
    const std::vector<WindowMedia> candidates{
        mpcHc(&kWindowA,
              {
                  {MediaInfoType::File, R"(C:\Program Files\MPC-HC\mpc-hc64.ini)"},
                  {MediaInfoType::Title, "MPC-HC"},
              }),
    };
    QVERIFY(!selectMedia(candidates, nullptr, false, isVideoPath));
    QVERIFY(!selectMedia({}, nullptr, false, isVideoPath));
  }

  void fallsBackToTheTitleWithoutFiles() {
    // Players read through their window title only report a title.
    const std::vector<WindowMedia> candidates{
        {.player = "PotPlayer",
         .window = &kWindowA,
         .information = {{MediaInfoType::Title, "Sousou no Frieren - 03"}}},
    };
    const auto selected = selectMedia(candidates, nullptr, false, isVideoPath);
    QVERIFY(selected);
    QVERIFY(selected->file.empty());
    QCOMPARE(selected->title, "Sousou no Frieren - 03");
  }
};

QTEST_GUILESS_MAIN(TestMediaSelect)
#include "test_media_select.moc"
