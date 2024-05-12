#pragma once

#include <QObject>
#include <QVideoFrame>

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
