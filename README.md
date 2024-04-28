# QLibCam for Qt6
> libcamera manager, built for Qt6 with QML example

LibCamera is a new industry standard for accessing cameras in embedded linux, Android etc
Unfortunately, Qt dont have backend to access cameras via libcamera API
The project illustarates, how to enumerate and access cameras via libcamera API

Another aspect, which was regressed since Qt5 times - video filters
For unknown reason, QtC decided to remove video filters from multimedia API,
suggesting users to use shaders, which is not usable for tasks like objects recognition:
barcode scanners, ML objects detection and so on
So, the project also provide a functionality, which supports chain of video filters
for given camera

For filters demonstration purposes added a barcode reader filter based on ZXing library:
[ZXing-C++ Library](https://github.com/zxing-cpp/zxing-cpp)
and Qt wrapper, taken from here
[![Scythe Studio](./assets/scytheStudioLogo.png)](https://scythe-studio.com)
and slightly adjusted


