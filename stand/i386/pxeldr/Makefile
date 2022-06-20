# $FreeBSD$

.include <bsd.init.mk>

PROG=
INTERNALPROG=
FILES=	${BOOT}
MAN=	${BOOT}.8
#SRCS=	${LDR}.S
CLEANFILES+= ${BOOT}

BOOT=	pxeboot
LDR=	pxeldr
ORG=	0x7c00
LOADER=	pxeldr1

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x2000

.if defined(BOOT_PXELDR_PROBE_KEYBOARD)
CFLAGS+=-DPROBE_KEYBOARD
.endif

.if defined(BOOT_PXELDR_ALWAYS_SERIAL)
CFLAGS+=-DALWAYS_SERIAL
.endif

CFLAGS+=-I${BOOTSRC}/i386/common
CFLAGS+=-I${BOOTSRC}/i386/libi386
CFLAGS+=-I${BOOTSRC}/common

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

CFLAGS+=-fomit-frame-pointer \
	-mrtd \
	-mregparm=3 \
	-DFLAGS=${BOOT_BOOT1_FLAGS} \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${LDRSRC} \
	-Wall -Waggregate-return -Wbad-function-cast -Wno-cast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings

LOADERBIN= ${BOOTOBJ}/i386/pxeldr/${LOADER}.bin

CLEANFILES+= ${BOOT}.tmp

LDFLAGS+=${LDFLAGS_BIN}

CLEANFILES+= ${LOADER}

pxeboot: pxeldr.bin pxeldr1.ld
	cat pxeldr.bin pxeldr1.ld > ${.TARGET}.tmp
	${DD} if=${.TARGET}.tmp of=${.TARGET} bs=2048 conv=sync
	rm ${.TARGET}.tmp

pxeldr.bin: pxeldr.out
	${OBJCOPY} -S -O binary pxeldr.out ${.TARGET}

pxeldr.out: pxeldr.o
	${LD} ${LD_FLAGS} --defsym ORG=${ORG1} -T ${LDSCRIPT} -o ${.TARGET} pxeldr.o

CLEANFILES+= pxeldr.bin pxeldr.out

pxeldr1.ld: pxeldr1.bin pxeldr1.ldr ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l pxeldr1.ldr \
	    -o ${.TARGET} -P 1 pxeldr1.bin

pxeldr1.ldr:
	${DD} if=/dev/zero of=${.TARGET} bs=512 count=1

pxeldr1.bin: pxeldr1.out
	${OBJCOPY} -S -O binary pxeldr1.out ${.TARGET}

CLEANFILES+= pxeldr1.ld pxeldr1.bin

# For __ashldi3
.PATH: ${SRCTOP}/contrib/llvm-project/compiler-rt/lib/builtins
CFLAGS.ashldi3.c=	-Wno-missing-prototypes -Wno-missing-declarations
CLEANFILES+=	ashldi3.o

pxeldr1.out: ${BTXCRT} pxeldr1.o sio.o ashldi3.o
	${LD} ${LD_FLAGS} --defsym ORG=${ORG2} -T ${LDSCRIPT} -o ${.TARGET} ${.ALLSRC}

CLEANFILES+= pxeldr1.out sio.o

SRCS=	pxeldr.S pxeldr1.c

.include <bsd.prog.mk>
