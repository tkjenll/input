SOURCES += \
attributes/attributecontroller.cpp \
attributes/attributedata.cpp \
attributes/attributeformmodel.cpp \
attributes/attributeformproxymodel.cpp \
attributes/attributepreviewcontroller.cpp \
attributes/attributetabmodel.cpp \
attributes/attributetabproxymodel.cpp \
attributes/rememberattributescontroller.cpp \
attributes/fieldvalidator.cpp \
featurelayerpair.cpp \
featurehighlight.cpp \
highlightsgnode.cpp \
identifykit.cpp \
positionkit.cpp \
scalebarkit.cpp \
simulatedpositionsource.cpp \
featureslistmodel.cpp \
inputhelp.cpp \
activelayer.cpp \
fieldsmodel.cpp \
layersmodel.cpp \
layersproxymodel.cpp \
main.cpp \
projectwizard.cpp \
loader.cpp \
digitizingcontroller.cpp \
mapthemesmodel.cpp \
appsettings.cpp \
androidutils.cpp \
inputexpressionfunctions.cpp \
inpututils.cpp \
positiondirection.cpp \
purchasing.cpp \
variablesmanager.cpp \
ios/iosimagepicker.cpp \
ios/iosutils.cpp \
inputprojutils.cpp \
codefilter.cpp \
qrdecoder.cpp \
projectsmodel.cpp \
projectsproxymodel.cpp \
compass.cpp \
relationfeaturesmodel.cpp \
relationreferencefeaturesmodel.cpp

HEADERS += \
attributes/attributecontroller.h \
attributes/attributedata.h \
attributes/attributeformmodel.h \
attributes/attributeformproxymodel.h \
attributes/attributepreviewcontroller.h \
attributes/attributetabmodel.h \
attributes/attributetabproxymodel.h \
attributes/rememberattributescontroller.h \
attributes/fieldvalidator.h \
highlightsgnode.h \
featurelayerpair.h \
featurehighlight.h \
identifykit.h \
positionkit.h \
scalebarkit.h \
simulatedpositionsource.h \
featureslistmodel.h \
inputhelp.h \
activelayer.h \
fieldsmodel.h \
layersmodel.h \
layersproxymodel.h \
projectwizard.h \
loader.h \
digitizingcontroller.h \
mapthemesmodel.h \
appsettings.h \
androidutils.h \
inputexpressionfunctions.h \
inpututils.h \
positiondirection.h \
purchasing.h \
variablesmanager.h \
ios/iosimagepicker.h \
ios/iosutils.h \
inputprojutils.h \
codefilter.h \
qrdecoder.h \
projectsmodel.h \
projectsproxymodel.h \
compass.h \
relationfeaturesmodel.h \
relationreferencefeaturesmodel.h

contains(DEFINES, INPUT_TEST) {

  contains(DEFINES, APPLE_PURCHASING) {
    error("invalid combination of flags. INPUT_TEST cannot be defined with APPLE_PURCHASING")
   }

  SOURCES += \
      test/inputtests.cpp \
      test/testutils.cpp \
      test/testutilsfunctions.cpp \
      test/testmerginapi.cpp \
      test/testingpurchasingbackend.cpp \
      test/testpurchasing.cpp \
      test/testlinks.cpp \
      test/testattributepreviewcontroller.cpp \
      test/testattributecontroller.cpp \
      test/testidentifykit.cpp \
      test/testpositionkit.cpp \
      test/testrememberattributescontroller.cpp \
      test/testscalebarkit.cpp \
      test/testvariablesmanager.cpp \
      test/testformeditors.cpp \

  HEADERS += \
      test/inputtests.h \
      test/testutils.h \
      test/testutilsfunctions.h \
      test/testmerginapi.h \
      test/testingpurchasingbackend.h \
      test/testpurchasing.h \
      test/testpositionkit.h \
      test/testlinks.h \
      test/testattributepreviewcontroller.h \
      test/testattributecontroller.h \
      test/testidentifykit.h \
      test/testrememberattributescontroller.h \
      test/testscalebarkit.h \
      test/testvariablesmanager.h \
      test/testformeditors.h \
}

contains(DEFINES, APPLE_PURCHASING) {
  HEADERS += \
      ios/iospurchasing.h

  OBJECTIVE_SOURCES += \
      ios/iospurchasing.mm
}

RESOURCES += \
    img/pics.qrc \
    qml/qml.qrc \
    fonts/fonts.qrc \
    i18n/input_i18n.qrc

TRANSLATIONS += $$files(i18n/*.ts)

# this makes the manifest visible from Qt Creator
DISTFILES += \
    qml/MapThemePanel.qml \
    qml/Notification.qml
