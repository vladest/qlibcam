#pragma once

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtConcurrent>
#include <QFuture>
#include "libcamera/pixel_format.h"
#include "qcontainerfwd.h"
#include "qfuturewatcher.h"

#include <common/image.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/controls.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include "qlibcameramanager.h"
#include "qvideoframe.h"
#include "qvideoframeformat.h"

class QEventLoop;

class AbstractVideoFilter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit AbstractVideoFilter(QObject *parent = nullptr);
    virtual ~AbstractVideoFilter() = default;

    bool isActive() const { return m_active; }
    void setActive(bool v);

    virtual QVideoFrame run(QVideoFrame *input) = 0;

Q_SIGNALS:
    void activeChanged();

private:
    Q_DISABLE_COPY(AbstractVideoFilter)
    bool m_active = false;
};

class QLibCamera : public QThread
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(QString model READ model CONSTANT FINAL)
    Q_PROPERTY(QString id READ id CONSTANT FINAL)
    Q_PROPERTY(QString cameraLocation READ cameraLocation CONSTANT FINAL)
    Q_PROPERTY(QList<AbstractVideoFilter*> filters READ filters NOTIFY videoFiltersChanged)
    Q_PROPERTY(QStringList formats READ formats NOTIFY formatsChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY isCapturingChanged FINAL)
    Q_PROPERTY(QVideoFrameFormat::PixelFormat frameFormat READ frameFormat WRITE setFrameFormat
                   NOTIFY frameFormatChanged FINAL)
    Q_PROPERTY(QSize frameSize READ frameSize WRITE setFrameSize NOTIFY frameSizeChanged FINAL)

public:
    explicit QLibCamera(QLibCameraManager *manager, const QString &cameraID, QObject *parent = nullptr);
    virtual ~QLibCamera();

    QVideoSink *videoSink() const;
    void setVideoSink(QVideoSink *newVideoSink);

    int startCapture(const QLibCameraManager::StreamingRoles &roles);
    void stopCapture();

    QString model() const;
    QString id() const;
    QString cameraLocation() const;

    QList<AbstractVideoFilter*> filters() const;
    void addFilter(AbstractVideoFilter *filter);
    void removeFilter(AbstractVideoFilter *filter);

    QStringList formats() const;

    bool isCapturing() const;
    void setIsCapturing(bool newIsCapturing);

    QVideoFrameFormat::PixelFormat frameFormat() const;
    void setFrameFormat(QVideoFrameFormat::PixelFormat newFrameFormat);

    QSize frameSize() const;
    void setFrameSize(const QSize &newFrameSize);

    Q_INVOKABLE QList<QSize> availableResolutions(const QString& format) const;
    Q_INVOKABLE QStringList availableResolutionsStrings(const QString& format) const;

    Q_INVOKABLE void setPrefferedFormat(const QString& format);
    Q_INVOKABLE void setPrefferedFormat(QVideoFrameFormat::PixelFormat prefferedFormat);

    Q_INVOKABLE void setPrefferedResolution(const QString& resolution);

Q_SIGNALS:
    void videoSinkChanged();
    void frameReady();
    void cameraChanged();
    void videoFiltersChanged();
    void videoFiltersFinished();
    void formatsChanged();
    void isCapturingChanged();
    void frameFormatChanged();
    void frameSizeChanged();

protected:
    void run() override;

private Q_SLOTS:
    void processCapture();
    void handleFiltersFinished();
    bool filtersRunner(const QVideoFrame& frame);

private:
    void requestComplete(libcamera::Request *request);
    void cameraCleanup(bool stopCapture);
    void processViewfinder(libcamera::FrameBuffer *buffer);
    void processRaw(libcamera::FrameBuffer *buffer, const libcamera::ControlList &metadata);

    void listControls() const;
    void listProperties() const;

    void renderComplete(libcamera::FrameBuffer *buffer);
    int queueRequest(libcamera::Request *request);

    void retrieveViefinderInfo();

private:
    QVideoFrame m_videoFrame;
    std::shared_ptr<libcamera::Camera> m_camera;
    libcamera::FrameBufferAllocator *m_allocator = nullptr;

    std::unique_ptr<libcamera::CameraConfiguration> m_cameraConfig;
    std::map<libcamera::FrameBuffer *, std::unique_ptr<Image>> m_mappedBuffers;

    /* Capture state, buffers queue and statistics */
    bool m_isCapturing = false;
    bool m_captureRaw = false;
    QMutex m_mutex;
    libcamera::Stream *m_viewFinderStream = nullptr;
    libcamera::Stream *m_rawStream = nullptr;
    std::map<const libcamera::Stream *, QQueue<libcamera::FrameBuffer *>> m_freeBuffers;

    QQueue<libcamera::Request *> m_doneQueue;
    QQueue<libcamera::Request *> m_freeQueue;
    std::vector<std::unique_ptr<libcamera::Request>> m_requests;

    QEventLoop *m_threadLoop = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QVideoFrameFormat::PixelFormat m_frameFormat = QVideoFrameFormat::Format_YUYV;
    QSize m_frameSize;
    libcamera::PixelFormat m_prefferedPixelFormat;
    QSize m_prefferedResolution;
    uint64_t m_lastBufferTime = 0;
    QLibCameraManager *m_manager = nullptr;
    QString m_cameraID;
    QString m_cameraModel;
    QString m_cameraLocation { "Unknown"};
    QList<AbstractVideoFilter *> m_videoFilters;
    QFutureWatcher<bool> m_filtersWatcher;
    QList<libcamera::StreamFormats> m_viewfinderInfo;
    QStringList m_formats;
};
