#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "netease/neteasecrypto.h"
#include "neteaseservice.h"
#include "neteasebaserequest.h"

using namespace Qt::Literals::StringLiterals;

NeteaseBaseRequest::NeteaseBaseRequest(NeteaseService *service, QObject *parent)
    : QObject(parent),
      service_(service),
      network_(new QNetworkAccessManager) {

  // network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

}

QNetworkReply *NeteaseBaseRequest::CreatePostRequest(const QString &ressource_name, const ParamList &params_provided) const {

  QUrl url(QLatin1String(NeteaseService::kWebApiUrl) + ressource_name);
  QNetworkRequest network_request(url);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded"_L1);

  QVariantMap params_map;
  for (const auto &param : params_provided) {
      params_map.insert(param.first, param.second);
  }

  const QVariantMap encrypted = NeteaseCrypto::weapi(QJsonDocument::fromVariant(params_map));

  QUrlQuery url_query;
  url_query.addQueryItem("params"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encrypted.value("params"_L1).toString())));
  url_query.addQueryItem("encSecKey"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encrypted.value("encSecKey"_L1).toString())));
  const QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(network_request, query);
  // QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseBaseRequest::HandleSSLErrors);
  // qLog(Debug) << "Netease: Sending request" << url;

  return reply;

}

// void NeteaseBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {
//
//   for (const QSslError &ssl_error : ssl_errors) {
//     Error(ssl_error.errorString());
//   }
//
// }

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
