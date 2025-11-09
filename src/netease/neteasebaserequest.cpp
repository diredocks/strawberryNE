#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <qlatin1stringview.h>

#include "netease/neteasecrypto.h"
#include "neteaseservice.h"
#include "neteasebaserequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {

static const QStringList kUserAgents = {
    "Mozilla/5.0 (iPhone; CPU iPhone OS 9_1 like Mac OS X) "
    "AppleWebKit/601.1.46 (KHTML, like Gecko) Version/9.0 Mobile/13B143 Safari/601.1"_L1,
    "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.115 Mobile Safari/537.36"_L1,
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36"_L1,
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_5) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.115 Safari/537.36"_L1,
    "Mozilla/5.0 (iPhone; CPU iPhone OS 10_0 like Mac OS X) "
    "AppleWebKit/602.1.38 (KHTML, like Gecko) Version/10.0 Mobile/14A300 Safari/602.1"_L1,
};
inline QByteArray RandomUserAgent() {
    const QString &ua = kUserAgents.at(QRandomGenerator::global()->bounded(kUserAgents.size()));
    return ua.toUtf8();
}

}

NeteaseBaseRequest::NeteaseBaseRequest(NeteaseService *service, QObject *parent)
    : QObject(parent),
      service_(service),
      network_(new QNetworkAccessManager) { }

QNetworkReply *NeteaseBaseRequest::CreatePostRequest(const QString &resource_name, const ParamList &params_provided) const {

  QUrl url(QLatin1String(NeteaseService::kWebApiUrl) + resource_name);

  QList<QNetworkCookie> cookies;
  if (service_->authenticated()) {
    cookies = service_->cookies();
  }
  else if (network_ && network_->cookieJar()) {
    cookies = network_->cookieJar()->cookiesForUrl(QUrl(QLatin1String(NeteaseService::kWebApiUrl)));
  }

  for (const auto &cookie : cookies) {
    if (cookie.name() == QByteArrayLiteral("__csrf")) {
      url.setQuery(QUrlQuery{{"csrf"_L1, QString::fromUtf8(cookie.value())}});
      break;
    }
  }

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded"_L1);
  req.setRawHeader("Accept", "*/*");
  req.setRawHeader("Accept-Language", "en-US,en;q=0.5");
  req.setRawHeader("Connection", "keep-alive");
  req.setRawHeader("Host", "music.163.com");
  req.setRawHeader("Referer", "https://music.163.com");
  req.setRawHeader("User-Agent", RandomUserAgent());

  QStringList cookie_pairs;
  cookie_pairs.reserve(cookies.size() + 2);
  for (const auto &cookie : cookies) {
    cookie_pairs << QString::fromUtf8(cookie.name()) + "="_L1 + QString::fromUtf8(cookie.value());
  }
  cookie_pairs << "os=pc"_L1 << "appver=2.7.1.198277"_L1;
  req.setRawHeader("Cookie", cookie_pairs.join("; "_L1).toUtf8());

  QVariantMap params_map;
  for (const auto &param : params_provided)
    params_map.insert(param.first, param.second);
  const QVariantMap encrypted = NeteaseCrypto::weapi(QJsonDocument::fromVariant(params_map));

  QUrlQuery body_query;
  body_query.addQueryItem("params"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encrypted.value("params"_L1).toString())));
  body_query.addQueryItem("encSecKey"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encrypted.value("encSecKey"_L1).toString())));

  const QByteArray body = body_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(req, body);
  // qLog(Debug) << "Netease: POST" << url;

  return reply;

}

JsonBaseRequest::JsonObjectResult NeteaseBaseRequest::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
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
      if (json_object.contains("msg"_L1) && json_object.contains("code"_L1)) {
        const QString error = json_object["msg"_L1].toString();
        const qint64 code = json_object["code"_L1].toInteger();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(error, QString::number(code));
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
    else if (result.http_status_code < 200 || result.http_status_code > 207) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}
