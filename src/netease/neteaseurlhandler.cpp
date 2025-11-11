#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"
#include "core/song.h"
#include "netease/neteaseservice.h"
#include "neteaseurlhandler.h"

NeteaseUrlHandler::NeteaseUrlHandler(const SharedPtr<TaskManager> task_manager, NeteaseService *service)
    : UrlHandler(service),
      task_manager_(task_manager),
      service_(service) {

  QObject::connect(service, &NeteaseService::StreamURLFailure, this, &NeteaseUrlHandler::GetStreamURLFailure);
  QObject::connect(service, &NeteaseService::StreamURLSuccess, this, &NeteaseUrlHandler::GetStreamURLSuccess);

}

QString NeteaseUrlHandler::scheme() const {

  return service_->url_scheme();

}

UrlHandler::LoadResult NeteaseUrlHandler::StartLoading(const QUrl &url) {

  Request request;
  request.task_id = task_manager_->StartTask(QStringLiteral("Loading %1 stream...").arg(url.scheme()));
  QString error;
  request.id = service_->GetStreamURL(url, error);
  if (request.id == 0) {
    CancelTask(request.task_id);
    return LoadResult(url, LoadResult::Type::Error, error);
  }

  requests_.insert(request.id, request);

  LoadResult ret(url);
  ret.type_ = LoadResult::Type::WillLoadAsynchronously;

  return ret;

}

void NeteaseUrlHandler::GetStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!requests_.contains(id)) return;
  Request req = requests_.take(id);
  CancelTask(req.task_id);

  Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::Error, error));

}

void NeteaseUrlHandler::GetStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!requests_.contains(id)) return;
  Request req = requests_.take(id);
  CancelTask(req.task_id);

  Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::TrackAvailable, stream_url, filetype, samplerate, bit_depth, duration));

}

void NeteaseUrlHandler::CancelTask(const int task_id) {
  task_manager_->SetTaskFinished(task_id);
}
