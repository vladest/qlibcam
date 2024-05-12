#include "tensorflowfilter.h"

TensorFlowFilter::TensorFlowFilter(QObject *parent)
    : AbstractVideoFilter{parent}
{
    const auto modelName = qgetenv("TF2_MODEL");
    const auto modelLabelsName = qgetenv("TF2_MODEL_LABELS");

    m_neuralNetwork.init(modelName, modelLabelsName, "");
    m_filterResult = new FilterResult;
}

TensorFlowFilter::~TensorFlowFilter()
{
    delete m_filterResult;
}

QVideoFrame TensorFlowFilter::run(QVideoFrame *input)
{
    if (!m_neuralNetwork.initialized()) {
        return *input;
    }

    m_neuralNetwork.setInputImage(*input, false);
    if (m_neuralNetwork.process()) {
        m_filterResult->clear();
        const QList<DetectionResult>& results = m_neuralNetwork.results();
        for (int i = 0; i < results.size(); ++i) {
            DetectionResult res = results.at(i);
            m_filterResult->m_masks.append(res.mask);
            m_filterResult->m_names.append(res.label);
            m_filterResult->m_confidences.append(res.confidence);
            m_filterResult->m_rects.append(res.objectRect);
            m_filterResult->m_colors.append(res.color);
        }
    }
    Q_EMIT processingFinished(m_filterResult);
    return *input;
}

FilterResult *TensorFlowFilter::filterResult() const
{
    return m_filterResult;
}
