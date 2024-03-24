#include <string.h>

#include "types.h"
#include "lsi11.h"

#define	MEM_SIZE	65536
#define	RAM_SIZE	(56 * 1024)

#define	DISK_SIZE	2 * 40 * 512 * 256

#ifdef RX2U0IMG
extern unsigned char RX2U0IMG[512512];
#endif

#ifdef RX2U1IMG
extern unsigned char RX2U1IMG[512512];
#endif

#ifdef RL2IMG
extern unsigned char RL2IMG[DISK_SIZE];
#endif

extern unsigned char fdd_ram[512512 * 2];

extern unsigned char* disk_ram;

void SYSProcess(unsigned long dt)
{
	(void) dt;
	LSI11Step();
}

void SYSBreak(void)
{
	lsi.kd11.state = 0;
}

void SYSInit(void)
{
#ifdef RL2IMG
	disk_ram = RL2IMG;
#else
	disk_ram = NULL;
#endif

	/* initialize LSI-11 */
	LSI11Init();
	LSI11Reset();

	memset(fdd_ram, 0, 512512 * 2);

#ifdef RX2U0IMG
	memcpy(fdd_ram, RX2U0IMG, 512512);
#endif

#ifdef RX2U1IMG
	memcpy(fdd_ram + 512512, RX2U1IMG, 512512);
#endif

	lsi.kd11.state = 1;
}
