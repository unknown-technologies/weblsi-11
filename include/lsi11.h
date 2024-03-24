#ifndef __LSI_11_H__
#define __LSI_11_H__

#include "types.h"

#ifndef _BV
#define	_BV(x)	(1 << (x))
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define	U16B(x)			(x)
#define	U32B(x)			(x)
#define	U64B(x)			(x)
#define	U16L(x)			__builtin_bswap16(x)
#define	U32L(x)			__builtin_bswap32(x)
#define	U64L(x)			__builtin_bswap64(x)
#else
#define	U16B(x)			__builtin_bswap16(x)
#define	U32B(x)			__builtin_bswap32(x)
#define	U64B(x)			__builtin_bswap64(x)
#define	U16L(x)			(x)
#define	U32L(x)			(x)
#define	U64L(x)			(x)
#endif

/* Main memory size: 32kW / 64kB */
#define	MSV11D_SIZE		(65536 - 2 * 4096)

/* QBUS interrupt request delay */
#define	QBUS_DELAY		20
#define	QBUS_DELAY_JITTER	10

/* SRAM offsets for emulated storage devices */
#define	SRAM_RX02_UNIT0		0
#define	SRAM_RX02_UNIT1		(SRAM_RX02_UNIT0 + 512512)

extern volatile int lsi_irq;

typedef struct {
	u16	addr;
	u16	val;
	u8	input;
	u8	state;
	u8	next;
	u8	buf[16];
	u8	buf_r;
	u8	buf_sz;
} KD11ODT;

typedef struct {
	u16	r[8];
	u16	psw;
	KD11ODT	odt;
	u8	state;
	u16	trap;
} KD11;

typedef struct QBUS QBUS;
typedef struct LSI11 LSI11;

struct QBUS {
	u16	trap;
	u16	delay;
	u16	irq;
	u16	nxm;
};

/* peripherals */
typedef struct {
	u16	data[MSV11D_SIZE / 2];
} MSV11D;

typedef struct {
	u16	irq;

	u16	rcsr0;
	u16	rbuf0;
	u16	xcsr0;
	u16	xbuf0;

	u16	rcsr1;
	u16	rbuf1;
	u16	xcsr1;
	u16	xbuf1;

	u16	rcsr2;
	u16	rbuf2;
	u16	xcsr2;
	u16	xbuf2;

	u16	rcsr3;
	u16	rbuf3;
	u16	xcsr3;
	u16	xbuf3;
} DLV11J;

typedef struct {
	u16	pcr;
	u16	scratch;
	u16	option;
	u16	display;
	u16	ltc;
	u16	switches;
} BDV11;

typedef struct {
	u16	rx2cs;
	u16	rx2db;

	u16	rx2ta;	/* RX Track Address */
	u16	rx2sa;	/* RX Sector Address */
	u16	rx2wc;	/* RX Word Count Register */
	u16	rx2ba;	/* RX Bus Address Register */
	u16	rx2es;	/* RX Error and Status */

	u16	state;
	u16	error;

	u16	irq;
	u16	nxm;

	u16	buffer[128];
} RXV21;

typedef struct {
	u16	csr;
	u16	bar;
	u16	dar;
	u16	bae;
	u16	wc;

	u16	state;
	u16	fifo[8];
	u16	error;

	u16	irq;
	BOOL	nxm;

	u16	hs[4];
	u16	ca[4];
	u16	sa[4];

	u16	dma[256];
} RLV12;

typedef struct {
	u16	csr;
	u16	irq;
} SPI11;

typedef struct {
	u16	csr;
} DIO11;

typedef struct {
	u16	cursor;
	u16	buffer[64];
	u16	csr;
	u16	mpr;
	u16	op1;
	u16	op2;
	u16	op3;
} ESL11;

/* global LSI-11 state */
struct LSI11 {
	QBUS	bus;
	KD11	kd11;
	MSV11D	msv11;
	DLV11J	dlv11;
	BDV11	bdv11;
	RXV21	rxv21;
	RLV12	rlv12;
	SPI11	spi11;
	DIO11	dio11;
	ESL11	esl11;
};

extern LSI11 lsi;

/* peripheral subroutines */

void	MSV11DInit(void);

void	DLV11JInit(void);
void	DLV11JReset(void);
void	DLV11JSend(unsigned char c);
u16	DLV11JRead(u16 address);
void	DLV11JWrite(u16 address, u16 value);
void	DLV11JStep(void);

void	BDV11Init(void);
void	BDV11Reset(void);
void	BDV11Step(void);
u16	BDV11Read(u16 address);
void	BDV11Write(u16 address, u16 value);
void	BDV11SetSwitches(u16 switches);

void	RXV21Init(void);
void	RXV21Reset(void);
u16	RXV21Read(u16 address);
void	RXV21Write(u16 address, u16 value);
void	RXV21Step(void);

void	RLV12Init(void);
void	RLV12Reset(void);
void	RLV12Step(void);
u16	RLV12Read(u16 address);
void	RLV12Write(u16 address, u16 value);

void	SPI11Init(void);
void	SPI11Reset(void);
u16	SPI11Read(u16 address);
void	SPI11Write(u16 address, u16 value);
void	SPI11Step(void);

void	DIO11Init(void);
void	DIO11Reset(void);
u16	DIO11Read(u16 address);
void	DIO11Write(u16 address, u16 value);

void	ESL11Init(void);
void	ESL11Reset(void);
u16	ESL11Read(u16 address);
void	ESL11Write(u16 address, u16 value);

/* KD11 subroutines */
void	KD11Init(void);
void	KD11Reset(void);
void	KD11Step(void);
void	KD11Trap(int n);

/* LSI-11 subroutines */
void	LSI11Init(void);
void	LSI11Reset(void);
void	LSI11Restart(void);
void	LSI11Step(void);

int	LSI11Interrupt(int n);
void	QBUSReset(void);

/* small routines */
static inline void LSI11CancelInterrupt(int n)
{
	if(lsi.bus.irq == n) {
		lsi.bus.irq = 0;
	}

	if(lsi.bus.trap == n) {
		lsi.bus.trap = 0;
	}
}


/* performance critical memory access routines */
static inline u16 LSI11ReadDMA(u16 address, BOOL* nxm)
{
	*nxm = 0;
	address &= 0xFFFE;

	if(address < MSV11D_SIZE) {
		return lsi.msv11.data[address >> 1];
	} else if(address >= 0173000 && address <= 0173776) {
		return BDV11Read(address);
	} else {
		switch(address) {
			case 0177170:
			case 0177172:
				return RXV21Read(address);
			case 0174400:
			case 0174402:
			case 0174404:
			case 0174406:
			case 0174410:
				return RLV12Read(address);
			case 0177520:
			case 0177522:
			case 0177524:
			case 0177546:
				return BDV11Read(address);
			case 0176500:
			case 0176502:
			case 0176504:
			case 0176506:
			case 0176510:
			case 0176512:
			case 0176514:
			case 0176516:
			case 0177560:
			case 0177562:
			case 0177564:
			case 0177566:
				return DLV11JRead(address);
		}
	}

	*nxm = 1;
	return 0;
}

static inline u16 LSI11WriteDMA(u16 address, u16 value)
{
	address &= 0xFFFE;

	if(address < MSV11D_SIZE) {
		lsi.msv11.data[address >> 1] = value;
	} else if(address >= 0173000 && address <= 0173776) {
		BDV11Write(address, value);
	} else {
		switch(address) {
			case 0177170:
			case 0177172:
				RXV21Write(address, value);
				break;
			case 0174400:
			case 0174402:
			case 0174404:
			case 0174406:
			case 0174410:
				RLV12Write(address, value);
				break;
			case 0177520:
			case 0177522:
			case 0177524:
			case 0177546:
				BDV11Write(address, value);
				break;
			case 0176500:
			case 0176502:
			case 0176504:
			case 0176506:
			case 0176510:
			case 0176512:
			case 0176514:
			case 0176516:
			case 0177560:
			case 0177562:
			case 0177564:
			case 0177566:
				DLV11JWrite(address, value);
				break;
			default:
				return 1;
		}
	}

	return 0;
}

static inline u16 LSI11Read(u16 address)
{
	BOOL nxm;

	u16 data = LSI11ReadDMA(address, &nxm);

	if(nxm) {
		lsi.bus.nxm = 1;
	}

	return data;
}

static inline void LSI11Write(u16 address, u16 value)
{
	if(LSI11WriteDMA(address, value)) {
		lsi.bus.nxm = 1;
	}
}

static inline void LSIIRQ(void)
{
	lsi_irq = 1;
}

#endif
