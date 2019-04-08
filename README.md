# dv-runtime

Event-based processing framework for neuromorphic devices, written in C/C++, targeting embedded and desktop systems. <br />

# Dependencies:

NOTE: if you intend to install the latest git master checkout, please also make sure to use the latest
libcaer git master checkout, as we often depend on new features of the libcaer development version.

Linux, MacOS X or Windows (for Windows build instructions see README.Windows) <br />
gcc >= 7.0 or clang >= 5.0 <br />
cmake >= 2.8.12 <br />
Boost >= 1.50 (with system, filesystem, iostreams, program_options) <br />
OpenSSL (for Boost.ASIO SSL) <br />
OpenCV >= 3.1.0 <br />
libcaer >= 3.1.2 <br />
Optional: tcmalloc >= 2.2 (faster memory allocation) <br />
Optional: SFML >= 2.3.0, glew >= 1.10.0 (visualizer module) <br />

Install all dependencies manually on Ubuntu Bionic:
$ sudo apt install git cmake build-essential pkg-config libboost-all-dev libusb-1.0-0-dev libserialport-dev libopencv-contrib-dev libopencv-dev libsfml-dev libglew-dev

# Installation

1) configure: <br />

$ cmake -DCMAKE_INSTALL_PREFIX=/usr <OPTIONS> . <br />

The following options are currently supported: <br />
-DENABLE_TCMALLOC=1 -- Enables usage of TCMalloc from Google to allocate memory. <br />
-DENABLE_VISUALIZER=1 -- Open separate windows in which to visualize data. <br />

2) build:
<br />
$ make
<br />

3) install:
<br />
$ make install
<br />

# Usage

You will need a 'caer-config.xml' file that specifies which and how modules
should be interconnected. A good starting point is 'docs/davis-config.xml',
please also read through 'docs/modules.txt' for an explanation of the modules
system and its configuration syntax.
<br />
$ dv-runtime (see docs/ for more information) <br />
$ dv-control (command-line run-time control program) <br />

# Help

Please use our GitLab bug tracker to report issues and bugs, or
our Google Groups mailing list for discussions and announcements.

BUG TRACKER: https://gitlab.com/inivation/caer/issues/
MAILING LIST: https://groups.google.com/d/forum/caer-users/

BUILD STATUS: https://travis-ci.org/inivation/caer/
CODE ANALYSIS: https://sonarcloud.io/dashboard?id=com.inivation.caer
