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

#include "base/string.hpp"
#include "track/player_media.hpp"

using namespace track::media;

class TestPlayerMedia final : public QObject {
  Q_OBJECT

private slots:
  void recognizesVideoFiles() {
    QVERIFY(isVideoFilePath(u"/home/a/Anime/[SubsPlease] Dandadan - 07 (1080p).mkv"));
    QVERIFY(isVideoFilePath(u"/Volumes/Media/Frieren - 01.MP4"));
    QVERIFY(!isVideoFilePath(u"/usr/lib/libmpv.so.2"));
    QVERIFY(!isVideoFilePath(u"/home/a/subs.ass"));
    QVERIFY(!isVideoFilePath(u"/home/a/no-extension"));
    QVERIFY(!isVideoFilePath(u"/home/a/trailing."));
  }

  void recognizesWebBrowsers() {
    QVERIFY(isWebBrowser(u"Mozilla Firefox"));
    QVERIFY(isWebBrowser(u"org.mpris.MediaPlayer2.chromium.instance123"));
    QVERIFY(!isWebBrowser(u"VLC media player"));
    QVERIFY(!isWebBrowser(u"mpv"));
  }

  void parsesLsofOutput() {
    const QByteArray output =
        "p123\ncmpv\nn/dev/null\nn/Users/a/Anime/Dandadan - 07.mkv\n"
        "p456\ncIINA\nn/Users/a/Anime/Frieren - 01.mp4\nn/Users/a/Library/x.db\n";
    const auto files = parseLsofOutput(output);
    QCOMPARE(files.size(), 2u);
    QCOMPARE(files[0].pid, 123);
    QCOMPARE(files[0].command, u"mpv"_s);
    QCOMPARE(files[0].path, u"/Users/a/Anime/Dandadan - 07.mkv"_s);
    QCOMPARE(files[1].command, u"IINA"_s);
  }

  void prefersPlayingLocalFiles() {
    const std::vector<PlayerMedia> candidates{
        {.player = u"Firefox"_s,
         .title = u"Dandadan Episode 7"_s,
         .playing = true,
         .webBrowser = true},
        {.player = u"VLC"_s, .file = u"/a/Frieren - 01.mkv"_s, .playing = false},
        {.player = u"Celluloid"_s, .file = u"/a/Dandadan - 07.mkv"_s, .playing = true},
    };
    const auto picked = pickMedia(candidates, true);
    QVERIFY(picked);
    QCOMPARE(picked->player, u"Celluloid"_s);
  }

  void skipsWebBrowsersUnlessAllowed() {
    const std::vector<PlayerMedia> candidates{
        {.player = u"Firefox"_s,
         .title = u"Dandadan Episode 7"_s,
         .playing = true,
         .webBrowser = true},
    };
    QVERIFY(pickMedia(candidates, false) == nullptr);
    QVERIFY(pickMedia(candidates, true) != nullptr);
  }

  void skipsEmptyMedia() {
    const std::vector<PlayerMedia> candidates{{.player = u"mpv"_s, .playing = true}};
    QVERIFY(pickMedia(candidates, true) == nullptr);
  }
};

QTEST_APPLESS_MAIN(TestPlayerMedia)
#include "test_player_media.moc"
