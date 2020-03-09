Simple photo and video browser.

* Browse directories and view all photos and videos in there.
* Simple video player.
* Option to recursively collect media from subdirectories.
* Sorting options by name, date, EXIF date.

100% only tested on macOS.

Uses [exiv2][1] for meta data, [gstreamer][2] for video, [Qt][3] for images and GUI,
[sodium-cxx][4] for functional reactive programming, [CMake][5] as build system,
and [Haskell Shelly][6] for deployment.

[1]: https://www.exiv2.org/
[2]: https://gstreamer.freedesktop.org/
[3]: https://www.qt.io/
[4]: https://github.com/SodiumFRP/sodium-cxx
[5]: https://cmake.org/
[6]: https://github.com/gregwebs/Shelly.hs
