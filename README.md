Chicony Infrared Decoder
========================

A GStreamer element to decode the "raw" output from a Chicony infrared
camera, as found on the ThinkPad X1 Carbon (5th Gen).

Background
----------

When I bought my ThinkPad, one of the available customisation options
was an infrared camera.  It was advertised as supporting "Windows
Hello", but I was curious about how it would work under Linux.

The camera appears as two USB video class devices:

    $ lsusb
    ...
    Bus 001 Device 004: ID 04f2:b5ce Chicony Electronics Co., Ltd
    Bus 001 Device 002: ID 04f2:b5cf Chicony Electronics Co., Ltd

So shows up as `/dev/video0` (the IR camera) and `/dev/video1` (the
colour camera).  The IR camera advertises two modes: Motion-JPEG and
YUYV 4:2:2.

The Motion-JPEG mode produces 640x480 frames, but the red illumination
LEDs flash leading to different frames being overexposed or
underexposed.

Surprisingly, the YUYV mode instead produces a garbled 400x480 image.
The fact the image dimensions are different is the first clue that the
image data isn't in the advertised YUYV format.  Whereas YUYV encodes
two pixels in four bytes, the data produced by the camera appears to
have a period of five bytes.

More specifically, it appears each five bytes encodes four packed 10
bit values.  This also accounts for the width difference: 400 YUYV
pixels translates to 800 bytes, which in turn can hold 640 packed 10
bit values.

The GStreamer Element
---------------------

The provided `chiconyirdec` element allows data from the infrared camera to be processed in a GStreamer pipeline.  For example:

    gst-launch-1.0 v4l2src device=/dev/video0 ! chiconyirdec ! videoconvert ! xvimagesink

The element accepts "video/x-raw,format=(string)YUY2" on the sink
side, and converts it to "video/x-raw,format=(string)GRAY16_LE" with
the appropriate frame width.  The 10 bit pixel values are scaled to 16
bit to avoid losing data.
