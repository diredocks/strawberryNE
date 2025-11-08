#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonParseError>

#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/settings.h"
#include "constants/neteasesettings.h"
#include "netease/neteasecrypto.h"
#include "netease/neteaseservice.h"
#include "neteaseauthenticator.h"

using namespace Qt::Literals::StringLiterals;

namespace {
enum class QrLoginStatus : qint64 {
  Timeout = 800,
  Waiting = 801,
  Scanning = 802,
  Confirmed = 803,
  Unknown = -1
};
const char kApiUrl[] = "https://music.163.com";
}

NeteaseAuthenticator::NeteaseAuthenticator(const SharedPtr<NetworkAccessManager> network, QObject *parent)
  : QObject(parent),
    network_(network),
    timer_check_login_(new QTimer(this)) {

  timer_check_login_->setInterval(1000);
  QObject::connect(timer_check_login_, &QTimer::timeout, this, &NeteaseAuthenticator::CreateQrCheckRequest);
  QObject::connect(this, &NeteaseAuthenticator::AuthenticationFinished, this, &NeteaseAuthenticator::StopCheckLoginTimer);

}

NeteaseAuthenticator::~NeteaseAuthenticator() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void NeteaseAuthenticator::LoadSession() {

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  // TODO: load session from settings, cookies for sure
  s.endGroup();

}

void NeteaseAuthenticator::ClearSession() {

  cookies_.clear();

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  // TODO: remove from settings etc...
  s.endGroup();

  StopCheckLoginTimer();

}

void NeteaseAuthenticator::StopCheckLoginTimer() {

  if (timer_check_login_->isActive()) {
    timer_check_login_->stop();
  }

}

void NeteaseAuthenticator::Authenticate() {

  CreateUnikeyRequest();

}

QNetworkReply *NeteaseAuthenticator::CreateUnikeyRequest() {

  QUrl unikey_url(QLatin1String(kApiUrl) + "/weapi/login/qrcode/unikey"_L1);

  const QVariantMap encryped = NeteaseCrypto::weapi(QJsonDocument(QJsonObject{{"type"_L1, "1"_L1}}));
  QUrlQuery url_query;
  url_query.addQueryItem("params"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encryped.value("params"_L1).toString())));
  url_query.addQueryItem("encSecKey"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encryped.value("encSecKey"_L1).toString())));

  const QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(QNetworkRequest(unikey_url), query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { UnikeyRequestFinished(reply); });
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);

  return reply;

}

QNetworkReply *NeteaseAuthenticator::CreateQrCheckRequest() {

  QUrl qrcheck_url(QLatin1String(kApiUrl) + "/weapi/login/qrcode/client/login"_L1);

  const QVariantMap encryped = NeteaseCrypto::weapi(QJsonDocument(QJsonObject{
        {"type"_L1, "1"_L1},
        {"key"_L1, unikey_},
        }));
  QUrlQuery url_query;
  url_query.addQueryItem("params"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encryped.value("params"_L1).toString())));
  url_query.addQueryItem("encSecKey"_L1, QString::fromLatin1(QUrl::toPercentEncoding(encryped.value("encSecKey"_L1).toString())));

  const QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(QNetworkRequest(qrcheck_url), query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { QrCheckRequestFinished(reply); });
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);

  return reply;

}

void NeteaseAuthenticator::UnikeyRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    const QString error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    Q_EMIT AuthenticationFinished(false, error_message);
  }

  const QByteArray data = reply->readAll();

  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    Q_EMIT AuthenticationFinished(false, QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_document.isEmpty()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has empty Json document."_s);
    return;
  }
  if (!json_document.isObject()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has Json document that is not an object."_s);
    return;
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has empty Json object."_s);
    return;
  }

  if (json_object.contains("msg"_L1) && json_object.contains("code"_L1)) {
    const QString error = json_object["msg"_L1].toString();
    const qint64 code = json_object["code"_L1].toInteger();
    qLog(Debug) << json_object;
    Q_EMIT AuthenticationFinished(false, QStringLiteral("%1 (%2)").arg(error, QString::number(code)));
    return;
  }

  if (!json_object.contains("unikey"_L1)) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server is missing unikey."_s);
    return;
  }

  unikey_ = json_object["unikey"_L1].toString();
  qLog(Debug) << unikey_;
  if (!timer_check_login_->isActive()) {
    timer_check_login_->start();
  }

}

void NeteaseAuthenticator::QrCheckRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    const QString error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    Q_EMIT AuthenticationFinished(false, error_message);
  }

  const QByteArray data = reply->readAll();

  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    Q_EMIT AuthenticationFinished(false, QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_document.isEmpty()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has empty Json document."_s);
    return;
  }
  if (!json_document.isObject()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has Json document that is not an object."_s);
    return;
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server has empty Json object."_s);
    return;
  }

  if (json_object.contains("msg"_L1) && json_object.contains("code"_L1)) {
    const QString error = json_object["msg"_L1].toString();
    const qint64 code = json_object["code"_L1].toInteger();
    Q_EMIT AuthenticationFinished(false, QStringLiteral("%1 (%2)").arg(error, QString::number(code)));
    return;
  }

  if (!json_object.contains("message"_L1) && !json_object.contains("code"_L1)) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server is missing message and code."_s);
    return;
  }

  const qint64 code = json_object["code"_L1].toInteger();
  const QrLoginStatus status = static_cast<QrLoginStatus>(code);

  qLog(Debug) << json_object;
  switch (status) {
    case QrLoginStatus::Timeout:
      Q_EMIT AuthenticationFinished(false, u"Authentication failed, QRCode timeout reached"_s);
      break;

    case QrLoginStatus::Waiting:
      break;

    case QrLoginStatus::Scanning:
      break;

    default:
      Q_EMIT AuthenticationFinished(false, QStringLiteral("Authentication failed, unknown status code %1").arg(code));
      break;
  }

}

void NeteaseAuthenticator::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Debug) << NeteaseSettings::kSettingsGroup << ssl_error.errorString();
  }

}
