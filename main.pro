# ******************************************************************************
#  main.pro                                                         Tao project
# ******************************************************************************
# File Description:
# Main Qt build file for Tao
# ******************************************************************************
# This document is released under the GNU General Public License.
# See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
# (C) 2010 Jerome Forissier <jerome@taodyne.com>
# (C) 2010 Christophe de Dinechin <christophe@taodyne.com>
# (C) 2010 Taodyne SAS
# ******************************************************************************

# Generate Makefiles with:
#
# (MacOSX)  qmake -r -spec macx-g++ [options]
# (Windows) qmake -r -spec win32-g++ [options]
# (Linux)   qmake -r -spec linux-g++ [options]
#
#   Options:
#
#   DEFINES += CFG_NOGIT
#     Build without Git support for Tao documents (Git is still used for module
#     update)
#   DEFINES += CFG_NOSTEREO
#     Build without support for stereoscopic displays
#   DEFINES += CFG_NOSRCEDIT
#     Build without document source editor
#
# make
# make install     # installs locally under ./install/
# make clean
# make distclean
#
# Note that parallel build (make -jX) sometimes fails for the install target.
# Instead, do:
#   make -j3 && make install

# Include global definitions and rules.
include(main.pri)

TEMPLATE = subdirs
SUBDIRS  = libxlr tao modules ssh_ask_pass tests
win32:SUBDIRS += detach

tao.depends = libxlr
modules.depends = tao
tests.depends = tao
