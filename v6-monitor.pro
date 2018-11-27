TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    addrs.c \
    event.c \
    rtnlmsg.c \
    lib.c \
    sigmsg.c \
    monitor.c \
    salinfo.c

HEADERS += \
    addrs.h \
    debug.h \
    event.h \
    ipv6_event.h \
    rtnlmsg.h \
    lib.h \
    sigmsg.h \
    monitor.h \
    salinfo.h \
    link.h
