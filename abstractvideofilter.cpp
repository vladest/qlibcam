#include "abstractvideofilter.h"

AbstractVideoFilter::AbstractVideoFilter(QObject *parent) : QObject(parent) {}

void AbstractVideoFilter::setActive(bool v)
{
    m_active = v;
    Q_EMIT activeChanged();
}

