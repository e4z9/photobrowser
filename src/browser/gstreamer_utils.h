#pragma once

#include <optional.h>

#include <QImage>

#include <gst/gst.h>

#include <functional>

class GstElementRef
{
public:
    GstElementRef() = default;
    GstElementRef(GstElement *element);
    GstElementRef(const GstElementRef &other) = default;
    GstElementRef &operator=(const GstElementRef &) = delete;
    ~GstElementRef();

    GstElement *operator()() const;
    void reset(GstElement *element = nullptr);

    using CleanUp = std::function<void(GstElement *)>;
    void setCleanUp(const CleanUp &cleanup);

private:
    GstElement *m_element = nullptr;
    CleanUp m_cleanup;
};

std::optional<QImage> imageFromGstSample(GstSample *sample);
