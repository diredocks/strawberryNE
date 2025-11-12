#include <memory>

#include <QUrl>
#include <QTimer>
#include <QString>
#include <QByteArray>

#include "constants/neteasesettings.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/urlhandlers.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "neteaseservice.h"
#include "neteasestreamurlrequest.h"
#include "neteasebaserequest.h"
#include "neteaserequest.h"
#include "neteaseauthenticator.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

const Song::Source NeteaseService::kSource = Song::Source::Netease;
const char NeteaseService::kApiUrl[] = "https://interface.music.163.com";
const char NeteaseService::kWebApiUrl[] = "https://music.163.com";

namespace {

constexpr char kArtistsSongsTable[] = "spotify_artists_songs";
constexpr char kAlbumsSongsTable[] = "spotify_albums_songs";
constexpr char kSongsTable[] = "spotify_songs";

}

NeteaseService::NeteaseService(const SharedPtr<TaskManager> task_manager,
                               const SharedPtr<Database> database,
                               const SharedPtr<NetworkAccessManager> network,
                               const SharedPtr<UrlHandlers> url_handlers,
                               const SharedPtr<AlbumCoverLoader> albumcover_loader,
                               QObject *parent)
    : StreamingService(Song::Source::Netease, u"Netease"_s, u"netease"_s, QLatin1String(NeteaseSettings::kSettingsGroup), parent),
      network_(network),
      url_handler_(new NeteaseUrlHandler(task_manager, this)),
      auth_(new NeteaseAuthenticator(this, network, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      enabled_(false),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(SearchType::Artists),
      search_id_(0) {

  url_handlers->Register(url_handler_);

  QObject::connect(auth_.get(), &NeteaseAuthenticator::AuthenticationFinished, this, &NeteaseService::AuthFinished);

  // Backends
  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &NeteaseService::StartSearch);

  NeteaseService::ReloadSettings();
  auth_->LoadSession();
  Authenticate();

}

NeteaseService::~NeteaseService() {

  artists_collection_backend_->deleteLater();
  albums_collection_backend_->deleteLater();
  songs_collection_backend_->deleteLater();

}

void NeteaseService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &NeteaseService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &NeteaseService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &NeteaseService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();
  // Q_EMIT ExitFinished(); // Trigger this to indicate exit finished

}

void NeteaseService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

bool NeteaseService::authenticated() const {

  return auth_->authenticated();

}

QList<QNetworkCookie> NeteaseService::cookies() const {

  return auth_->cookies();

}

void NeteaseService::ReloadSettings() {

  Settings s;
  s.beginGroup(NeteaseSettings::kSettingsGroup);

  enabled_ = s.value(NeteaseSettings::kEnabled, false).toBool();

  quint64 search_delay = std::max(s.value(NeteaseSettings::kSearchDelay, 1500).toULongLong(), 500ULL);
  artistssearchlimit_ = s.value(NeteaseSettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(NeteaseSettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(NeteaseSettings::kSongsSearchLimit, 10).toInt();
  fetchalbums_ = s.value(NeteaseSettings::kFetchAlbums, false).toBool();
  download_album_covers_ = s.value(NeteaseSettings::kDownloadAlbumCovers, true).toBool();
  // remove_remastered_ = s.value(NeteaseSettings::kRemoveRemastered, true).toBool();

  s.endGroup();

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

  auth_->LoadSession(); // Reload cookie in case user changed it

};

void NeteaseService::Authenticate() {

  auth_->Authenticate();

}

void NeteaseService::ClearSession() {

  auth_->ClearSession();

}

void NeteaseService::AuthFinished(const bool success, const QString &error) {

  qLog(Debug) << success << error;

  if (success) {
    Q_EMIT LoginSuccess();
    // Q_EMIT UpdateSpotifyAccessToken(oauth_->access_token());
  }
  else {
    Q_EMIT LoginFailure(error);
  }

  Q_EMIT LoginFinished(success);

}

void NeteaseService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new NeteaseRequest(this, url_handler_, network_, NeteaseBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &NeteaseRequest::Results, this, &NeteaseService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::ArtistsProgressSetMaximumReceived);
  QObject::connect(&*artists_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}

void NeteaseService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new NeteaseRequest(this, url_handler_, network_, NeteaseBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &NeteaseRequest::Results, this, &NeteaseService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::AlbumsProgressSetMaximumReceived);
  QObject::connect(&*albums_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void NeteaseService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new NeteaseRequest(this, url_handler_, network_, NeteaseBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &NeteaseRequest::Results, this, &NeteaseService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::SongsProgressSetMaximumReceived);
  QObject::connect(&*songs_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void NeteaseService::ResetArtistsRequest() {

  albums_request_.reset();

}

void NeteaseService::ResetAlbumsRequest() {

  albums_request_.reset();

}
void NeteaseService::ResetSongsRequest() {

  songs_request_.reset();

}

int NeteaseService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_text_ = text;
  pending_search_type_ = type;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    return pending_search_id_;
  }
  timer_search_delay_->start();

  return pending_search_id_;

}

void NeteaseService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Netease."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void NeteaseService::CancelSearch() {}

void NeteaseService::SendSearch() {

  NeteaseBaseRequest::Type type = NeteaseBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      type = NeteaseBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      type = NeteaseBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      type = NeteaseBaseRequest::Type::SearchSongs;
      break;
    default:
      // Error("Invalid search type.");
      return;
  }

  search_request_.reset(new NeteaseRequest(this, url_handler_, network_, type, this));
  QObject::connect(&*search_request_, &NeteaseRequest::Results, this, &NeteaseService::SearchResultsReceived);
  QObject::connect(&*search_request_, &NeteaseRequest::UpdateStatus, this, &NeteaseService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &NeteaseRequest::ProgressSetMaximum, this, &NeteaseService::SearchProgressSetMaximum);
  QObject::connect(&*search_request_, &NeteaseRequest::UpdateProgress, this, &NeteaseService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void NeteaseService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

void NeteaseService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {
  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();
}

void NeteaseService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {
  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();
}

void NeteaseService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void NeteaseService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void NeteaseService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void NeteaseService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void NeteaseService::ArtistsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT ArtistsProgressSetMaximum(max);
}

void NeteaseService::AlbumsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT AlbumsProgressSetMaximum(max);
}

void NeteaseService::SongsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT SongsProgressSetMaximum(max);
}

void NeteaseService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void NeteaseService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void NeteaseService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

uint NeteaseService::GetStreamURL(const QUrl &url, QString &error) {

  if (!authenticated()) {
    error = tr("Not authenticated with Netease.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  NeteaseStreamURLRequestPtr stream_url_request = NeteaseStreamURLRequestPtr(new NeteaseStreamURLRequest(this, network_, url, id));
  stream_url_requests_.insert(id, stream_url_request);
  QObject::connect(&*stream_url_request, &NeteaseStreamURLRequest::StreamURLFailure, this, &NeteaseService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_request, &NeteaseStreamURLRequest::StreamURLSuccess, this, &NeteaseService::HandleStreamURLSuccess);
  stream_url_request->Process();

  return id;

}

void NeteaseService::HandleStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLFailure(id, media_url, error);

}

void NeteaseService::HandleStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLSuccess(id, media_url, stream_url, filetype, samplerate, bit_depth, duration);

}
