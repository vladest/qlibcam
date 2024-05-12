#pragma once

#include <QObject>
#include <QVideoFrame>
#include <QVariantList>
#include "abstractvideofilter.h"
#include "tensorflowtpuneuralnetwork.h"

class TensorFlowFilter;

class FilterResult : public QObject
{
    Q_OBJECT

public:
    void clear()
    {
        m_rects.clear();
        m_confidences.clear();
        m_names.clear();
        m_masks.clear();
        m_colors.clear();
    }

public Q_SLOTS:

    QVariantList rects() const { return m_rects; }
    QVariantList confidences() const { return m_confidences; }
    QStringList names() const { return m_names; }
    QList<QImage> masks() const  { return m_masks; }
    QList<QColor> colors() const  { return m_colors; }

private:
    QVariantList m_rects;
    QVariantList m_confidences;
    QStringList m_names;
    QList<QImage> m_masks;
    QList<QColor> m_colors;
    friend class TensorFlowFilter;
};

class TensorFlowFilter: public AbstractVideoFilter
{
    Q_OBJECT
    Q_PROPERTY(FilterResult *filterResult READ filterResult CONSTANT FINAL)

public:
    explicit TensorFlowFilter(QObject *parent = nullptr);
    ~TensorFlowFilter() override;

    FilterResult *filterResult() const;

Q_SIGNALS:
    void processingFinished(FilterResult * result);

private:
    QVideoFrame run(QVideoFrame *input) override;
    TensorFlowTPUNeuralNetwork m_neuralNetwork;
    FilterResult* m_filterResult = nullptr;
};

Q_DECLARE_METATYPE(FilterResult*)
