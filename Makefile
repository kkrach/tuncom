
TARGET=tuncom

CROSS_COMPILE=/opt/crosstool-ng-powerpc/bin/powerpc-e500v2-linux-gnuspe-
CC=${CROSS_COMPILE}gcc
CFLAGS += -Wall -Werror
IP=172.16.10.123

SRCS=tuncom.c

${TARGET}: $(SRCS)
	${CC} ${CFLAGS} $^ -o $@

clean:
	rm -f $(TARGET)

upload: ${TARGET}
	scp $(TARGET) root@$(IP):/var/lib/work/
