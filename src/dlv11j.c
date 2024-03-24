#include "types.h"
#include "lsi11.h"

#define	RCSR_READER_ENABLE	_BV(0)
#define	RCSR_RCVR_INT		_BV(6)
#define	RCSR_RCVR_DONE		_BV(7)

#define	RBUF_ERROR		_BV(15)
#define	RBUF_OVERRUN		_BV(14)
#define	RBUF_FRAMING_ERROR	_BV(13)
#define	RBUF_PARITY_ERROR	_BV(12)
#define	RBUF_ERROR_MASK		(RBUF_OVERRUN | RBUF_FRAMING_ERROR \
		| RBUF_PARITY_ERROR)

#define	XCSR_TRANSMIT_READY	_BV(7)
#define	XCSR_TRANSMIT_INT	_BV(6)
#define	XCSR_TRANSMIT_BREAK	_BV(0)

#define	RCSR_WR_MASK		(RCSR_RCVR_INT | RCSR_READER_ENABLE)
#define	XCSR_WR_MASK		(XCSR_TRANSMIT_INT | XCSR_TRANSMIT_BREAK)

#define	IRQ(x)			if(!LSI11Interrupt(x)) { dlv->irq = (x); LSIIRQ(); }

#define	DC1			0x11
#define	DC2			0x12
#define	DC3			0x13

#define	DLV11J_BUF		512
static unsigned char buf[DLV11J_BUF];
static unsigned int buf_r = 0;
static unsigned int buf_w = 0;
static unsigned int buf_size = 0;

void SYSSend(unsigned char c);

/* ========================================================================== */
/* DLV11-J routines                                                           */
/* ========================================================================== */

static void DLV11JReadChannel3(void)
{
	DLV11J* dlv = &lsi.dlv11;

	if(buf_size > 0) {
		dlv->rbuf3 = (u8) buf[buf_r++];
		buf_r %= DLV11J_BUF;
		buf_size--;

		if(buf_size) {
			/* more date in the RX buffer... */
			dlv->rcsr3 |= RCSR_RCVR_DONE;
			if(dlv->rcsr3 & RCSR_RCVR_INT) {
				IRQ(060);
			}
		} else {
			dlv->rcsr3 &= ~RCSR_RCVR_DONE;
		}
	} else {
		dlv->rbuf3 = RBUF_OVERRUN;
		if(dlv->rbuf3 & RBUF_ERROR_MASK) {
			dlv->rbuf3 |= RBUF_ERROR;
		}
	}
}

u16 DLV11JRead(u16 address)
{
	DLV11J* dlv = &lsi.dlv11;
	switch(address) {
		/* Channel 3 */
		case 0177560:
			return dlv->rcsr3;
		case 0177562:
			DLV11JReadChannel3();
			return dlv->rbuf3;
		case 0177564:
			return dlv->xcsr3;
		case 0177566:
			return 0;
		default:
			return 0;
	}
}

static void DLV11JWriteRCSR3(u16 value)
{
	DLV11J* dlv = &lsi.dlv11;
	u16 old = dlv->rcsr3;
	dlv->rcsr3 = (dlv->rcsr3 & ~RCSR_WR_MASK) | (value & RCSR_WR_MASK);
	if((value & RCSR_RCVR_INT) && !(old & RCSR_RCVR_INT)
			&& (dlv->rcsr3 & RCSR_RCVR_DONE)) {
		IRQ(060);
	}
}

static void DLV11JWriteXCSR3(u16 value)
{
	DLV11J* dlv = &lsi.dlv11;
	u16 old = dlv->xcsr3;
	dlv->xcsr3 = (dlv->xcsr3 & ~XCSR_WR_MASK) | (value & XCSR_WR_MASK);
	if((value & XCSR_TRANSMIT_INT) && !(old & XCSR_TRANSMIT_INT)
			&& (dlv->xcsr3 & XCSR_TRANSMIT_READY)) {
		IRQ(064);
	}
}

static void DLV11JWriteXBUF3(u16 value)
{
	DLV11J* dlv = &lsi.dlv11;

	LSI11CancelInterrupt(064);

	SYSSend((unsigned char) value);

	dlv->xcsr3 |= XCSR_TRANSMIT_READY;
	if(dlv->xcsr3 & XCSR_TRANSMIT_INT) {
		IRQ(064);
	}
}

void DLV11JWrite(u16 address, u16 value)
{
	switch(address) {
		/* Channel 3 */
		case 0177560:
			DLV11JWriteRCSR3(value);
			break;
		case 0177562:
			/* ignored */
			break;
		case 0177564:
			DLV11JWriteXCSR3(value);
			break;
		case 0177566:
			DLV11JWriteXBUF3(value);
			break;
	}
}

void DLV11JReset(void)
{
	DLV11J* dlv = &lsi.dlv11;
	dlv->rcsr3 = 0;
	dlv->xcsr3 = XCSR_TRANSMIT_READY;
	dlv->rbuf3 = 0;

	dlv->irq = 0;
}

void DLV11JInit(void)
{
	DLV11J* dlv = &lsi.dlv11;

	dlv->rcsr3 = 0;
	dlv->xcsr3 = 0;
	dlv->rbuf3 = 0;
	dlv->xbuf3 = 0;

	buf_r = 0;
	buf_w = 0;
	buf_size = 0;

	DLV11JReset();
}

void DLV11JStep(void)
{
	DLV11J* dlv = &lsi.dlv11;

	/* re-try IRQ dispatch that was blocked previously */
	if(dlv->irq) {
		if(LSI11Interrupt(dlv->irq)) {
			dlv->irq = 0;
			return;
		}
	}
}

void SYSReceive(unsigned char c)
{
	DLV11J* dlv = &lsi.dlv11;

	/* ignore XON/XOFF */
	if(c == DC1 || c == DC3) {
		return;
	}

	if(buf_size < DLV11J_BUF) {
		buf[buf_w++] = c;
		buf_w %= DLV11J_BUF;
		buf_size++;
		dlv->rcsr3 |= RCSR_RCVR_DONE;
		if(dlv->rcsr3 & RCSR_RCVR_INT) {
			IRQ(060);
		}
	}
}
