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
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "base/string.hpp"
#include "link/discord_frames.hpp"
#include "link/discord_ipc.hpp"
#include "taiga/discord_activity.hpp"

using namespace discord;

class TestDiscord final : public QObject {
  Q_OBJECT

private slots:
  void encodesAndReadsFrames() {
    const QJsonObject payload{{"cmd", "DISPATCH"}, {"evt", "READY"}};
    auto data = encodeFrame(Opcode::Frame, payload);
    data += encodeFrame(Opcode::Ping, QJsonObject{{"n", 1}});

    FrameReader reader;
    // Bytes may arrive in pieces.
    reader.append(data.left(5));
    QVERIFY(!reader.next().has_value());
    reader.append(data.mid(5));

    const auto first = reader.next();
    QVERIFY(first.has_value());
    QCOMPARE(first->opcode, Opcode::Frame);
    QCOMPARE(first->payload, payload);

    const auto second = reader.next();
    QVERIFY(second.has_value());
    QCOMPARE(second->opcode, Opcode::Ping);
    QVERIFY(!reader.next().has_value());
    QVERIFY(!reader.hasError());
  }

  void rejectsInvalidFrames() {
    FrameReader reader;
    QByteArray bad(8, '\xff');  // unknown opcode, huge length
    reader.append(bad);
    QVERIFY(!reader.next().has_value());
    QVERIFY(reader.hasError());
  }

  void buildsActivity() {
    const auto activity = activityJson({
        .details = u"Sousou no Frieren"_s,
        .state = u"Episode 5 of 28"_s,
        .largeImage = u"https://example.com/poster.jpg"_s,
        .largeText = u"Sousou no Frieren"_s,
        .smallImage = u"anilist"_s,
        .smallText = u"AniList"_s,
        .start = 1000,
        .buttonLabel = u"View anime"_s,
        .buttonUrl = u"https://anilist.co/anime/154587"_s,
    });

    QCOMPARE(activity["details"].toString(), u"Sousou no Frieren"_s);
    QCOMPARE(activity["state"].toString(), u"Episode 5 of 28"_s);
    QCOMPARE(activity["timestamps"]["start"].toInteger(), 1000);
    QCOMPARE(activity["assets"]["large_image"].toString(), u"https://example.com/poster.jpg"_s);
    QCOMPARE(activity["assets"]["small_image"].toString(), u"anilist"_s);
    QCOMPARE(activity["buttons"].toArray().size(), 1);
    QCOMPARE(activity["buttons"][0]["url"].toString(), u"https://anilist.co/anime/154587"_s);
  }

  void keepsActivityWithinLimits() {
    const auto activity = activityJson({
        .details = QString(300, u'a'),
        .state = u"x"_s,  // too short
        .buttonLabel = u"View anime"_s,
        .buttonUrl = u"javascript:alert(1)"_s,  // not a web link
    });

    QCOMPARE(activity["details"].toString().size(), 128);
    QCOMPARE(activity["state"].toString().size(), 2);
    QVERIFY(!activity.contains("buttons"));
    QVERIFY(!activity.contains("assets"));
    QVERIFY(!activity.contains("timestamps"));
  }

  void clearsActivity() {
    const auto payload = setActivityPayload(42, std::nullopt, u"1"_s);
    QCOMPARE(payload["cmd"].toString(), u"SET_ACTIVITY"_s);
    QCOMPARE(payload["args"]["pid"].toInteger(), 42);
    QVERIFY(payload["args"]["activity"].isNull());
  }

  void makesPresenceFromWhatsPlaying() {
    const taiga::NowPlaying playing{
        .title = u"Dandadan"_s,
        .episodes = std::pair{7, 7},
        .episodeCount = 12,
        .group = u"SubsPlease"_s,
        .imageUrl = u"https://example.com/dandadan.jpg"_s,
        .start = 1000,
    };
    const taiga::ServiceInfo service{
        .slug = u"anilist"_s,
        .name = u"AniList"_s,
        .username = u"Amine"_s,
        .animePageUrl = u"https://anilist.co/anime/171018"_s,
    };

    auto presence = taiga::makePresence(playing, {}, service);
    QCOMPARE(presence.details, u"Dandadan"_s);
    QCOMPARE(presence.state, u"Episode 7 of 12 by SubsPlease"_s);
    QCOMPARE(presence.largeImage, u"https://example.com/dandadan.jpg"_s);
    QCOMPARE(presence.smallImage, u"anilist"_s);
    QCOMPARE(presence.smallText, u"Amine at AniList"_s);
    QCOMPARE(presence.start, std::time_t{1000});
    QCOMPARE(presence.buttonUrl, u"https://anilist.co/anime/171018"_s);

    // Everything optional turned off
    presence = taiga::makePresence(
        playing,
        {.showButton = false, .showGroup = false, .showTime = false, .showUsername = false},
        service);
    QCOMPARE(presence.state, u"Episode 7 of 12"_s);
    QCOMPARE(presence.smallText, u"AniList"_s);
    QCOMPARE(presence.start, std::time_t{0});
    QVERIFY(presence.buttonUrl.isEmpty());
  }

  void makesPresenceWithMissingDetails() {
    const auto presence = taiga::makePresence(
        {.title = u"One Piece"_s, .episodes = std::pair{1100, 1101}, .imageUrl = u"http://x"_s}, {},
        {.slug = u"myanimelist"_s, .name = u"MyAnimeList"_s});
    QCOMPARE(presence.state, u"Episode 1100-1101"_s);  // unknown episode count
    QCOMPARE(presence.largeImage, u"default"_s);       // not HTTPS
    QCOMPARE(presence.smallText, u"MyAnimeList"_s);    // no username
  }

  // Talks to a fake Discord over a local socket.
  void connectsAndSetsPresence() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath(u"discord-ipc-0"_s);

    QLocalServer server;
    QVERIFY(server.listen(path));

    Client client;
    client.setClientId(u"379871385176244224"_s);
    // The first candidate doesn't exist, so the client should move on to the next one.
    client.setSocketNames({dir.filePath(u"missing"_s), path});
    client.setPresence(Presence{.details = u"Dandadan"_s, .state = u"Episode 7 of 12"_s});
    QSignalSpy readySpy(&client, &Client::ready);
    client.start();

    QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
    auto connection = server.nextPendingConnection();
    FrameReader reader;
    const auto readFrame = [&]() -> std::optional<Frame> {
      for (int i = 0; i < 50; ++i) {
        if (auto frame = reader.next()) return frame;
        if (connection->bytesAvailable() || connection->waitForReadyRead(100)) {
          reader.append(connection->readAll());
        }
        QCoreApplication::processEvents();
      }
      return std::nullopt;
    };

    // Handshake
    const auto handshake = readFrame();
    QVERIFY(handshake.has_value());
    QCOMPARE(handshake->opcode, Opcode::Handshake);
    QCOMPARE(handshake->payload["client_id"].toString(), u"379871385176244224"_s);
    QCOMPARE(handshake->payload["v"].toInt(), 1);

    connection->write(encodeFrame(Opcode::Frame, {{"cmd", "DISPATCH"}, {"evt", "READY"}}));
    connection->flush();
    QTRY_COMPARE(readySpy.count(), 1);
    QVERIFY(client.isReady());

    // The presence set before connecting is sent once ready.
    const auto activity = readFrame();
    QVERIFY(activity.has_value());
    QCOMPARE(activity->payload["cmd"].toString(), u"SET_ACTIVITY"_s);
    QCOMPARE(activity->payload["args"]["activity"]["details"].toString(), u"Dandadan"_s);

    // Pings are answered
    connection->write(encodeFrame(Opcode::Ping, {{"n", 7}}));
    connection->flush();
    const auto pong = readFrame();
    QVERIFY(pong.has_value());
    QCOMPARE(pong->opcode, Opcode::Pong);

    // Clearing
    client.setPresence(std::nullopt);
    const auto cleared = readFrame();
    QVERIFY(cleared.has_value());
    QVERIFY(cleared->payload["args"]["activity"].isNull());
  }
};

QTEST_GUILESS_MAIN(TestDiscord)
#include "test_discord.moc"
