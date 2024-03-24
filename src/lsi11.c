#include <string.h>

#include "types.h"
#include "lsi11.h"

LSI11 lsi;

volatile int lsi_irq = 0;

int LSI11Interrupt(int n)
{
	if(n != 004) {
		LSIIRQ();
		if(lsi.bus.trap || lsi.bus.irq) {
			return 0;
		} else {
			lsi.bus.irq = n;
			lsi.bus.delay = 0;
			return 1;
		}
	} else {
		lsi.bus.trap = n;
		return 1;
	}
}

static unsigned int rng_state = 0;

static inline unsigned int rng(void)
{
	rng_state = (rng_state * 1103515245) + 12345;
	return rng_state;
}

static void LSI11QBUSStep()
{
	QBUS* bus = &lsi.bus;
	if(bus->delay >= QBUS_DELAY) {
		/* wait until last trap was serviced */
		if(!bus->trap) {
			bus->trap = bus->irq;
			bus->irq = 0;
			bus->delay = rng() % QBUS_DELAY_JITTER;
		}
	} else if(bus->irq) {
		bus->delay++;
	}
}

void LSI11Init(void)
{
	memset(&lsi, 0, sizeof(LSI11));

	KD11Init();
	lsi.bus.trap = 0;
	lsi.bus.nxm = 0;

	MSV11DInit();
	DLV11JInit();
	BDV11Init();
	RXV21Init();
	RLV12Init();
}

void QBUSReset(void)
{
	/* clear pending interrupts */
	lsi_irq = 0;
	lsi.bus.trap = 0;
	lsi.bus.irq = 0;
	lsi.bus.delay = 0;

	DLV11JReset();
	BDV11Reset();
	RXV21Reset();
	RLV12Reset();
}

void LSI11Reset(void)
{
	KD11Reset();

	QBUSReset();
}

static volatile int lsi_restart = 0;
void LSI11Restart(void)
{
	lsi_restart = 1;
	LSIIRQ();
}

/* TODO: tune this properly */
#define STEPS	7000

void LSI11Step(void)
{
	/* check for reset */
	if(lsi_restart) {
		lsi_restart = 0;

		KD11Init();
		KD11Reset();
		QBUSReset();
		lsi.kd11.state = 1;
	}

	for(unsigned int i = 0; i < STEPS; i++) {
		LSI11QBUSStep();
		KD11Step();

		if(lsi.kd11.state == 0) {
			i += STEPS / 4;
		}

		if(lsi_irq || lsi.kd11.state != 1) {
			lsi_irq = 0;

			DLV11JStep();
			RXV21Step();
			RLV12Step();
			BDV11Step();
		}
	}

	lsi_irq = 0;

	DLV11JStep();
	RXV21Step();
	RLV12Step();
	BDV11Step();
}
