#pragma once

#include <optional.h>

#include <QImage>

#include <gst/gst.h>

#include <functional>

template<typename T>
class GstRef
{
public:
    GstRef() = default;
    GstRef(T *element)
        : m_element(element)
    {}

    GstRef(const GstRef<T> &other) = default;
    GstRef &operator=(const GstRef<T> &) = delete;
    ~GstRef() { reset(); }

    T *operator()() const { return m_element; }

    void reset(T *element = nullptr)
    {
        if (m_cleanup)
            m_cleanup(m_element);
        if (m_element)
            gst_object_unref(m_element);
        m_element = element;
    }

    using CleanUp = std::function<void(T *)>;
    void setCleanUp(const CleanUp &cleanup) { m_cleanup = cleanup; }

private:
    T *m_element = nullptr;
    CleanUp m_cleanup;
};

std::optional<QImage> imageFromGstSample(GstSample *sample);
