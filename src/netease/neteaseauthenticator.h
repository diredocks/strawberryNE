#ifndef NETEASEAUTHENTICATOR_H
#define NETEASEAUTHENTICATOR_H

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSslError>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"

#include "neteasebaserequest.h"

class QTimer;
class QNetworkReply;
class NetworkAccessManager;
class NeteaseBaseRequest;

class NeteaseAuthenticator: public NeteaseBaseRequest {
  Q_OBJECT

 public:
  explicit NeteaseAuthenticator(NeteaseService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~NeteaseAuthenticator() override;

  QList<QNetworkCookie> cookies() const { return cookies_; }
  bool authenticated() const { return !cookies_.empty(); }

  void Authenticate();
  void ClearSession();
  void LoadSession();

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  void StopCheckLoginTimer();
  QNetworkReply *CreateUnikeyRequest();
  QNetworkReply *CreateQrCheckRequest();
  QNetworkReply *CreateAnonimousRequest();

 Q_SIGNALS:
  void Error(const QString &error);
  void AuthenticationFinished(const bool success, const QString &error = QString());

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void UnikeyRequestFinished(QNetworkReply *reply);
  void QrCheckRequestFinished(QNetworkReply *reply);
  void AnonimousRequestFinished(QNetworkReply *reply);

 private:
  const SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_check_login_;

  QString unikey_;
  QList<QNetworkCookie> cookies_;

  QList<QNetworkReply*> replies_;
};

#endif
