#include <string.h>

#include "types.h"
#include "lsi11.h"

/* RX2CS bits */
#define	RX_GO			_BV(0)
#define	RX_FUNCTION_MASK	(_BV(1) | _BV(2) | _BV(3))
#define	RX_UNIT_SEL		_BV(4)
#define	RX_DONE			_BV(5)
#define	RX_INTR_ENB		_BV(6)
#define	RX_TR			_BV(7)
#define	RX_DEN			_BV(8)
#define	RX_HD_SEL		_BV(9)
#define	RX_RX02			_BV(11)
#define	RX_EXT_ADDR_MASK	(_BV(12) | _BV(13))
#define	RX_INIT			_BV(14)
#define	RX_ERROR		_BV(15)

/* NOTE: bit 9 of RX2CS can be read/written */

#define	RX_RMASK		(RX_UNIT_SEL | RX_DONE | RX_INTR_ENB \
		| RX_TR | RX_DEN | RX_RX02 | RX_ERROR | _BV(9))
#define	RX_WMASK		(RX_GO | RX_FUNCTION_MASK | RX_UNIT_SEL \
		| RX_INTR_ENB | RX_DEN | RX_EXT_ADDR_MASK | RX_INIT | _BV(9))

/* RX2CS function codes */
#define	RX_FILL_BUFFER		(00 << 1)
#define	RX_EMPTY_BUFFER		(01 << 1)
#define	RX_WRITE_SECTOR		(02 << 1)
#define	RX_READ_SECTOR		(03 << 1)
#define	RX_SET_MEDIA_DENSITY	(04 << 1)
#define	RX_READ_STATUS		(05 << 1)
#define	RX_WRITE_DELETED_DATA	(06 << 1)
#define	RX_READ_ERROR_CODE	(07 << 1)

/* RX2ES bits */
#define	RX2ES_CRC		_BV(0)
#define	RX2ES_SIDE1_RDY		_BV(1)
#define	RX2ES_ID		_BV(2)
#define	RX2ES_RX_AC_LO		_BV(3)
#define	RX2ES_DEN_ERR		_BV(4)
#define	RX2ES_DRV_DEN		_BV(5)
#define	RX2ES_DD		_BV(6)
#define	RX2ES_DRV_RDY		_BV(7)
#define	RX2ES_UNIT_SEL		_BV(8)
#define	RX2ES_HD_SEL		_BV(9)
#define	RX2ES_WC_OVFL		_BV(10)
#define	RX2ES_NXM		_BV(11)

#define	RX2ES_DEFAULT		(RX2ES_ID | RX2ES_DRV_DEN | RX2ES_DRV_RDY)
#define	RX2ES_ERRORS		(RX2ES_CRC | RX2ES_RX_AC_LO | RX2ES_DEN_ERR \
		| RX2ES_DD | RX2ES_WC_OVFL | RX2ES_NXM)

#define	READ(addr)		(LSI11ReadDMA((addr), &nxm))
#define	WRITE(addr, val)	(nxm |= LSI11WriteDMA((addr), (val)))

#define	IRQ(x)			{ if(!LSI11Interrupt(x)) rx->irq = (x); }

/* this is 512 bytes large */
static u32 del_marks_unit0[64];
static u32 del_marks_unit1[64];

u8 fdd_ram[512512 * 2];

void RXV21ClearErrors(void)
{
	RXV21* rx = &lsi.rxv21;

	rx->rx2cs &= ~RX_ERROR;
	rx->rx2es &= ~RX2ES_ERRORS;
}

void RXV21Done(void)
{
	RXV21* rx = &lsi.rxv21;

	rx->state = 0;
	rx->rx2cs |= RX_DONE;
	rx->rx2es |= RX2ES_DRV_DEN;
	rx->rx2db = rx->rx2es;

	if(rx->rx2es & RX2ES_ERRORS) {
		rx->rx2cs |= RX_ERROR;
	}

	if(rx->rx2cs & RX_INTR_ENB) {
		IRQ(0264);
	}
}

void RXV21FillBuffer(void)
{
	RXV21* rx = &lsi.rxv21;
	u16 limit = (rx->rx2cs & RX_DEN) ? 128 : 64;
	u16 wc;
	u16 ptr;

	if(rx->rx2wc > limit) {
		rx->error = 0230; /* Word count overflow */
		rx->rx2es |= RX2ES_WC_OVFL;
		rx->rx2cs |= RX_ERROR;
		RXV21Done();
		return;
	}

	/* memset(rx->buffer, 0, 256); */

	BOOL nxm = FALSE;
	for(wc = rx->rx2wc, ptr = 0; wc > 0; wc--, ptr++) {
		/* transfer words */
		u16 tmp = READ(rx->rx2ba);
		if(nxm) {
			rx->rx2es |= RX2ES_NXM;
			break;
		} else {
			rx->buffer[ptr] = tmp;
		}
		rx->rx2ba += 2;
	}

	RXV21Done();
}

void RXV21EmptyBuffer(void)
{
	RXV21* rx = &lsi.rxv21;
	u16 limit = (rx->rx2cs & RX_DEN) ? 128 : 64;
	u16 wc;
	u16 ptr;

	if(rx->rx2wc > limit) {
		rx->error = 0230; /* Word count overflow */
		rx->rx2es |= RX2ES_WC_OVFL;
		rx->rx2cs |= RX_ERROR;
		RXV21Done();
		return;
	}

	for(wc = rx->rx2wc, ptr = 0; wc > 0; wc--, ptr++) {
		/* transfer words */
		BOOL nxm = FALSE;
		WRITE(rx->rx2ba, rx->buffer[ptr]);
		if(nxm) {
			rx->rx2es |= RX2ES_NXM;
			break;
		}
		rx->rx2ba += 2;
	}

	RXV21Done();
}

void RXV21WriteSector(BOOL deleted)
{
	RXV21* rx = &lsi.rxv21;
	u32 offset;

	rx->rx2sa &= 0037;
	rx->rx2ta &= 0177;

	offset = (rx->rx2sa - 1) * 256 + rx->rx2ta * (26 * 256);

	if(!(rx->rx2cs & RX_DEN)) {
		rx->error = 0240; /* Density Error */
		rx->rx2cs |= RX_ERROR;
		rx->rx2es |= RX2ES_DEN_ERR;
		RXV21Done();
		return;
	}

	if(rx->rx2sa == 0 || rx->rx2sa > 26 || rx->rx2ta > 76) {
		if(rx->rx2ta > 76) {
			rx->error = 0040; /* Tried to access a track greater than 76 */
		} else {
			rx->error = 0070; /* Desired sector could not be found after looking at 52 headers (2 revolutions) */
		}
		rx->rx2cs |= RX_ERROR;
	} else {
		u32 base = (rx->rx2cs & RX_UNIT_SEL) ? SRAM_RX02_UNIT1 : \
				SRAM_RX02_UNIT0;
		memcpy(&fdd_ram[base + offset], rx->buffer, 256);
	}

	/* update DELETED DATA marker */
	u32* marks = (rx->rx2cs & RX_UNIT_SEL) ? del_marks_unit1 : \
		     del_marks_unit0;
	u32 index = offset / 256;
	u32 word = index / 32;
	u32 bit = 1L << (index % 32);
	if(deleted) {
		marks[word] |= bit;
	} else {
		marks[word] &= ~bit;
	}

	RXV21Done();
}

void RXV21ReadSector(void)
{
	RXV21* rx = &lsi.rxv21;
	u32 offset;

	rx->rx2sa &= 0037;
	rx->rx2ta &= 0177;

	offset = (rx->rx2sa - 1) * 256 + rx->rx2ta * (26 * 256);

	if(!(rx->rx2cs & RX_DEN)) {
		rx->error = 0240; /* Density Error */
		rx->rx2cs |= RX_ERROR;
		rx->rx2es |= RX2ES_DEN_ERR;
		RXV21Done();
		return;
	}

	if(rx->rx2sa == 0 || rx->rx2sa > 26 || rx->rx2ta > 76) {
		if(rx->rx2ta > 76) {
			rx->error = 0040; /* Tried to access a track greater than 76 */
		} else {
			rx->error = 0070; /* Desired sector could not be found after looking at 52 headers (2 revolutions) */
		}
		rx->rx2cs |= RX_ERROR;
	} else {
		u32 base = (rx->rx2cs & RX_UNIT_SEL) ? SRAM_RX02_UNIT1 : \
				SRAM_RX02_UNIT0;
		memcpy(rx->buffer, &fdd_ram[base + offset], 256);
	}

	/* read DELETED DATA marker */
	u32* marks = (rx->rx2cs & RX_UNIT_SEL) ? del_marks_unit1 : \
		     del_marks_unit0;
	u32 index = offset / 256;
	u32 word = index / 32;
	u32 bit = 1L << (index % 32);
	if(marks[word] & bit) {
		rx->rx2es |= RX2ES_DD;
	}

	RXV21Done();
}

void RXV21ReadStatus(void)
{
	RXV21* rx = &lsi.rxv21;

	rx->rx2es |= RX2ES_DRV_RDY | RX2ES_DRV_DEN;
	if(rx->rx2cs & RX_UNIT_SEL)
		rx->rx2es |= RX2ES_UNIT_SEL;
	else
		rx->rx2es &= ~RX2ES_UNIT_SEL;

	RXV21Done();
}

void RXV21SetMediaDensity(void)
{
	RXV21* rx = &lsi.rxv21;

	u32* marks = (rx->rx2cs & RX_UNIT_SEL) ? del_marks_unit1 : \
		     del_marks_unit0;
	memset(marks, 0, 64 * sizeof(u32));

	u32 base = (rx->rx2cs & RX_UNIT_SEL) ? SRAM_RX02_UNIT1 : SRAM_RX02_UNIT0;
	memset(&fdd_ram[base], 0, 512512);
	RXV21Done();
}

void RXV21ReadErrorCode(void)
{
	RXV21* rx = &lsi.rxv21;

	BOOL nxm = FALSE;

	/* < 7:0> Definitive Error Codes */
	/* <15:8> Word Count Register */
	WRITE(rx->rx2ba, rx->error | (rx->rx2wc << 8));

	/* < 7:0> Current Track Address of Drive 0 */
	/* <15:8> Current Track Address of Drive 1 */
	WRITE(rx->rx2ba + 2, rx->rx2ta | (rx->rx2ta << 8));

	/* < 7:0> Target Track of Current Disk Access */
	/* <15:8> Target Sector of Current Disk Access */
	WRITE(rx->rx2ba + 4, rx->rx2ta | (rx->rx2sa << 8));

	/* <7> Unit Select Bit */
	/* <5> Head Load Bit */
	/* <6> <4> Drive Density Bit of Both Drives */
	/* <0> Density of Read Error Register Command */
	/* <15:8> Track Address of Selected Drive */
	WRITE(rx->rx2ba + 6, ((rx->rx2cs & RX_UNIT_SEL) ? _BV(7) : 0) | _BV(5)
			| _BV(6) | _BV(4) | _BV(0) | (rx->rx2ta << 8));

	RXV21Done();
}

void RXV21ExecuteCommand(void)
{
	RXV21* rx = &lsi.rxv21;

	rx->rx2cs &= ~RX_GO;
	rx->state = 1;

	if(rx->rx2cs & RX_UNIT_SEL)
		rx->rx2es |= RX2ES_UNIT_SEL;
	else
		rx->rx2es &= ~RX2ES_UNIT_SEL;

	switch(rx->rx2cs & RX_FUNCTION_MASK) {
		case RX_FILL_BUFFER:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			break;
		case RX_EMPTY_BUFFER:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			break;
		case RX_WRITE_SECTOR:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			rx->rx2es = RX2ES_DEFAULT;
			if(rx->rx2cs & RX_UNIT_SEL)
				rx->rx2es |= RX2ES_UNIT_SEL;
			break;
		case RX_READ_SECTOR:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			rx->rx2es = RX2ES_DEFAULT;
			if(rx->rx2cs & RX_UNIT_SEL)
				rx->rx2es |= RX2ES_UNIT_SEL;
			break;
		case RX_SET_MEDIA_DENSITY:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			rx->rx2es = RX2ES_DEFAULT;
			if(rx->rx2cs & RX_UNIT_SEL)
				rx->rx2es |= RX2ES_UNIT_SEL;
			break;
		case RX_READ_STATUS:
			RXV21ReadStatus();
			break;
		case RX_WRITE_DELETED_DATA:
			RXV21ClearErrors();
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			rx->rx2es = RX2ES_DEFAULT;
			if(rx->rx2cs & RX_UNIT_SEL)
				rx->rx2es |= RX2ES_UNIT_SEL;
			break;
		case RX_READ_ERROR_CODE:
			rx->rx2cs &= ~RX_DONE;
			rx->rx2cs |= RX_TR;
			rx->rx2es = RX2ES_DEFAULT;
			break;
	}
}

void RXV21Process(void)
{
	RXV21* rx = &lsi.rxv21;

	if(rx->state == 0) {
		return;
	}

	switch(rx->rx2cs & RX_FUNCTION_MASK) {
		case RX_FILL_BUFFER:
			switch(rx->state) {
				case 1: /* read RX2WC */
					rx->rx2wc = rx->rx2db;
					rx->state++;
					break;
				case 2: /* read RX2BA */
					rx->rx2ba = rx->rx2db;
					rx->rx2cs &= ~RX_TR;
					RXV21FillBuffer();
					break;
			}
			break;
		case RX_EMPTY_BUFFER:
			switch(rx->state) {
				case 1: /* read RX2WC */
					rx->rx2wc = rx->rx2db;
					rx->state++;
					break;
				case 2: /* read RX2BA */
					rx->rx2ba = rx->rx2db;
					rx->rx2cs &= ~RX_TR;
					RXV21EmptyBuffer();
					break;
			}
			break;
		case RX_WRITE_SECTOR:
			switch(rx->state) {
				case 1: /* read RX2SA */
					rx->rx2sa = rx->rx2db;
					rx->state++;
					break;
				case 2: /* read RX2TA */
					rx->rx2ta = rx->rx2db;
					rx->rx2cs &= ~RX_TR;
					RXV21WriteSector(FALSE);
					break;
			}
			break;
		case RX_READ_SECTOR:
			switch(rx->state) {
				case 1: /* read RX2SA */
					rx->rx2sa = rx->rx2db;
					rx->state++;
					break;
				case 2: /* read RX2TA */
					rx->rx2ta = rx->rx2db;
					rx->rx2cs &= ~RX_TR;
					RXV21ReadSector();
					break;
			}
			break;
		case RX_SET_MEDIA_DENSITY:
			/* read 'I' */
			rx->rx2cs &= ~RX_TR;
			if(rx->rx2db == 'I') {
				RXV21SetMediaDensity();
			} else {
				rx->error = 0250; /* Key Word Error */
				rx->rx2cs |= RX_ERROR;
				RXV21Done();
			}
			break;
		case RX_WRITE_DELETED_DATA:
			switch(rx->state) {
				case 1: /* read RX2SA */
					rx->rx2sa = rx->rx2db;
					rx->state++;
					break;
				case 2: /* read RX2TA */
					rx->rx2ta = rx->rx2db;
					rx->rx2cs &= ~RX_TR;
					RXV21WriteSector(TRUE);
					break;
			}
			break;
		case RX_READ_ERROR_CODE:
			rx->rx2ba = rx->rx2db;
			rx->rx2cs &= ~RX_TR;
			RXV21ReadErrorCode();
			break;
	}
}

u16 RXV21Read(u16 address)
{
	RXV21* rx = &lsi.rxv21;
	if(address == 0177170) { /* RX2CS */
		return rx->rx2cs & RX_RMASK;
	} else if(address == 0177172) { /* RX2DB */
		return rx->rx2db;
	}

	return 0;
}

void RXV21Write(u16 address, u16 value)
{
	RXV21* rx = &lsi.rxv21;

	if(address == 0177170) { /* RX2CS */
		int intr = rx->rx2cs & RX_INTR_ENB;
		rx->rx2cs = (rx->rx2cs & ~RX_WMASK) | (value & RX_WMASK);
		rx->rx2cs &= ~(RX_TR | RX_INIT | RX_ERROR);
		rx->rx2db = value & 0367;
		rx->error = 0;
		if(value & RX_INIT) {
			RXV21Reset();
			return;
		}
		if(rx->rx2cs & RX_GO) {
			/* initiate command */
			RXV21ExecuteCommand();
		}
		if(!intr && (value & RX_INTR_ENB) && (rx->rx2cs & RX_DONE)) {
			IRQ(0264);
		}
	} else if(address == 0177172) { /* RX2DB */
		if(rx->rx2cs & RX_DONE) {
			/* no operation in progress */
			rx->rx2db = value & 0173767;
		} else {
			rx->rx2db = value;
		}
		RXV21Process();
	}
}

void RXV21Reset(void)
{
	RXV21* rx = &lsi.rxv21;

	rx->state = 0;
	rx->rx2cs = RX_RX02 | RX_DONE;
	rx->rx2es = RX2ES_DEFAULT;
	rx->rx2db = rx->rx2es;

	/* read sector 1 / track 1 of drive 0 into buffer */
	rx->rx2sa = 1;
	rx->rx2ta = 1;
	u32 offset = (rx->rx2sa - 1) * 256 + rx->rx2ta * (26 * 256);

	memcpy(rx->buffer, &fdd_ram[SRAM_RX02_UNIT0 + offset], 256);
}

void RXV21Init(void)
{
	/* clear DELETED DATA markers */
	memset(del_marks_unit0, 0, sizeof(del_marks_unit0));
	memset(del_marks_unit1, 0, sizeof(del_marks_unit1));

	RXV21Reset();
}

void RXV21Step(void)
{
	RXV21* rx = &lsi.rxv21;
	if(rx->irq) {
		if(LSI11Interrupt(rx->irq))
			rx->irq = 0;
	}
}
