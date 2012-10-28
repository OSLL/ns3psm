######### NS-3 related
DEFINES += WITH_NS3


HEADERS += \
    designer/designermode.h \
    designer/designerview.h \
    designer/designerscene.h \
    designer/designernoderect.h \
    designer/abstractdesigneritem.h \
    designer/designerp2plinkline.h \
    designer/designerconstants.h \
    designer/nodepropertiestab.h \
    designer/p2plinkmanager.h \
    designer/nodenetworkwidget.h \
    designer/appmanager.h \
    designer/designermodule.h \
    designer/applicationswidget.h \
    designer/attributerandomvariablewidget.h \
    designer/attributeipv4addrportwidget.h \
    designer/attributeprotocolwidget.h

SOURCES += \
    designer/designermode.cpp \
    designer/designerview.cpp \
    designer/designerscene.cpp \
    designer/designernoderect.cpp \
    designer/abstractdesigneritem.cpp \
    designer/designerp2plinkline.cpp \
    designer/nodepropertiestab.cpp \
    designer/p2plinkmanager.cpp \
    designer/nodenetworkwidget.cpp \
    designer/appmanager.cpp \
    designer/applicationswidget.cpp \
    designer/attributerandomvariablewidget.cpp \
    designer/attributeipv4addrportwidget.cpp \
    designer/attributeprotocolwidget.cpp

INCLUDEPATH += ../ns-3.14.1/build
LIBS += -L../ns-3.14.1/build

LIBS+=-lns3.14.1-aodv-debug
LIBS+=-lns3.14.1-antenna-debug
LIBS+=-lns3.14.1-buildings-debug
LIBS+=-lns3.14.1-applications-debug
LIBS+=-lns3.14.1-wifi-debug
LIBS+=-lns3.14.1-bridge-debug
LIBS+=-lns3.14.1-olsr-debug
LIBS+=-lns3.14.1-config-store-debug
LIBS+=-lns3.14.1-point-to-point-layout-debug
LIBS+=-lns3.14.1-core-debug
LIBS+=-lns3.14.1-point-to-point-debug
LIBS+=-lns3.14.1-csma-layout-debug
LIBS+=-lns3.14.1-propagation-debug
LIBS+=-lns3.14.1-csma-debug
LIBS+=-lns3.14.1-spectrum-debug
LIBS+=-lns3.14.1-dsdv-debug
LIBS+=-lns3.14.1-stats-debug
LIBS+=-lns3.14.1-energy-debug
LIBS+=-lns3.14.1-flow-monitor-debug
LIBS+=-lns3.14.1-test-debug
LIBS+=-lns3.14.1-internet-debug
LIBS+=-lns3.14.1-tools-debug
LIBS+=-lns3.14.1-lte-debug
LIBS+=-lns3.14.1-topology-read-debug
LIBS+=-lns3.14.1-mesh-debug
LIBS+=-lns3.14.1-uan-debug
LIBS+=-lns3.14.1-mobility-debug
LIBS+=-lns3.14.1-virtual-net-device-debug
LIBS+=-lns3.14.1-mpi-debug
LIBS+=-lns3.14.1-netanim-debug
LIBS+=-lns3.14.1-wifi-debug
LIBS+=-lns3.14.1-network-debug
LIBS+=-lns3.14.1-wimax-debug
LIBS+=-lns3.14.1-nix-vector-routing-debug

DEFINES += _DEBUG
DEFINES += NS3_ASSERT_ENABLE
DEFINES += NS3_LOG_ENABLE

