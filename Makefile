VERSION := fw version: v1.48
CC := /opt/am1808/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-linux-gnueabi-gcc
LD := /opt/am1808/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-linux-gnueabi-ld
STRIP := /opt/am1808/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-linux-gnueabi-strip
RPATH := -Wl,-rpath=/home/root/applib
EXECUTABLE := app
#REMOTE_PC := 192.168.5.254
APPLIBS := -lcommon -lexio -lpio -lcli -lserial -lplc -lstorage -lgsm -leth -lconnection -lujuk -lgubanov -lpool -lmicron_v1 -lmayk -lunknown -lacounts -lpthread
FILES := common.c exio.c pio.c cli.c serial.c plc.c storage.c gsm.c eth.c connection.c ujuk.c gubanov.c pool.c micron_v1.c mayk.c unknown.c acounts.c debug.c main.c

WARNINGS := -Wall -Wextra -Wformat=2 -Winit-self -Wunreachable-code -Wfloat-equal -Wstrict-overflow=4 -Wundef -Wbad-function-cast -Wlogical-op -Wcast-align -Wcast-qual -Wshadow
CC_LIB_FLAGS := -g ${WARNINGS} -std=gnu99
LB_LIB_FLAGS := -g ${WARNINGS} -std=gnu99 -shared
CC_APP_FLAGS := -g ${WARNINGS} -std=gnu99 -L.

RFLAG_FILE := release_flag.h

.PHONY : all clean app applib cleanapp cleanlib cleanobject scp

all: rflag2debug applib app rflag_clean

release: clean rflag2release applib app strip md5 rflag_clean

static: rflag2debug monogapp rflag_clean

monoapp:
	-@echo "Compiling mono app..."
	-@$(CC) $(FILES) -o $(EXECUTABLE) -lpthread -Wall -g

rflag2release:
	-@echo "Changing rflag to \"Release\""
	-@echo "#define REV_COMPILE_RELEASE 1" > $(RFLAG_FILE)

rflag2debug:
	-@echo "Changing rflag to \"Debug\""
	-@echo "#define REV_COMPILE_RELEASE 0" > $(RFLAG_FILE)

rflag_clean:
	-@echo "Removing rflag"
	-@rm -f $(RFLAG_FILE)

md5:
	-@echo "Calculating md5..."
	-@echo $(VERSION) > version.info
	-@md5sum lib*.so $(EXECUTABLE) >> version.info


app:
	-@echo "Compiling application..."
	-@$(CC) $(CC_APP_FLAGS) main.c debug.c $(APPLIBS) -o $(EXECUTABLE) $(RPATH)

strip:
	-@echo "Striping application..."
	-@$(STRIP) --strip-all $(EXECUTABLE)
	-@echo "Striping labraries..."
	-@$(STRIP) --strip-all lib*.so

scp:
	-@echo "Copy application and libraries to $(REMOTE_PC)"
	scp lib*.so root@$(REMOTE_PC):/home/root/applib
	scp $(EXECUTABLE) version.info root@$(REMOTE_PC):/home/root

cleanapp:
	-@echo "Removing executable..."
	-@rm -rf $(EXECUTABLE)
	-@echo "Removing temporary IDE files..."
	-@rm -f *~
	-@echo "Removing version.info"
	-@rm -f version.info

cleanlib:
	-@echo "Removing labrary-files..."
	-@rm -rf *.so
	-@rm -rf *.so.*

cleanobjects:
	-@echo "Removing object-files..."
	-@rm -rf *.o

clean: rflag_clean cleanobjects cleanapp cleanlib

applib: libcommon libexio libpio libcli libserial libplc libstorage libgsm libeth libconnection libujuk libgubanov libpool libmicron_v1 libmayk libunknown libacounts

libcommon:
	-@echo "Compiling common.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC common.c -o common.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libcommon.so -o libcommon.so common.o $(RPATH)

libexio:
	-@echo "Compiling exio.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC exio.c -o exio.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libexio.so -o libexio.so exio.o $(RPATH)

libpio:
	-@echo "Compiling pio.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC pio.c -o pio.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libpio.so -o libpio.so pio.o $(RPATH)

libcli:
	-@echo "Compiling cli.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC cli.c -o cli.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libcli.so -o libcli.so cli.o $(RPATH)

libserial:
	-@echo "Compiling serial.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC serial.c -o serial.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libserial.so -o libserial.so serial.o $(RPATH)

libplc:
	-@echo "Compiling plc.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC plc.c -o plc.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libplc.so -o libplc.so plc.o $(RPATH)

libstorage:
	-@echo "Compiling storage.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC storage.c -o storage.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libstorage.so -o libstorage.so storage.o $(RPATH)

libgsm:
	-@echo "Compiling gsm.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC gsm.c -o gsm.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libgsm.so -o libgsm.so gsm.o $(RPATH)

libeth:
	-@echo "Compiling eth.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC eth.c -o eth.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libeth.so -o libeth.so eth.o $(RPATH)

libconnection:
	-@echo "Compiling connection.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC connection.c -o connection.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libconnection.so -o libconnection.so connection.o $(RPATH)

libujuk:
	-@echo "Compiling ujuk.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC ujuk.c -o ujuk.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libujuk.so -o libujuk.so ujuk.o $(RPATH)

libgubanov:
	-@echo "Compiling gubanov.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC gubanov.c -o gubanov.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libgubanov.so -o libgubanov.so gubanov.o $(RPATH)

libpool:
	-@echo "Compiling pool.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC pool.c -o pool.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libpool.so -o libpool.so pool.o $(RPATH)

libmicron_v1:
	-@echo "Compiling micron_v1.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC micron_v1.c -o micron_v1.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libmicron_v1.so -o libmicron_v1.so micron_v1.o $(RPATH)

libmayk:
	-@echo "Compiling mayk.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC mayk.c -o mayk.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libmayk.so -o libmayk.so mayk.o $(RPATH)

libunknown:
	-@echo "Compiling unknown.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC unknown.c -o unknown.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libunknown.so -o libunknown.so unknown.o $(RPATH)

libacounts:
	-@echo "Compiling acounts.c..."
	-@$(CC) $(CC_LIB_FLAGS) -c -fPIC acounts.c -o acounts.o
	-@$(CC) $(LB_LIB_FLAGS) -Wl,-soname,libacounts.so -o libacounts.so acounts.o $(RPATH)
