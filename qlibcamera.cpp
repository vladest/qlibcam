#include "qlibcamera.h"
#include "qeventloop.h"
#include "qlibcameramanager.h"
#include "qvideoframe.h"
#include "qvideoframeformat.h"
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/logging.h>
#include <libcamera/property_ids.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#include <libcamera/pixel_format.h>
#include "abstractvideofilter.h"

#include <QDebug>
#include <QQmlListProperty>

using namespace libcamera;

QLibCamera::QLibCamera(QLibCameraManager* manager, const QString& cameraID, QObject *parent)
    : QThread{parent}, m_manager{manager}, m_cameraID{cameraID}
{
    auto camManager = m_manager->cameraManager();
    if (!camManager) {
        qWarning() << "Failed to get camera manager";
        return;
    }
    m_camera = camManager->get(m_cameraID.toStdString());
    if (!m_camera) {
        qWarning() << "Failed to get camera";
        return;
    }

    const libcamera::ControlList &camproperties = m_camera->properties();

    const auto &location = camproperties.get(libcamera::properties::Location);
    if (location) {
        switch (*location) {
        case libcamera::properties::CameraLocationFront:
            m_cameraLocation = "Internal front camera";
            break;
        case libcamera::properties::CameraLocationBack:
            m_cameraLocation = "Internal back camera";
            break;
        case libcamera::properties::CameraLocationExternal:
            m_cameraLocation = "External camera";
            break;
        }
    }

    m_cameraModel = QString(camproperties.get(libcamera::properties::Model).value_or("Unknown").data());
    qInfo() << "Location:" << m_cameraLocation << "Model:" << m_cameraModel;
    listControls();
    listProperties();
    retrieveViefinderInfo();

    QObject::connect(this, &QLibCamera::frameReady, this, &QLibCamera::processCapture, Qt::QueuedConnection);
    connect(&m_filtersWatcher, &QFutureWatcher<bool>::finished, this, &QLibCamera::handleFiltersFinished);

    start(QThread::TimeCriticalPriority);
}

QLibCamera::~QLibCamera()
{
    stopCapture();
    m_threadLoop->exit();
    wait(1000);
}

QVideoSink *QLibCamera::videoSink() const
{
    return m_videoSink;
}

void QLibCamera::setVideoSink(QVideoSink *newVideoSink)
{
    if (m_videoSink == newVideoSink)
        return;
    m_videoSink = newVideoSink;
    Q_EMIT videoSinkChanged();
}

void QLibCamera::listControls() const
{
    for (const auto &[id, info] : m_camera->controls()) {
        qDebug() << "Control:" << id->name() << ":" << info.toString();
    }
}

void QLibCamera::listProperties() const
{
    for (const auto &[key, value] : m_camera->properties()) {
        const ControlId *id = properties::properties.at(key);

        qDebug() << "Property:" << id->name() << "=" << value.toString();
    }
}

void QLibCamera::retrieveViefinderInfo()
{
    if (!m_camera) {
        qWarning() << "Camera not allocated";
        return;
    }

    if (m_camera->acquire()) {
        qWarning() << "Failed to acquire camera" << m_cameraID;
        return;
    }

    std::vector<libcamera::StreamRole> _roles;
    _roles.push_back(StreamRole::Viewfinder);

    /* Configure the camera. */
    const auto cameraConfig = m_camera->generateConfiguration(_roles);
    if (!cameraConfig) {
        qWarning() << "Failed to generate configuration from roles";
        return;
    }

    for (const StreamConfiguration &cfg : *cameraConfig) {
        const StreamFormats &formats = cfg.formats();
        for (PixelFormat pixelformat : formats.pixelformats()) {
            qDebug() << " * Pixelformat: "
                     << pixelformat.toString()
                     << formats.range(pixelformat).toString();

            QVector<QSize> sizes;
            for (const Size &size : formats.sizes(pixelformat)) {
                sizes.push_back(QSize(size.width, size.height));
                qDebug() << " - " << size.width << "x" << size.height;
            }
        }
        m_viewfinderInfo.append(formats);
    }

    Q_EMIT formatsChanged();
    m_camera->release();
}

QSize QLibCamera::frameSize() const
{
    return m_frameSize;
}

void QLibCamera::setFrameSize(const QSize &newFrameSize)
{
    if (m_frameSize == newFrameSize)
        return;
    m_frameSize = newFrameSize;
    Q_EMIT frameSizeChanged();
}

QVideoFrameFormat::PixelFormat QLibCamera::frameFormat() const
{
    return m_frameFormat;
}

void QLibCamera::setFrameFormat(QVideoFrameFormat::PixelFormat newFrameFormat)
{
    if (m_frameFormat == newFrameFormat)
        return;
    m_frameFormat = newFrameFormat;
    Q_EMIT frameFormatChanged();
}

bool QLibCamera::isCapturing() const
{
    return m_isCapturing;
}

int QLibCamera::startCapture(const QLibCameraManager::StreamingRoles& roles)
{
    int ret = -1;

    if (m_isCapturing) {
        qWarning() << "Already capturing";
        return -EBUSY;
    }

    const auto stdId = m_cameraID.toStdString();

    if (m_camera->acquire()) {
        qWarning() << "Failed to acquire camera" << m_cameraID;
        return -EINVAL;
    }

    std::vector<libcamera::StreamRole> _roles;

    // if (roles.testFlag(StreamingRole::RawStreamingRole)) {
    //     _roles.push_back(StreamRole::Raw);
    // }
    if (roles.testFlag(QLibCameraManager::StreamingRole::ViewFinderRole)) {
        _roles.push_back(StreamRole::Viewfinder);
    }

    //qDebug() << "Roles:" << roles << _roles.size() << (int)_roles[0] << (int)_roles[1];

    /* Configure the camera. */
    m_cameraConfig = m_camera->generateConfiguration(_roles);
    if (!m_cameraConfig) {
        qWarning() << "Failed to generate configuration from roles";
        return -EINVAL;
    }

    StreamConfiguration &vfConfig = m_cameraConfig->at(0);

    /* Use a format supported by the viewfinder if available. */
    std::vector<PixelFormat> formats = vfConfig.formats().pixelformats();
    if (formats.empty()) {
        qWarning() << "No formats available";
        return -EINVAL;
    }

    // choose format and resolution
    if (m_prefferedPixelFormat.isValid()) {
        vfConfig.pixelFormat = m_prefferedPixelFormat;
        m_frameFormat = QLibCameraManager::toQtFormat(m_prefferedPixelFormat);
        qDebug() << "Using preffered format" << m_prefferedPixelFormat.toString() << m_frameFormat;
    } else {
        vfConfig.pixelFormat = formats[0];
        m_frameFormat = QLibCameraManager::toQtFormat(formats[0]);
    }

    if (m_prefferedResolution.isValid()) {
        vfConfig.size = Size(m_prefferedResolution.width(), m_prefferedResolution.height());
        m_frameSize = m_prefferedResolution;
    } else {
        const auto sizes = vfConfig.formats().sizes(vfConfig.pixelFormat);
        vfConfig.size = sizes[sizes.size() - 1];
        m_frameSize = QSize(vfConfig.size.width, vfConfig.size.height);
    }
    /* Allow user to override configuration. */
    // if (StreamKeyValueParser::updateConfiguration(config_.get(),
    //                                               options_[OptStream])) {
    //     qWarning() << "Failed to update configuration";
    //     return -EINVAL;
    // }

    CameraConfiguration::Status validation = m_cameraConfig->validate();
    if (validation == CameraConfiguration::Invalid) {
        qWarning() << "Failed to create valid camera configuration";
        return -EINVAL;
    }

    if (validation == CameraConfiguration::Adjusted)
        qInfo() << "Stream configuration adjusted to "
                << vfConfig.toString().c_str();

    ret = m_camera->configure(m_cameraConfig.get());
    if (ret < 0) {
        qInfo() << "Failed to configure camera";
        return ret;
    }

    /* Store stream allocation. */
    m_viewFinderStream = m_cameraConfig->at(0).stream();
    if (m_cameraConfig->size() == 2)
        m_rawStream = m_cameraConfig->at(1).stream();
    else
        m_rawStream = nullptr;

    /*
     * Configure the viewfinder. If no color space is reported, default to
     * sYCC.
     */
    m_frameSize = QSize(vfConfig.size.width, vfConfig.size.height);
    auto pixelFormat = vfConfig.pixelFormat;
    auto stride = vfConfig.stride;
    qInfo() << "Viewfinder configuration: " << m_frameSize << pixelFormat.toString().c_str() << stride << vfConfig.toString().c_str();

    /* Allocate and map buffers. */
    m_allocator = new FrameBufferAllocator(m_camera);

    for (StreamConfiguration &config : *m_cameraConfig) {
        Stream *stream = config.stream();

        ret = m_allocator->allocate(stream);
        if (ret < 0) {
            qWarning() << "Failed to allocate capture buffers";
            cameraCleanup(false);
            return ret;
        }

        for (const std::unique_ptr<FrameBuffer> &buffer : m_allocator->buffers(stream)) {
            /* Map memory buffers and cache the mappings. */
            std::unique_ptr<Image> image =
                Image::fromFrameBuffer(buffer.get(), Image::MapMode::ReadOnly);
            assert(image != nullptr);
            m_mappedBuffers[buffer.get()] = std::move(image);

            /* Store buffers on the free list. */
            m_freeBuffers[stream].enqueue(buffer.get());
        }
    }

    /* Create requests and fill them with buffers from the viewfinder. */
    while (!m_freeBuffers[m_viewFinderStream].isEmpty()) {
        FrameBuffer *buffer = m_freeBuffers[m_viewFinderStream].dequeue();

        std::unique_ptr<Request> request = m_camera->createRequest();
        if (!request) {
            qWarning() << "Can't create request";
            ret = -ENOMEM;
            cameraCleanup(false);
            return ret;
        }

        ret = request->addBuffer(m_viewFinderStream, buffer);
        if (ret < 0) {
            qWarning() << "Can't set buffer for request";
            cameraCleanup(false);
            return ret;
        }

        m_requests.push_back(std::move(request));
    }

    /* Start the title timer and the camera. */
    m_lastBufferTime = 0;

    auto controls_ = m_camera->properties();

    int64_t frame_time = 1000000 / 30.0; // in us
    controls_.set(controls::FrameDurationLimits,
                  libcamera::Span<const int64_t, 2>({ frame_time, frame_time }));
    // controls_.set(controls::FrameDuration,frame_time);
    // controls_.set(controls::ExposureTime, 300);

    m_camera->requestCompleted.connect(this, &QLibCamera::requestComplete);

    ret = m_camera->start(/*&controls_*/);
    if (ret) {
        qInfo() << "Failed to start capture";
        cameraCleanup(false);
        return ret;
    }

    /* Queue all requests. */
    for (std::unique_ptr<Request> &request : m_requests) {
        ret = queueRequest(request.get());
        if (ret < 0) {
            qWarning() << "Can't queue request";
            cameraCleanup(true);
            return ret;
        }
    }

    m_isCapturing = true;
    Q_EMIT isCapturingChanged();

    return 0;
}

void QLibCamera::cameraCleanup(bool stopCapture)
{
    if (stopCapture && m_camera) {
        m_camera->requestCompleted.disconnect(this);
        m_camera->stop();
        m_camera->release();
    }

    m_requests.clear();
    m_mappedBuffers.clear();
    m_freeBuffers.clear();
    delete m_allocator;
    m_allocator = nullptr;
}

void QLibCamera::stopCapture()
{
    if (!m_isCapturing) {
        qWarning() << "Not capturing";
        return;
    }

    cameraCleanup(true);
    m_captureRaw = false;
    m_isCapturing = false;
    Q_EMIT isCapturingChanged();
    m_freeQueue.clear();
    m_cameraConfig.reset();
    m_doneQueue.clear();
}

void QLibCamera::requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled) {
        qInfo() << "Request cancelled";
        return;
    }

    /*
     * We're running in the libcamera thread context, expensive operations
     * are not allowed. Add the buffer to the done queue and post a
     * CaptureEvent for the application thread to handle.
     */
    {
        QMutexLocker locker(&m_mutex);
        m_doneQueue.enqueue(request);
    }
    //int64_t frame_time = 1000000 / 10.0; // in us
    //std::int64_t value_pair[2] = {frame_time, frame_time};
    //request->controls().set(libcamera::controls::FrameDurationLimits, libcamera::Span<const std::int64_t, 2>(value_pair));
    request->controls().set(libcamera::controls::AeEnable, false);
    request->controls().set(libcamera::controls::ExposureTime, 300);
    //qDebug() << "requestComplete" << (QThread::currentThread() == qApp->thread() ? "Main thread" : "Worker thread");
    processCapture();
}

void QLibCamera::processCapture()
{
    /*
     * Retrieve the next buffer from the done queue. The queue may be empty
     * if stopCapture() has been called while a CaptureEvent was posted but
     * not processed yet. Return immediately in that case.
     */
    //qDebug() << "processCapture" << (QThread::currentThread() == qApp->thread() ? "Main thread" : "Worker thread") << QDateTime::currentMSecsSinceEpoch();
    Request *request;
    {
        QMutexLocker locker(&m_mutex);
        if (m_doneQueue.isEmpty())
            return;

        request = m_doneQueue.dequeue();
    }

    /* Process buffers. */
    if (request->buffers().count(m_viewFinderStream))
        processViewfinder(request->buffers().at(m_viewFinderStream));

    if (request->buffers().count(m_rawStream))
        processRaw(request->buffers().at(m_rawStream), request->metadata());

    request->reuse();
    QMutexLocker locker(&m_mutex);
    m_freeQueue.enqueue(request);
}

void QLibCamera::handleFiltersFinished()
{
    Q_EMIT videoFiltersFinished();
}

void QLibCamera::processViewfinder(FrameBuffer *buffer)
{
    //framesCaptured_++;

    const FrameMetadata &metadata = buffer->metadata();

    double fps = metadata.timestamp - m_lastBufferTime;
    fps = m_lastBufferTime && fps ? 1000000000.0 / fps : 0.0;
    m_lastBufferTime = metadata.timestamp;

    QStringList bytesused;
    for (const FrameMetadata::Plane &plane : metadata.planes())
        bytesused << QString::number(plane.bytesused);

    // qDebug().noquote()
    //     << QString("seq: %1").arg(metadata.sequence, 6, 10, QLatin1Char('0'))
    //     << "bytesused: {" << bytesused.join(", ")
    //     << "} timestamp:" << metadata.timestamp
    //     << "fps:" << Qt::fixed << qSetRealNumberPrecision(2) << fps << "format:" << m_frameFormat;

    /* Render the frame on the viewfinder. */
    size_t size = buffer->metadata().planes()[0].bytesused;

    QVideoFrame frame = QVideoFrame(QVideoFrameFormat(m_frameSize, m_frameFormat));
    if (frame.map(QVideoFrame::WriteOnly)) {
        auto image  = m_mappedBuffers[buffer].get();
        memcpy(frame.bits(0), image->data(0).data(), size);
        frame.unmap();
        // Convert JPEG to RGBA8888
        // For video sink it should be done automatically, but for filters we want to have it in RGBA8888
        // so do it here
        if (m_frameFormat == QVideoFrameFormat::Format_Jpeg) {
            const auto img = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
            QVideoFrameFormat fmt(img.size(), QVideoFrameFormat::Format_RGBA8888);
            frame = QVideoFrame(fmt);
            frame.map(QVideoFrame::WriteOnly);
            memcpy(frame.bits(0), img.bits(), img.sizeInBytes());
            frame.unmap();
        }
        if (m_videoSink) {
            m_videoSink->setVideoFrame(frame);
        }
        if (m_videoFilters.isEmpty() == false && m_filtersWatcher.isRunning() == false) {
            QFuture<bool> future = QtConcurrent::run(&QLibCamera::filtersRunner, this, frame);
            m_filtersWatcher.setFuture(future);
        }
    }
    renderComplete(buffer);
}

void QLibCamera::processRaw(FrameBuffer *buffer,
                                   [[maybe_unused]] const ControlList &metadata)
{
#ifdef HAVE_DNG
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString filename = QFileDialog::getSaveFileName(this, "Save DNG", defaultPath,
                                                    "DNG Files (*.dng)");

    if (!filename.isEmpty()) {
        uint8_t *memory = mappedBuffers_[buffer]->data(0).data();
        DNGWriter::write(filename.toStdString().c_str(), camera_.get(),
                         rawStream_->configuration(), metadata, buffer,
                         memory);
    }
#endif

    {
        QMutexLocker locker(&m_mutex);
        m_freeBuffers[m_rawStream].enqueue(buffer);
    }
}

void QLibCamera::renderComplete(FrameBuffer *buffer)
{
    Request *request;
    {
        QMutexLocker locker(&m_mutex);
        if (m_freeQueue.isEmpty())
            return;

        request = m_freeQueue.dequeue();
    }

    request->addBuffer(m_viewFinderStream, buffer);

    if (m_captureRaw) {
        FrameBuffer *rawBuffer = nullptr;

        {
            QMutexLocker locker(&m_mutex);
            if (!m_freeBuffers[m_rawStream].isEmpty())
                rawBuffer = m_freeBuffers[m_rawStream].dequeue();
        }

        if (rawBuffer) {
            request->addBuffer(m_rawStream, rawBuffer);
            m_captureRaw = false;
        } else {
            qWarning() << "No free buffer available for RAW capture";
        }
    }
    queueRequest(request);
}

int QLibCamera::queueRequest(Request *request)
{
    return m_camera->queueRequest(request);
}

void QLibCamera::addFilter(AbstractVideoFilter *filter)
{
    if (!filter)
        return;
    if (m_videoFilters.contains(filter))
        return;
    m_videoFilters.append(filter);
    Q_EMIT videoFiltersChanged();
}

void QLibCamera::removeFilter(AbstractVideoFilter *filter)
{
    if (!filter)
        return;
    m_videoFilters.removeAll(filter);
    Q_EMIT videoFiltersChanged();
}

QList<AbstractVideoFilter*> QLibCamera::filters() const
{
    return m_videoFilters;
}

QString QLibCamera::cameraLocation() const
{
    return m_cameraLocation;
}

QString QLibCamera::model() const
{
    return m_cameraModel;
}

QString QLibCamera::id() const
{
    return m_cameraID;
}

QStringList QLibCamera::formats() const
{
    QStringList _formats;
    for (const auto &format : m_viewfinderInfo) {
        for (const auto &pixformat : format.pixelformats()) {
            _formats << QString::fromStdString(pixformat.toString());
        }
    }
    return _formats;
}

QList<QSize> QLibCamera::availableResolutions(const QString &format) const
{
    QList<QSize> _resolutions;
    const auto _format = PixelFormat::fromString(format.toStdString());
    if (!_format.isValid()) {
        qWarning() << "Invalid format" << format;
        return _resolutions;
    }
    for (const auto &format_ : m_viewfinderInfo) {
        for (const auto &pixformat : format_.pixelformats()) {
            if (QString::fromStdString(pixformat.toString()) != format) {
                continue;
            }
            for (const auto &size : format_.sizes(pixformat)) {
                _resolutions << QSize(size.width, size.height);
            }
        }
    }
    return _resolutions;
}

QStringList QLibCamera::availableResolutionsStrings(const QString &format) const
{
    const auto _resolutions = availableResolutions(format);
    QStringList _resolutionsStrings;
    for (const auto &resolution : _resolutions) {
        _resolutionsStrings << QString("%1x%2").arg(resolution.width()).arg(resolution.height());
    }
    return _resolutionsStrings;
}

void QLibCamera::setPrefferedFormat(QVideoFrameFormat::PixelFormat prefferedFormat)
{
    m_prefferedPixelFormat = QLibCameraManager::toLibCameraFormat(prefferedFormat);
}

void QLibCamera::setPrefferedFormat(const QString &format)
{
    m_prefferedPixelFormat = PixelFormat::fromString(format.toStdString());
    qWarning() << "Set preffered format" << m_prefferedPixelFormat.toString();
}

void QLibCamera::setPrefferedResolution(const QString &resolution)
{
    if (resolution.isEmpty() || resolution.split("x").size() != 2) {
        return;
    }
    const auto& resList = resolution.split("x");
    if (resList.size() != 2) {
        return;
    }
    m_prefferedResolution = QSize(resList.at(0).toInt(), resList.at(1).toInt());
}

void QLibCamera::run()
{
    m_threadLoop = new QEventLoop();
    m_threadLoop->exec();
    m_threadLoop->deleteLater();
    m_threadLoop = nullptr;

    qWarning() << "QLibCam thread finished";
}

bool QLibCamera::filtersRunner(const QVideoFrame &frame)
{
    QVideoFrame _frame = frame;
    for (AbstractVideoFilter *filter : m_videoFilters) {
        if (filter->isActive()) {
            _frame = filter->run(&_frame);
        }
    }
    return true;
}
