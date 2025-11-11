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
}

NeteaseAuthenticator::NeteaseAuthenticator(const NeteaseService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
  : NeteaseBaseRequest(service, network, parent),
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
  QByteArray cookieData = s.value(NeteaseSettings::kCookies).toByteArray();
  s.endGroup();

  QList<QNetworkCookie> loadedCookies;
  for (const QByteArray &line : cookieData.split(';')) {
    QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) continue;
    const QList<QNetworkCookie> parsed = QNetworkCookie::parseCookies(trimmed);
    loadedCookies.append(parsed);
  }

  cookies_ = loadedCookies;

}

void NeteaseAuthenticator::ClearSession() {

  cookies_.clear();

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  s.remove(NeteaseSettings::kCookies);
  s.endGroup();

  StopCheckLoginTimer();

}

void NeteaseAuthenticator::StopCheckLoginTimer() {

  if (timer_check_login_->isActive()) {
    timer_check_login_->stop();
  }

}

void NeteaseAuthenticator::Authenticate() {

  if (authenticated()) return;

  // CreateUnikeyRequest();
  CreateAnonimousRequest();

}

QNetworkReply *NeteaseAuthenticator::CreateUnikeyRequest() {

  const ParamList params = ParamList() << Param("type"_L1, "1"_L1);

  QNetworkReply *reply = CreatePostRequest("/weapi/login/qrcode/unikey"_L1, params); // network_->post(QNetworkRequest(unikey_url), query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { UnikeyRequestFinished(reply); });
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);

  return reply;

}

QNetworkReply *NeteaseAuthenticator::CreateQrCheckRequest() {

  const ParamList params = ParamList() << Param("type"_L1, "1"_L1)
                                       << Param("key"_L1, unikey_);

  QNetworkReply *reply = CreatePostRequest("/weapi/login/qrcode/client/login"_L1, params);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { QrCheckRequestFinished(reply); });
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);

  return reply;

}

QNetworkReply *NeteaseAuthenticator::CreateAnonimousRequest() {

  const QByteArray ID_XOR_KEY_1 = QByteArrayLiteral("3go8&$833h0k(2)2");

  auto cloudmusic_dll_encode_id = [ID_XOR_KEY_1](const QString &some_id) -> QByteArray {
    QByteArray input = some_id.toUtf8();
    QByteArray xored;
    xored.resize(input.size());

    for (int i = 0; i < input.size(); ++i) {
      xored[i] = input[i] ^ ID_XOR_KEY_1[i % ID_XOR_KEY_1.size()];
    }

    QByteArray md5 = QCryptographicHash::hash(xored, QCryptographicHash::Md5);
    return md5.toBase64();
  };

  const QString deviceId = QStringLiteral("NMUSIC");

  const QByteArray encodedId = QString(deviceId + QStringLiteral(" ")
    + QString::fromUtf8(cloudmusic_dll_encode_id(deviceId))
  ).toUtf8().toBase64();

  QUrl url(QLatin1String(NeteaseService::kApiUrl) + "/api/register/anonimous"_L1);
  url.setQuery(QUrlQuery{{"username"_L1, QString::fromUtf8(encodedId)}});

  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AnonimousRequestFinished(reply); });
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);

  return reply;

}

void NeteaseAuthenticator::UnikeyRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult result = ParseJsonObject(reply);
  if (result.error_code != ErrorCode::Success) {
    Q_EMIT AuthenticationFinished(false, result.error_message);
    return;
  }

  const QJsonObject &json_object = result.json_object;
  if (!json_object.contains("unikey"_L1)) {
    Q_EMIT AuthenticationFinished(false, u"Authentication reply from server is missing unikey."_s);
    return;
  }

  unikey_ = json_object["unikey"_L1].toString();
  qLog(Debug) << "Received unikey:" << unikey_;

  if (!timer_check_login_->isActive()) {
    timer_check_login_->start();
  }

}

void NeteaseAuthenticator::QrCheckRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult result = ParseJsonObject(reply);
  if (result.error_code != ErrorCode::Success) {
    Q_EMIT AuthenticationFinished(false, result.error_message);
    return;
  }

  const QJsonObject &json_object = result.json_object;
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

void NeteaseAuthenticator::AnonimousRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult result = ParseJsonObject(reply);
  if (result.error_code != ErrorCode::Success) {
    Q_EMIT AuthenticationFinished(false, result.error_message);
    return;
  }

  const QJsonObject &json_object = result.json_object;
  qLog(Debug) << json_object;

  QVariant setCookieHeader = reply->header(QNetworkRequest::SetCookieHeader);
  QList<QNetworkCookie> cookies = setCookieHeader.value<QList<QNetworkCookie>>();

  QByteArray cookieData;
  bool first = true;
  for (const QNetworkCookie &cookie : cookies) {
      if (!first)
          cookieData.append("; ");
      cookieData.append(cookie.name());
      cookieData.append('=');
      cookieData.append(cookie.value());
      first = false;
  }

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  s.setValue(NeteaseSettings::kCookies, QString::fromUtf8(cookieData));
  s.endGroup();

  cookies_ = cookies;

}

void NeteaseAuthenticator::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Debug) << NeteaseSettings::kSettingsGroup << ssl_error.errorString();
  }

}
