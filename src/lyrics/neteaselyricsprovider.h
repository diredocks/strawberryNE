#ifndef NETEASELYRICSPROVIDER_H
#define NETEASELYRICSPROVIDER_H

#include <QList>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;

class NeteaseLyricsProcvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit NeteaseLyricsProcvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 private:
  struct LyricsSearchContext {
    explicit LyricsSearchContext() : id(-1) {}
    int id;
    LyricsSearchRequest request;
    QMap<qint64, LyricsSearchResult> active_tracks_;
    LyricsSearchResults results;
  };

  using LyricsSearchContextPtr = SharedPtr<LyricsSearchContext>;

  bool SendSearchRequest(LyricsSearchContextPtr search);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  bool SendLyricsRequest(LyricsSearchContextPtr search, const qint64 track_id);
  void EndSearch(LyricsSearchContextPtr search, const qint64 track_id);

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, NeteaseLyricsProcvider::LyricsSearchContextPtr search);
  void HandleLyricsReply(QNetworkReply *reply, NeteaseLyricsProcvider::LyricsSearchContextPtr search, const qint64 track_id);

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

 private:
  QList<LyricsSearchContextPtr> requests_search_;
};

#endif // !NETEASELYRICSPROVIDER_H
