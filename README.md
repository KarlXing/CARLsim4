
README for CARLsim SNN simulator and parameter tuning interface (PTI)
-------------------------------------------------------------------------------

CARLsim is available from: http://socsci.uci.edu/~jkrichma/CARLsim/.

For a list of all people who have contributed to this project, please refer to 
AUTHORS.

For installation instructions, please refer to INSTALL.

For a log of source code changes, refer to CHANGELOG.

For a description of added features please refer to the RELEASE_NOTES.


### QUICKSTART INSTALLATION

If the NVIDIA CUDA drivers/toolkits/SDKs are installed and configured 
correctly:

Type 'make' or 'make all' to compile CARLsim and CARLsim examples.

If BOTH the NVIDIA CUDA drivers/toolkits/SDKs and the Evolving Objects 
libraries are installed and configured correctly:

Type 'make pti' to make the pti library.

Type 'make install' to install the pti library.

Type 'make pti_examples' to compile the pti examples.

TO UNINSTALL:	
Type 'make uninstall' to uninstall the pti library.


### SOURCE CODE DIRECTORY DESCRIPTION

<pre>
  Main
directory
    │
    │
    ├── carlsim
    │   ├── include
    │   └── src
    ├── examples
    │   ├── colorblind
    │   │   ├── results
    │   │   ├── scripts
    │   │   └── videos
    │   ├── colorcycle
    │   │   ├── results
    │   │   ├── scripts
    │   │   └── videos
    │   ├── common
    │   │   └── scripts
    │   ├── orientation
    │   │   ├── results
    │   │   ├── scripts
    │   │   └── videos
    │   ├── random
    │   │   └── results
    │   ├── rdk
    │   │   ├── results
    │   │   ├── scripts
    │   │   └── videos
    │   ├── simpleEA
    │   │   └── results
    │   ├── SORFs
    │   │   └── results
    │   ├── tuneFiringRates
    │   │   └── results
    │   ├── v1MTLIP
    │   │   ├── results
    │   │   ├── scripts
    │   │   └── videos
    │   └── v1v4PFC
    │       ├── results
    │       ├── scripts
    │       └── videos
    ├── include
    ├── interface
    │   ├── include
    │   └── src
    ├── libpti
    ├── server
    └── test
</pre>

* Main directory - contains the Makefile, documentation files, and other
directories.

* carlsim - contains the core implementation of CARLsim

* examples - contains all the examples for CARLsim and CARLsim PTI. Each
example is in its own subfolder (along with input videos, scripts, and
results). examples/common contains support files that multiple examples use.

* include -  contains all the header files for the PTI.

* libpti - contains the source code and Makefile includes for the PTI.

* test - contains a regression suite


### MAKEFILE STRUCTURE DESCRIPTION


Variable naming conventions - Environment variables that could be defined 
outside the Makefile are all caps.  Variables that are used only in the 
Makefile are not capitalized.

The Makefile is composed of individual include files.  makefile.mk contains
additional makefile definitions. carlsim.mk and libpti.mk contain specific
build rules and definitions for CARLsim and the PTI library. Each example has
a corresponding include file.
