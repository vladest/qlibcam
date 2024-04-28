#pragma once

#include <QObject>
#include <QFlags>
#include <QVideoFrameFormat>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/controls.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include <QMutex>
#include <QQmlEngine>
#include <QQueue>
#include <QVideoSink>

class QEventLoop;
class QLibCamera;
class AbstractVideoFilter;

using namespace libcamera;

class QLibCameraManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QLibCamera*> cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QStringList camerasModels READ camerasModels NOTIFY camerasChanged)

public:

    enum StreamingRole {
        RawStreamingRole = 1,
        ImageStillRole = 2,
        VideoRecordingRole = 4,
        ViewFinderRole = 8,
    };
    Q_DECLARE_FLAGS(StreamingRoles, StreamingRole)
    Q_FLAG(StreamingRoles)

    QLibCameraManager(QObject* parent = nullptr);
    virtual ~QLibCameraManager();

    static QObject *qmlInstance(QQmlEngine *, QJSEngine *);
    static QLibCameraManager *instance();

    libcamera::CameraManager *cameraManager() const;

    QList<QLibCamera*> cameras() const;
    QStringList camerasModels() const;

    // add or remove for all registered cameras or camera with ginen id
    void addCameraFilter(AbstractVideoFilter *filter, const QString& cameraId = QString());
    void removeCameraFilter(AbstractVideoFilter *filter, const QString& cameraId = QString());

    static const QMap<libcamera::PixelFormat, QVideoFrameFormat::PixelFormat> nativeFormats;
    static libcamera::PixelFormat toLibCameraFormat(QVideoFrameFormat::PixelFormat format);
    static QVideoFrameFormat::PixelFormat toQtFormat(libcamera::PixelFormat format);

    void finishManager();

public Q_SLOTS:
    QLibCamera* camera(const QString &cameraId) const;
    int startCapture(const QString& cameraId, const QLibCameraManager::StreamingRoles &roles, QVideoFrameFormat::PixelFormat prefferedFormat = QVideoFrameFormat::Format_Invalid);
    void stopCapture(const QString& cameraId);

Q_SIGNALS:
    void cameraAdded(const QString &cameraId);
    void cameraRemoved(const QString &cameraId);
    void camerasChanged();

protected:
  void initManager();

private:
  void addCamera(std::shared_ptr<libcamera::Camera> camera);
  void removeCamera(std::shared_ptr<libcamera::Camera> camera);

private:
  libcamera::CameraManager *m_cameraManager = nullptr;
  QMap<QString, QLibCamera*> m_cameras;
  bool m_initialized = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QLibCameraManager::StreamingRoles)
