#ifndef NETEASEURLHANDLER_H
#define NETEASEURLHANDLER_H

#include <QMap>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/urlhandler.h"
#include "core/song.h"

class TaskManager;
class NeteaseService;

class NeteaseUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit NeteaseUrlHandler(const SharedPtr<TaskManager> task_manager, NeteaseService *service);

  QString scheme() const override;
  LoadResult StartLoading(const QUrl &url) override;

 private:
  void CancelTask(const int task_id);

 private Q_SLOTS:
  void GetStreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void GetStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);

 private:
  struct Request {
    Request() : id(0), task_id(-1) {}
    uint id;
    int task_id;
  };
  const SharedPtr<TaskManager> task_manager_;
  NeteaseService *service_;
  QMap<uint, Request> requests_;
};

#endif  // NETEASEURLHANDLER_H
