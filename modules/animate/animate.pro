# *****************************************************************************
#  animate.pro                                                     Tao project
# *****************************************************************************
# 
#   File Description:
# 
#     Qt configuration file for the 'animate' module
# 
# 
# 
# 
# 
# 
# 
# 
# *****************************************************************************
# This document is released under the GNU General Public License v3.
# See file COPYING for details.
#  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
#  (C) 2010 Taodyne SAS
# *****************************************************************************

MODINSTDIR = animate

include(../modules.pri)

OTHER_FILES = animate.xl

INSTALLS    -= thismod_bin

QMAKE_SUBSTITUTES = doc/Doxyfile.in
DOXYFILE = doc/Doxyfile
DOXYLANG = en,fr
include(../modules_doc.pri)
