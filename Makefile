CC?=	cc

OBJS=	nbu-export.o nbu.o utf.o compat/getprogname.o compat/reallocarray.o

.PHONY: all clean

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

all: nbu-export

nbu-export: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

clean:
	rm -f nbu-export nbu-export.core core ${OBJS}
