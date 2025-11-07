#include <algorithm>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qlatin1stringview.h>
#include <qlogging.h>
#include <qnetworkrequest.h>
#include <utility>

#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "netease/neteaseservice.h"
#include "neteasecoverprovider.h"

using namespace Qt::Literals::StringLiterals;

NeteaseCoverProvider::NeteaseCoverProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Netease"_s, true, false, 2.0, true, true, network, parent) {}

bool NeteaseCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if ((artist.isEmpty() && album.isEmpty() && title.isEmpty())) return false;

  QStringList parts;
  if (!artist.isEmpty()) parts << artist;
  if (!album.isEmpty())  parts << album;
  if (!title.isEmpty())  parts << title;

  QString search_query = parts.join(u" "_s);

  ParamList params = ParamList() << Param(u"type"_s, u"1"_s)
                                 << Param(u"s"_s, search_query);
  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)),QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(NeteaseService::kApiUrl) + QLatin1String("/api/cloudsearch/pc"));
  url.setQuery(url_query);

  QNetworkRequest network_request(url);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  // network_request.setHeader(QNetworkRequest::SetCookieHeader, service_->cookie());
  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void NeteaseCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

JsonBaseRequest::JsonObjectResult NeteaseCoverProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains(u"msg"_s) && json_object.contains("code"_L1)) {
        const int code = json_object["code"_L1].toInt();
        const QString message = json_object[u"msg"_s].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}

void NeteaseCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonValue value_result;
  if (json_object.contains("result"_L1)) {
    value_result = json_object["result"_L1];
  } else {
    Error("Json reply is missing result"_L1, json_object);
    return;
  }

  if (!value_result.isObject()) {
    Error("Json result is not a object."_L1, value_result);
    return;
  }
  const QJsonObject object_songs = value_result.toObject();

  if (!object_songs.contains("songs"_L1)) {
    Error("Json result object does not contain songs."_L1, object_songs);
    return;
  }
  const QJsonValue value_songs = object_songs["songs"_L1];

  if (!value_songs.isArray()) {
    Error("Json result object songs is not a array."_L1, value_songs);
    return;
  }
  const QJsonArray array_songs = value_songs.toArray();

  for (const QJsonValue &value : array_songs) {

    if (!value.isObject()) {
      Error("Invalid Json reply, value in items is not a object."_L1);
      continue;
    }
    const QJsonObject object_song = value.toObject();

    if (!object_song.contains("al"_L1) || !object_song.contains("ar"_L1)) {
      Error("Invalid Json reply, song is missing artist or album."_L1, object_song);
      continue;
    }

    const QJsonArray array_artists = object_song["ar"_L1].toArray();

    QString artist;
    if (!array_artists.isEmpty()) {
      const QJsonValue first_artist = array_artists.first();
      if (!first_artist.isObject() || !first_artist.toObject().contains("name"_L1)) {
        Error("Invalid Json reply, artist entry missing name."_L1, first_artist);
        continue;
      }
      artist = first_artist.toObject()["name"_L1].toString();
    }

    const QJsonValue value_album = object_song["al"_L1];
    if (!value_album.isObject()) {
      Error("Invalid Json reply, album is not a object."_L1, value_album);
      continue;
    }
    const QJsonObject object_album = value_album.toObject();

    if (!object_album.contains("name"_L1) || !object_album.contains("picUrl"_L1)) {
      Error("Invalid Json reply, album missing name or picUrl."_L1, object_album);
      continue;
    }

    const QString album = object_album["name"_L1].toString();
    const QUrl cover_url(object_album["picUrl"_L1].toString());

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);
    cover_result.image_url = cover_url;
    cover_result.image_size = QSize(800, 800);
    results << cover_result;
  }
}

void NeteaseCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Netease:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
