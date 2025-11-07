#ifndef NETEASECOVERPROVIDER_H
#define NETEASECOVERPROVIDER_H

#include <QVariant>
#include <QString>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"

class QNetworkReply;
class NetworkAccessManager;

class NeteaseCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit NeteaseCoverProvider(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  // virtual bool authenticated() const override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;
  // void ClearSession() override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, const int id);

 private:
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
};

#endif
