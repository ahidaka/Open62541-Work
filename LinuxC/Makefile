#

TARGET = EnOceanJob

CFLAGS = -g -Wall -std=c99 open62541.c

all: $(TARGET)

EnOceanJob: EnOceanJob.c
	gcc $(CFLAGS) EnOceanJob.c -o EnOceanJob

clean:
	/bin/rm -f *.o *~ $(TARGET)
