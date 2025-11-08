#ifndef NETEASESERVICE_H
#define NETEASESERVICE_H

#include "config.h"

#include <QList>
#include <QString>
#include <QScopedPointer>
#include <qcontainerfwd.h>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "collection/collectionmodel.h"

class QTimer;

class TaskManager;
class Database;
class NetworkAccessManager;
class AlbumCoverLoader;
class NeteaseBaseRequest;
class NeteaseAuthenticator;

// using SpotifyRequestPtr = QScopedPointer<SpotifyRequest, QScopedPointerDeleteLater>;

class NeteaseService : public StreamingService {
  Q_OBJECT

 public:
  explicit NeteaseService(const SharedPtr<TaskManager> task_manager,
                          const SharedPtr<Database> database,
                          const SharedPtr<NetworkAccessManager> network,
                          const SharedPtr<AlbumCoverLoader> albumcover_Loader,
                          QObject *parent = nullptr);

  ~NeteaseService() override;

  static const Song::Source kSource;
  static const char kApiUrl[];
  static const char kWebApiUrl[];

  void Exit() override;
  // TODO: void ReloadSettings() override;

  // TODO: int Search(const QString &text, const SearchType type) override;
  // TODO: void CancelSearch() override;

  // int artistssearchlimit() const { return artistssearchlimit_; }
  // int albumssearchlimit() const { return albumssearchlimit_; }
  // int songssearchlimit() const { return songssearchlimit_; }
  // bool fetchalbums() const { return fetchalbums_; }
  // bool download_album_covers() const { return download_album_covers_; }
  // bool remove_remastered() const { return remove_remastered_; }

  bool authenticated() const override;
  // TODO: QString cookie() const;

  // SharedPtr<CollectionBackend> artists_collection_backend() override { return artists_collection_backend_; }
  // SharedPtr<CollectionBackend> albums_collection_backend() override { return albums_collection_backend_; }
  // SharedPtr<CollectionBackend> songs_collection_backend() override { return songs_collection_backend_; }
  //
  // CollectionModel *artists_collection_model() override { return artists_collection_model_; }
  // CollectionModel *albums_collection_model() override { return albums_collection_model_; }
  // CollectionModel *songs_collection_model() override NeteaseAuthenticator { return songs_collection_model_; }
  //
  // CollectionFilter *artists_collection_filter_model() override { return artists_collection_model_->filter(); }
  // CollectionFilter *albums_collection_filter_model() override { return albums_collection_model_->filter(); }
  // CollectionFilter *songs_collection_filter_model() override { return songs_collection_model_->filter(); }
 
 // Q_SIGNALS:
 //  void UpdateSpotifyAccessToken(const QString &access_token);

 public Q_SLOTS:
  void Authenticate();
  void ClearSession();
  // void GetArtists() override;
  // void GetAlbums() override;
  // void GetSongs() override;
  // void ResetArtistsRequest() override;
  // void ResetAlbumsRequest() override;
  // void ResetSongsRequest() override;

 private Q_SLOTS:
  // TODO: void ExitReceived();
  void AuthFinished(const bool success, const QString &error = QString());
  // TODO: void StartSearch();
  //
  // void ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error);
  // void AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error);
  // void SongsResultsReceived(const int id, const SongMap &songs, const QString &error);
  // void SearchResultsReceived(const int id, const SongMap &songs, const QString &error);
  // void ArtistsUpdateStatusReceived(const int id, const QString &text);
  // void AlbumsUpdateStatusReceived(const int id, const QString &text);
  // void SongsUpdateStatusReceived(const int id, const QString &text);
  // void ArtistsProgressSetMaximumReceived(const int id, const int max);
  // void AlbumsProgressSetMaximumReceived(const int id, const int max);
  // void SongsProgressSetMaximumReceived(const int id, const int max);
  // void ArtistsUpdateProgressReceived(const int id, const int progress);
  // void AlbumsUpdateProgressReceived(const int id, const int progress);
  // void SongsUpdateProgressReceived(const int id, const int progress);

 private:
  // TODO: void SendSearch();

 private:
  const SharedPtr<NetworkAccessManager> network_;

  // OAuthenticator *oauth_;
  SharedPtr<NeteaseAuthenticator> netease_auth_;

  // SharedPtr<CollectionBackend> artists_collection_backend_;
  // SharedPtr<CollectionBackend> albums_collection_backend_;
  // SharedPtr<CollectionBackend> songs_collection_backend_;
  //
  // CollectionModel *artists_collection_model_;
  // CollectionModel *albums_collection_model_;
  // CollectionModel *songs_collection_model_;

  // QTimer *timer_search_delay_;

  // SpotifyRequestPtr artists_request_;
  // SpotifyRequestPtr albums_request_;
  // SpotifyRequestPtr songs_request_;
  // SpotifyRequestPtr search_request_;
  // SpotifyFavoriteRequest *favorite_request_;

  bool enabled_;
  // int artistssearchlimit_;
  // int albumssearchlimit_;
  // int songssearchlimit_;
  // bool fetchalbums_;
  // bool download_album_covers_;
  // bool remove_remastered_;

  // int pending_search_id_;
  // int next_pending_search_id_;
  // QString pending_search_text_;
  // SearchType pending_search_type_;
  //
  // int search_id_;
  // QString search_text_;
  //
  // QList<QObject*> wait_for_exit_;
};

using NeteaseServicePtr = SharedPtr<NeteaseService>;

#endif // NETEASESERVICE_H
