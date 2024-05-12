#ifndef ABSTRACTNEURALNETWORK_H
#define ABSTRACTNEURALNETWORK_H

#include <QList>
#include <QString>
#include <QRectF>
#include <QImage>
#include <QVideoFrame>
#include <QVideoFrameFormat>

struct DetectionResult {
    QString label;
    double confidence = 0.0;
    QRectF objectRect;
    QColor color;
    QImage mask;
};

class AbstractNeuralNetwork
{
public:

    enum NeuralNetworksBackend {
        TensorFlow,
        OpenCv
    };

    AbstractNeuralNetwork() = default;

    /**
      Abstract methods to be reimplemented
      */
    virtual bool init(const QString& modelFile, const QString& labelsFile, const QString& configurationFile) = 0;
    virtual bool setInputImage(QVideoFrame& input, bool flip) = 0;
    virtual bool process() = 0;
    virtual NeuralNetworksBackend type() = 0;

    QList<DetectionResult> results() const { return m_results; }

    bool initialized() const { return m_initialized; }

protected:
    QList<DetectionResult> m_results;
    bool m_initialized = false;
};

Q_DECLARE_METATYPE(DetectionResult)
Q_DECLARE_METATYPE(QList<DetectionResult>)

#endif // ABSTRACTNEURALNETWORK_H
