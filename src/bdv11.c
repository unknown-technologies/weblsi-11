#include <GL/glut.h>
#include "types.h"
#include "lsi11.h"

#define	_A(x)		(1 << ((x) - 1))
#define	_B(x)		(1 << ((x) + 7))

#define	BDV11_CPU_TEST	_A(1)
#define	BDV11_MEM_TEST	_A(2)
#define	BDV11_DECNET	_A(3)
#define	BDV11_DIALOG	_A(4)
#define	BDV11_LOOP	_B(1)
#define	BDV11_RK05	_A(8)
#define	BDV11_RL01	_A(7)
#define	BDV11_RX01	_A(6)
#define	BDV11_RX02	(_A(6) | _A(7))
#define	BDV11_ROM	_A(5)

#define	BDV11_EXT_DIAG	_B(2)
#define	BDV11_2780	_B(3)
#define	BDV11_PROG_ROM	_B(4)

#if 0
#define	BDV11_SWITCH	(BDV11_CPU_TEST | BDV11_MEM_TEST \
		| BDV11_DIALOG | BDV11_RX02)
#endif

#ifdef BOOT_NODLG
#define	DLG		0
#else
#define	DLG		BDV11_DIALOG
#endif

#ifdef BOOT_RX02
#define	BOOTDEV		BDV11_RX02
#elif defined(BOOT_RL01) || defined(BOOT_RL02)
#define	BOOTDEV		BDV11_RL01
#else
#error "Unknown boot device"
#endif

#define	BDV11_SWITCH	(BDV11_CPU_TEST | DLG | BOOTDEV)

#define	TIMER_INTERVAL	(1000 / 50)

extern const u16 bdv11_e53[2048];

static volatile int timer_irq = 0;
static unsigned long last_time;
static unsigned long time;

/* ========================================================================== */
/* 50Hz timer                                                                 */
/* ========================================================================== */

static void BDV11InitTimer(void)
{
	last_time = glutGet(GLUT_ELAPSED_TIME);
	time = 0;
}

static void BDV11Tick(void)
{
	unsigned long now = glutGet(GLUT_ELAPSED_TIME);
	unsigned long dt = now - last_time;
	time += dt;
	last_time = now;
	if(time >= TIMER_INTERVAL) {
		time %= TIMER_INTERVAL;
		timer_irq = 1;
		LSIIRQ();
	}
}


/* ========================================================================== */
/* BDV11 Routines                                                             */
/* ========================================================================== */

static inline void BDV11SetLEDs(void)
{
	BDV11* bdv = &lsi.bdv11;
	u16 leds = bdv->display & 0x0F;
	(void) leds; // TODO: implement
}

static inline u16 BDV11GetWordLow(u16 word)
{
	BDV11* bdv = &lsi.bdv11;
	u16 page = bdv->pcr & 0xFF;
	if(page < 0x10) {
		u16 romword = page * 0200 + word;
		return bdv11_e53[romword];
	} else {
		return 0177777;
	}
}

static inline u16 BDV11GetWordHigh(u16 word)
{
	BDV11* bdv = &lsi.bdv11;
	u16 page = (bdv->pcr >> 8) & 0xFF;
	if(page < 0x10) {
		u16 romword = page * 0200 + word;
		return bdv11_e53[romword];
	} else {
		return 0233;
	}
}

u16 BDV11Read(u16 address)
{
	BDV11* self = &lsi.bdv11;
	switch(address) {
		case 0177520:
			return self->pcr;
		case 0177522:
			return self->scratch;
		case 0177524:
			return self->switches;
		case 0177546:
			return self->ltc;
		default:
			if(address >= 0173000 && address < 0173400) {
				return BDV11GetWordLow((address - 0173000) / 2);
			} else if(address >= 0173400 && address < 0173776) {
				return BDV11GetWordHigh((address - 0173400) / 2);
			}
			return 0;
	}
}

void BDV11Write(u16 address, u16 value)
{
	BDV11* self = &lsi.bdv11;
	switch(address) {
		case 0177520:
			self->pcr = value;
			break;
		case 0177522:
			self->scratch = value;
			break;
		case 0177524:
			self->display = value;
			BDV11SetLEDs();
			break;
		case 0177546:
			self->ltc = value & 0100;
			break;
	}
}
void BDV11Reset(void)
{
	lsi.bdv11.pcr = 0;
	lsi.bdv11.scratch = 0;
	lsi.bdv11.display = 0;
	timer_irq = 0;
}

void BDV11Init(void)
{
	lsi.bdv11.ltc = 0;
	lsi.bdv11.switches = BDV11_SWITCH;

	BDV11InitTimer();
	BDV11Reset();
}

void BDV11Step(void)
{
	BDV11* bdv = &lsi.bdv11;

	BDV11Tick();

	if((bdv->ltc & 0100) && timer_irq) {
		if(LSI11Interrupt(0100))
			timer_irq = 0;
	}
}

void BDV11SetSwitches(u16 switches)
{
	lsi.bdv11.switches = switches;
}
