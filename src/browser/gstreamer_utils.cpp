#include "gstreamer_utils.h"

std::optional<QImage> imageFromGstSample(GstSample *sample)
{
    if (sample) {
        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        int width;
        int height;
        bool success = gst_structure_get_int(structure, "width", &width);
        success = success | gst_structure_get_int(structure, "height", &height);
        if (success) {
            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstMapInfo mapInfo;
            gst_buffer_map(buffer, &mapInfo, GST_MAP_READ);
            const std::size_t memcount = mapInfo.maxsize * sizeof(gint8);
            uchar *data = reinterpret_cast<uchar *>(std::malloc(memcount));
            memcpy(data, mapInfo.data, memcount);
            gst_buffer_unmap(buffer, &mapInfo);
            return QImage(
                data,
                width,
                height,
                GST_ROUND_UP_4(width * 3),
                QImage::Format_RGB888,
                [](void *d) { std::free(d); },
                data);
        }
    }
    return {};
}
