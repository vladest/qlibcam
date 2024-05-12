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

The code is tested on amd64 Kubuntu linux 23.10 with libcamera 0.2.0

For filters demonstration purposes added a barcode reader filter based on ZXing library:
[ZXing-C++ Library](https://github.com/zxing-cpp/zxing-cpp)
and Qt wrapper, taken from here
[Scythe Studio](https://scythe-studio.com)
and slightly adjusted


> TPU filter using TensorFlow

[TPU Library build](https://coral.ai/docs/notes/build-coral/)

[TensorFlow Library build](https://www.tensorflow.org/lite/guide/build_cmake)
Note: Install build doesnt work on Ubuntu 23.10/TF 2.1.6, so I included header files in the repo, however, its still need to build libedgetpu and tf2 lite libraries

[TensorFlow/TPU example models](https://coral.ai/models/object-detection/)

Set environment variables:
TF2_MODEL - pointing to model file
TF2_MODEL_LABELS - pointing to labels file
Note: Models with size 640x640 most probably will crash the TPU device so use less resolution for the models




