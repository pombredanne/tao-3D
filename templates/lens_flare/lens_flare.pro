# ******************************************************************************
#  lens_flare.pro                                                    Tao project
# ******************************************************************************
# File Description:
# Project file for document template: Lens Flare 

TEMPLATE = subdirs

include(../templates.pri)

files.path  = $$APPINST/templates/lens_flare
files.files = template.ini lens_flare.ddd lens_flare.jpg

images.path = $$APPINST/templates/lens_flare/images
images.files = images/*
 
INSTALLS += files images

SIGN_XL_SOURCES = lens_flare.ddd
include(../sign_template.pri)
