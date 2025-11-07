#include "constants/neteasesettings.h"
#include "core/logging.h"
#include "core/settings.h"

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
#include <qjsondocument.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qobject.h>
#include <qstringliteral.h>
#include <qurl.h>
#include <qurlquery.h>

#include "neteaseauthenticator.h"
#include "core/networkaccessmanager.h"

using namespace Qt::Literals::StringLiterals;

NeteaseAuthenticator::NeteaseAuthenticator(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network),
      timer_check_login_(new QTimer(this)) {

  // TODO: set timer_check_login_ handler and timeout here

}

NeteaseAuthenticator::~NeteaseAuthenticator() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}


QList<QNetworkCookie> NeteaseAuthenticator::cookies() const {

  return cookies_;

}

void NeteaseAuthenticator::LoadSession() {

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  // TODO: load from file to cookies
  s.endGroup();

}

void NeteaseAuthenticator::ClearSession() {

  cookies_.clear();

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);
  s.remove(NeteaseSettings::kCookie);
  s.endGroup();

  if(timer_check_login_->isActive()) {
    timer_check_login_->stop();
  }

}

void NeteaseAuthenticator::StartCheckLoginTimer() {

  if (!timer_check_login_->isActive()) {
    timer_check_login_->start();
  }

}

void NeteaseAuthenticator::Authenticate() {

  // From QCloudMusicApi/module.cpp
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

  const QString deviceId = u"NMUSIC"_s;
  const QByteArray encodedId = QString(deviceId + u" "_s +
      QString::fromUtf8(cloudmusic_dll_encode_id(deviceId))
  ).toUtf8().toBase64();

  const auto username = QString::fromUtf8(encodedId);
  //

  QUrlQuery url_query;
  url_query.addQueryItem(u"username"_s, username);
  QUrl url(u"https://interface.music.163.com/api/register/anonimous"_s);
  url.setQuery(url_query);

  QNetworkRequest network_request(url);
  QNetworkReply* reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &NeteaseAuthenticator::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AnonimousRegisterFinished(reply); });

}

void NeteaseAuthenticator::AnonimousRegisterFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    const QString error_message = u"%1 (%2)"_s.arg(reply->errorString()).arg(reply->error());
    Q_EMIT AuthenticationFinished(false, error_message);
  }

  // TODO: check API error
  // QByteArray response_data = reply->readAll();
  // qDebug() << "Response:" << response_data;
  cookies_ = network_->cookieJar()->cookiesForUrl(reply->url());

  qLog(Debug) << NeteaseSettings::kSettingsGroup << "Authentication was successful";

  Q_EMIT AuthenticationFinished(true);

}

void NeteaseAuthenticator::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Debug) << NeteaseSettings::kSettingsGroup << ssl_error.errorString();
  }

}
