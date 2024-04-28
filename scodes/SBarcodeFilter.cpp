#include "SBarcodeFilter.h"

#include <QImage>
#include <QtMultimedia/qvideoframe.h>

#include "SBarcodeDecoder.h"
#include "private/debug.h"

void processImage(SBarcodeDecoder *decoder, const QImage &image, ZXing::BarcodeFormats formats)
{
    decoder->process(image, formats);
}

SBarcodeFilter::SBarcodeFilter(QObject *parent)
    : AbstractVideoFilter{parent},
    m_decoder{new SBarcodeDecoder}
{
    connect(m_decoder, &SBarcodeDecoder::capturedChanged, this, &SBarcodeFilter::setCaptured);

    connect(this, &AbstractVideoFilter::activeChanged, this, [this](){
        if (this->isActive()) {
            this->clean();
        }
    });
}

QVideoFrame SBarcodeFilter::run(QVideoFrame* input)
{
    const QImage croppedCapturedImage = getDecoder()->videoFrameToImage(*input, captureRect().toRect());
    m_decoder->process(croppedCapturedImage, SCodes::toZXingFormat(format()));

    return *input;
}


QString SBarcodeFilter::captured() const
{
    return m_captured;
}

void SBarcodeFilter::setCaptured(const QString &captured)
{
    if (captured == m_captured) {
        return;
    }

    m_captured = captured;

    Q_EMIT capturedChanged(m_captured);
}

void SBarcodeFilter::clean()
{
    m_captured = "";

    m_decoder->clean();
}

QRectF SBarcodeFilter::captureRect() const
{
    return m_captureRect;
}

void SBarcodeFilter::setCaptureRect(const QRectF &captureRect)
{
    if (captureRect == m_captureRect) {
        return;
    }

    m_captureRect = captureRect;

    Q_EMIT captureRectChanged(m_captureRect);
}

SBarcodeDecoder *SBarcodeFilter::getDecoder() const
{
    return m_decoder;
}

// QFuture<void> SBarcodeFilter::getImageFuture() const
// {
//     return _imageFuture;
// }

const SCodes::SBarcodeFormats &SBarcodeFilter::format() const
{
    return m_format;
}

void SBarcodeFilter::setFormat(const SCodes::SBarcodeFormats &format)
{
    sDebug() << "set format " << format << ", old format " << m_format;

    if (m_format != format) {
        m_format = format;
        Q_EMIT formatChanged(m_format);
    }
}
