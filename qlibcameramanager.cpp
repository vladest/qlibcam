#include "qlibcameramanager.h"
#include "qcontainerfwd.h"

#include <QDebug>

#include <libcamera/camera_manager.h>
#include <libcamera/formats.h>

#include <QEventLoop>
#include <QImage>
#include <QMutexLocker>
#include <QQmlEngine>

#include <private/qmemoryvideobuffer_p.h>
#include <private/qimagevideobuffer_p.h>

#include "qlibcamera.h"

Q_GLOBAL_STATIC(QLibCameraManager, singletonInstance);

const QMap<libcamera::PixelFormat, QVideoFrameFormat::PixelFormat> QLibCameraManager::nativeFormats{
    {libcamera::formats::ABGR8888, QVideoFrameFormat::Format_RGBX8888},
    {libcamera::formats::XBGR8888, QVideoFrameFormat::Format_RGBX8888},
    {libcamera::formats::ARGB8888, QVideoFrameFormat::Format_ARGB8888},
    {libcamera::formats::XRGB8888, QVideoFrameFormat::Format_XRGB8888},
    {libcamera::formats::RGB888, QVideoFrameFormat::Format_BGRX8888},
    {libcamera::formats::BGR888, QVideoFrameFormat::Format_RGBX8888},
    {libcamera::formats::MJPEG, QVideoFrameFormat::Format_Jpeg},
    {libcamera::formats::NV12, QVideoFrameFormat::Format_NV12},
    {libcamera::formats::UYVY, QVideoFrameFormat::Format_UYVY},
    {libcamera::formats::YUYV, QVideoFrameFormat::Format_YUYV},
    {libcamera::formats::YVYU, QVideoFrameFormat::Format_YV12},
    {libcamera::formats::YUV420, QVideoFrameFormat::Format_YUV420P},
    {libcamera::formats::YUV422, QVideoFrameFormat::Format_YUV422P},
};

libcamera::PixelFormat QLibCameraManager::toLibCameraFormat(QVideoFrameFormat::PixelFormat format)
{
    for (auto it = nativeFormats.begin(); it != nativeFormats.end(); ++it) {
        if (it.value() == format) {
            return it.key();
        }
    }
    return libcamera::PixelFormat();
}

QVideoFrameFormat::PixelFormat QLibCameraManager::toQtFormat(libcamera::PixelFormat format)
{
    for (auto it = nativeFormats.begin(); it != nativeFormats.end(); ++it) {
        if (it.key() == format) {
            return it.value();
        }
    }
    return QVideoFrameFormat::Format_Invalid;
}

QLibCameraManager::QLibCameraManager(QObject *parent) : QObject(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    qRegisterMetaType<QLibCamera*>("QLibCamera*");
    qRegisterMetaType<QList<QLibCamera*>>("QList<QLibCamera*>");
    qRegisterMetaType<QList<AbstractVideoFilter*>>("QList<AbstractVideoFilter*>");

    m_cameraManager = new libcamera::CameraManager();

    QObject::connect(this, &QLibCameraManager::cameraAdded, this, [this](const QString &cameraId) {
            auto cam = new QLibCamera(this, cameraId);
            m_cameras.insert(cameraId, cam);
            Q_EMIT camerasChanged();
        }, Qt::QueuedConnection);

    QObject::connect(this, &QLibCameraManager::cameraRemoved, this, [this](const QString &cameraId) {
            auto cam = m_cameras.take(cameraId);

            if (cam != nullptr) {
                delete cam;
            }
            Q_EMIT camerasChanged();

        }, Qt::QueuedConnection);

    initManager();
}

QLibCameraManager::~QLibCameraManager()
{
    finishManager();
}

QLibCameraManager *QLibCameraManager::instance()
{
    return singletonInstance();
}

QObject *QLibCameraManager::qmlInstance(QQmlEngine *, QJSEngine *)
{
    return QLibCameraManager::instance();
}

int QLibCameraManager::startCapture(const QString& cameraId, const StreamingRoles& roles, QVideoFrameFormat::PixelFormat prefferedFormat)
{
    auto cam = camera(cameraId);
    if (!cam) {
        qWarning() << "Camera not found:" << cameraId;
        return -1;
    }
    if (prefferedFormat != QVideoFrameFormat::Format_Invalid) {
        cam->setPrefferedFormat(prefferedFormat);
    }
    return cam->startCapture(roles);
}

void QLibCameraManager::stopCapture(const QString& cameraId)
{
    auto cam = camera(cameraId);
    if (!cam) {
        qWarning() << "Camera not found:" << cameraId;
        return;
    }
    cam->stopCapture();
}

/* -----------------------------------------------------------------------------
 * Camera hotplugging support
 */

void QLibCameraManager::addCamera(std::shared_ptr<Camera> camera)
{
    qInfo() << "Adding new camera:" << camera->id().c_str();
    Q_EMIT cameraAdded(QString(camera->id().c_str()));
}

void QLibCameraManager::removeCamera(std::shared_ptr<Camera> camera)
{
    qInfo() << "Removing camera:" << camera->id().c_str();
    Q_EMIT cameraRemoved(QString(camera->id().c_str()));
}

libcamera::CameraManager *QLibCameraManager::cameraManager() const
{
    return m_cameraManager;
}

void QLibCameraManager::initManager()
{
    int ret = m_cameraManager->start();
    if (ret) {
        qFatal() << "Failed to start camera manager:" << strerror(-ret);
        return;
    }
    for (const auto &camera : m_cameraManager->cameras()) {
        const auto id = QString::fromStdString(camera->id());
        qInfo() << "Camera:" << id;
        auto cam = new QLibCamera(this, id);
        m_cameras.insert(id, cam);
    }
    Q_EMIT camerasChanged();
    m_cameraManager->cameraAdded.connect(this, &QLibCameraManager::addCamera);
    m_cameraManager->cameraRemoved.connect(this, &QLibCameraManager::removeCamera);
}

void QLibCameraManager::finishManager()
{
    m_cameraManager->cameraAdded.disconnect(this);
    m_cameraManager->cameraRemoved.disconnect(this);

    for (auto cam : m_cameras.values()) {
        delete cam;
    }
    m_cameraManager->stop();
    delete m_cameraManager;
}

QList<QLibCamera *> QLibCameraManager::cameras() const
{
    return m_cameras.values();
}

QStringList QLibCameraManager::camerasModels() const
{
    QStringList names;
    for (auto cam : m_cameras.values()) {
        names.append(cam->model());
    }
    return names;
}

QLibCamera *QLibCameraManager::camera(const QString &cameraId) const
{
    return m_cameras.value(cameraId, nullptr);
}
