include $(APPSPATH)/libs.mk

APP = v6-monitor

all:
	$(CC) $(CFLAGS) -Wno-error -Os -s *.c -o $(APP) -std=gnu99 -I$(PRIVATE_APPSPATH)/include/ -L$(INSTALLDIR)/lib $(X_PRIVATE_LIBS) $(X_PUBLIC_LIBS)

clean:
	-rm -f *.o $(APP)

install:all
	cp -f $(APP) $(INSTALLDIR)/usr/sbin

