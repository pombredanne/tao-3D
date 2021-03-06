# ******************************************************************************
#  mapping.pro                                                       Tao project
# ******************************************************************************
# File Description:
# Project file for document template: Mapping

TEMPLATE = subdirs

include(../templates.pri)

files.path  = $$APPINST/templates/mapping
files.files = template.ini mapping.ddd mapping.png theme.xl

images.path = $$APPINST/templates/mapping/images
images.files = images/*
 
INSTALLS += files images

SIGN_XL_SOURCES = mapping.ddd
include(../sign_template.pri)
