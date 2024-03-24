#include <string.h>
#include "types.h"
#include "lsi11.h"

#define	RL_VECTOR		0160

#define	RL_DRDY			_BV(0)
#define	RL_FUNCTION_MASK	(_BV(1) | _BV(2) | _BV(3))
#define	RL_BA16			_BV(4)
#define	RL_BA17			_BV(5)
#define	RL_IE			_BV(6)
#define	RL_CRDY			_BV(7)
#define	RL_DS0			_BV(8)
#define	RL_DS1			_BV(9)
#define	RL_E0			_BV(10)
#define	RL_E1			_BV(11)
#define	RL_E2			_BV(12)
#define	RL_E3			_BV(13)
#define	RL_DE			_BV(14)
#define	RL_ERR			_BV(15)

#define	RL_CSR_ERRMASK		(RL_E0 | RL_E1 | RL_E2 | RL_E3)

#define	RL_CSR_DEFAULT		(RL_DRDY | RL_CRDY)

#define	RL_CSR_WMASK		(RL_FUNCTION_MASK | RL_BA16 | RL_BA17 \
		| RL_IE | RL_CRDY | RL_DS0 | RL_DS1)

#define	RL_CSR_BAE_MASK		(RL_BA16 | RL_BA17)

#define	RL_CSR_UNIT_MASK	(RL_DS0 | RL_DS1)
#define	RL_CSR_UNIT(csr)	(((csr) & RL_CSR_UNIT_MASK) >> 8)

#define	RL_DAR_SEEK		_BV(1)
#define	RL_DAR_SEEK_DIR		_BV(2)
#define	RL_DAR_SEEK_HS		_BV(4)
#define	RL_DAR_SEEK_DF_MASK	(_BV(7) | _BV(8) | _BV(9) | _BV(10) \
		| _BV(11) | _BV(12) | _BV(13) | _BV(14) | _BV(15))

#define	RL_DAR_RW_SA_MASK	(_BV(0) | _BV(1) | _BV(2) | _BV(3) \
		| _BV(4) | _BV(5))
#define	RL_DAR_RW_HS		_BV(6)
#define	RL_DAR_RW_CA_MASK	(_BV(7) | _BV(8) | _BV(9) | _BV(10) \
		| _BV(11) | _BV(12) | _BV(13) | _BV(14) | _BV(15))

#define	RL_DAR_GS_GS		_BV(1)
#define	RL_DAR_GS_RST		_BV(3)

#define	RL_MPR_GS_STA		_BV(0)
#define	RL_MPR_GS_STB		_BV(1)
#define	RL_MPR_GS_STC		_BV(2)
#define	RL_MPR_GS_BH		_BV(3)
#define	RL_MPR_GS_HO		_BV(4)
#define	RL_MPR_GS_CO		_BV(5)
#define	RL_MPR_GS_HS		_BV(6)
#define	RL_MPR_GS_DT		_BV(7)
#define	RL_MPR_GS_DSE		_BV(8)
#define	RL_MPR_GS_VC		_BV(9)
#define	RL_MPR_GS_WGE		_BV(10)
#define	RL_MPR_GS_SPE		_BV(11)
#define	RL_MPR_GS_SKTO		_BV(12)
#define	RL_MPR_GS_WL		_BV(13)
#define	RL_MPR_GS_CHE		_BV(14)
#define	RL_MPR_GS_WDE		_BV(15)

#define	RL_MPR_GS_LOAD_CART	00
#define	RL_MPR_GS_SPIN_UP	01
#define	RL_MPR_GS_BRUSH_CYCLE	02
#define	RL_MPR_GS_LOAD_HEADS	03
#define	RL_MPR_GS_SEEK		04
#define	RL_MPR_GS_LOCK_ON	05
#define	RL_MPR_GS_UNLOAD_HEADS	06
#define	RL_MPR_GS_SPIN_DOWN	07

#define	RL_MPR_RW_WCMASK	037777
#define	RL_MPR_RW_WC_BITS	0140000

#define	RL_BAE_MASK		000077

#define	RL_MAINTENANCE		(00 << 1)
#define	RL_WRITE_CHECK		(01 << 1)
#define	RL_GET_STATUS		(02 << 1)
#define	RL_SEEK			(03 << 1)
#define	RL_READ_HEADER		(04 << 1)
#define	RL_WRITE_DATA		(05 << 1)
#define	RL_READ_DATA		(06 << 1)
#define	RL_READ_DATA_NOCHECK	(07 << 1)

#define	RL_ERR_OPI		(01 << 10)
#define	RL_ERR_DCRC		(02 << 10)
#define	RL_ERR_WCE		(02 << 10)
#define	RL_ERR_HCRC		(03 << 10)
#define	RL_ERR_DLT		(04 << 10)
#define	RL_ERR_HNF		(05 << 10)
#define	RL_ERR_NXM		(10 << 10)
#define	RL_ERR_MPE		(11 << 10)

#define	RL_STATE_GS_HDR		1
#define	RL_STATE_GS_ZERO	2
#define	RL_STATE_GS_CRC		3

#define	SET_ERROR(x)		\
		rl->csr = (rl->csr & ~RL_CSR_ERRMASK) | RL_ERR | x

#define	READ(addr)		(LSI11ReadDMA((addr), &rl->nxm))
#define	WRITE(addr, val)	(rl->nxm = LSI11WriteDMA((addr), (val)))
#define	CHECK(x)		{ \
	if(rl->nxm) { \
		rl->nxm = 0; \
		x; \
		SET_ERROR(RL_ERR_NXM); \
		RLV12Done(); \
		return; \
	} \
}

#define	IRQ(x)			if(!LSI11Interrupt(x)) rl->irq = (x)

#define	OFFSET() (ca * (2 * 40 * 128) + (hs ? (40 * 128) : 0) + sa * 128)

#define	DISK_SIZE		2 * 40 * 512 * 256

static int opi_timer = 0;

u8* disk_ram;

static inline u16 RLV12CRC16(u16 crc, u16 c, u16 mask)
{
	u8 i;
	for(i = 0; i < 8; i++) {
		if((crc ^ c) & 1) {
			crc = (crc >> 1) ^ mask;
		} else {
			crc >>= 1;
		}
		c>>=1;
	}
	return crc;
}

static inline u16 RLV12CRC(u16 crc, u16 c)
{
	u16 tmp = RLV12CRC16(crc, (u8) c, 0xA001);
	return RLV12CRC16(tmp, (u8) (c >> 8), 0xA001);
}

static inline void RLV12ReadSector(u16 ca, u16 hs, u16 sa, u16 unit)
{
	if(sa >= 40) {
		sa = 0;
	}

	u32 offset = OFFSET();
	switch(unit) {
		case 0:
			if(disk_ram) {
				memcpy(lsi.rlv12.dma, &disk_ram[offset << 1], 256);
			} else {
				memset(lsi.rlv12.dma, 0, 256);
			}
			break;
		default:
			memset(lsi.rlv12.dma, 0, 256);
			break;
	}
}

static inline void RLV12WriteSector(u16 ca, u16 hs, u16 sa, u16 unit)
{
	if(sa >= 40) {
		sa = 0;
	}

	u32 offset = OFFSET();
	switch(unit) {
		case 0:
			if(disk_ram) {
				memcpy(&disk_ram[offset << 1], lsi.rlv12.dma, 256);
			}
			break;
		default:
			/* nothing, as if we're writing into the void */
			break;
	}
}

void RLV12Done(void)
{
	RLV12* rl = &lsi.rlv12;

	rl->state = 0;
	rl->csr |= RL_CRDY;
	if(rl->csr & RL_IE) {
		IRQ(0160);
	}
}

void RLV12Maintenance(void)
{
	RLV12* rl = &lsi.rlv12;
	u16 i;

	/* clear controller errors except DE */
	rl->csr &= ~(RL_CSR_ERRMASK | RL_ERR);

	/* internal diagnostic #1 */
	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	/* internal diagnostic #2 */
	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	/* DMA transfer to FIFO */
	for(i = 0; i < 256; i++) {
		rl->dma[i] = READ(rl->bar);
		CHECK();
		rl->bar += 2;
		rl->wc++;
	}

	/* DMA transfer back from FIFO */
	i = 0;
	for(i = 0; i < 255; i++) {
		WRITE(rl->bar, rl->dma[i]);
		CHECK();
		rl->bar += 2;
		rl->wc++;
	}

	/* internal diagnostic #3 */
	if(rl->wc) {
		opi_timer = 500; /* 186 => 74ms reported */
		/* SET_ERROR(RL_ERR_HNF); */
		/* RLV12Done(); */
		return;
	}

	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	/* CRC computation */
	rl->fifo[0] = RLV12CRC(0, rl->dar);

	/* internal diagnostic #4 */
	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	i = RLV12CRC(0, rl->dar);

	/* internal diagnostic #5 */
	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	rl->fifo[1] = RLV12CRC(0, i);

	/* internal diagnostic #6 */
	rl->dar = (rl->dar & 0xFF00) | ((rl->dar + 1) & 0x00FF);

	rl->state = 0;

	RLV12Done();
}

void RLV12WriteCheck(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 ca = rl->dar >> 7;
	u16 hs = rl->dar & RL_DAR_RW_HS;
	u16 sa = rl->dar & RL_DAR_RW_SA_MASK;
	u16 wc = (rl->wc & RL_MPR_RW_WCMASK) | RL_MPR_RW_WC_BITS;

	u16* data = rl->dma;

	u16 left = 128;

	u16 unit = RL_CSR_UNIT(rl->csr);
	if((ca != rl->ca[unit]) || (sa >= 40)) {
		/* wrong cylinder or invalid sector, report error */
		SET_ERROR(RL_ERR_HNF);
		RLV12Done();
		return;
	}

	RLV12ReadSector(ca, hs, sa, unit);
	while(wc++) {
		u16 cmp = READ(rl->bar);
		/* NXM? */
		CHECK(rl->wc = wc | 0xE000; rl->dar =
				(rl->dar & ~RL_DAR_RW_SA_MASK) | sa);

		rl->bar += 2;
		if(rl->bar == 0 || rl->bar == 1) { /* overflow */
			rl->bae = (rl->bae + 1) & RL_BAE_MASK;
			rl->csr = (rl->csr & ~RL_CSR_BAE_MASK)
				| ((rl->bae & 3) << 4);
		}

		if(*(data++) != cmp) {
			SET_ERROR(RL_ERR_DCRC);
			RLV12Done();
			rl->wc = wc;
			rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK) | sa;
			rl->sa[unit] = sa;
			rl->hs[unit] = !!hs;
			return;
		}

		/* increment SA at end of sector */
		if(!--left) {
			sa++;
			left = 128;
			if(sa >= 40 && wc) {
				sa &= RL_DAR_RW_SA_MASK;
				rl->hs[unit] = !!hs;
				rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK)
					| sa;
				SET_ERROR(RL_ERR_HNF);
				RLV12Done();
				return;
			} else {
				RLV12ReadSector(ca, hs, sa, unit);
				data = rl->dma;
			}
		}
	}

	/* increment SA unless we did that already */
	if(left != 128)
		sa++;

	/* write back data */
	rl->wc = 0;
	rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK) | (sa & RL_DAR_RW_SA_MASK);
	rl->sa[unit] = sa;
	rl->hs[unit] = !!hs;

	RLV12Done();
}

void RLV12GetStatus(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 unit = RL_CSR_UNIT(rl->csr);
	if(!(rl->dar & RL_DAR_GS_GS)) {
		SET_ERROR(RL_ERR_OPI);
		RLV12Done();
		return;
	}

	u16 status = RL_MPR_GS_LOCK_ON | RL_MPR_GS_DT | RL_MPR_GS_BH
		| RL_MPR_GS_HO;
	if(rl->hs[unit])  {
		status |= RL_MPR_GS_HS;
	}

	if(rl->dar & RL_DAR_GS_RST) {
		rl->csr &= ~(RL_CSR_ERRMASK | RL_ERR);
	}

	rl->fifo[0] = status;
	rl->fifo[1] = 0;
	rl->fifo[2] = 0;
	rl->fifo[3] = 0;
	rl->fifo[4] = 0;
	rl->fifo[5] = 0;
	rl->fifo[6] = 0;
	rl->fifo[7] = 0;

	RLV12Done();
}

void RLV12ReadHeader(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 unit = RL_CSR_UNIT(rl->csr);
	u16 sa = rl->sa[unit];
	if(sa >= 40)
		sa = 0;

	rl->fifo[0] = rl->sa[unit] | (rl->hs[unit] << 6) | (rl->ca[unit] << 7);
	rl->fifo[1] = 0;
	rl->fifo[2] = RLV12CRC(RLV12CRC(0, rl->fifo[0]), 0);
	rl->fifo[3] = 0;
	rl->fifo[4] = 0;
	rl->fifo[5] = 0;
	rl->fifo[6] = 0;
	rl->fifo[7] = 0;

	/* increment SA */
	sa = (sa + 1) % 40;
	rl->sa[unit] = sa;

	RLV12Done();
}

void RLV12Seek(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 unit = RL_CSR_UNIT(rl->csr);
	u16 df = rl->dar >> 7;
	s32 ca = rl->ca[unit];

	/* update CA */
	if(rl->dar & RL_DAR_SEEK_DIR)
		ca += df;
	else
		ca -= df;

	/* clamp to 0-511 */
	if(ca >= 512)
		ca = 512;
	else if(ca < 0)
		ca = 0;

	rl->ca[unit] = ca;

	rl->hs[unit] = !!(rl->dar & RL_DAR_SEEK_HS);

	RLV12Done();
}

void RLV12WriteData(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 ca = rl->dar >> 7;
	u16 hs = rl->dar & RL_DAR_RW_HS;
	u16 sa = rl->dar & RL_DAR_RW_SA_MASK;
	u16 wc = (rl->wc & RL_MPR_RW_WCMASK) | RL_MPR_RW_WC_BITS;

	u16* data = rl->dma;

	u16 left = 128;

	u16 unit = RL_CSR_UNIT(rl->csr);
	if((ca != rl->ca[unit]) || (sa >= 40)) {
		/* wrong cylinder or invalid sector, report error */
		SET_ERROR(RL_ERR_HNF);
		RLV12Done();
		return;
	}

	memset(rl->dma, 0, 128 * sizeof(u16));
	while(wc++) {
		*(data++) = READ(rl->bar);
		rl->bar += 2;
		if(rl->bar == 0 || rl->bar == 1) { /* overflow */
			rl->bae = (rl->bae + 1) & RL_BAE_MASK;
			rl->csr = (rl->csr & ~RL_CSR_BAE_MASK)
				| ((rl->bae & 3) << 4);
		}

		/* NXM? */
		CHECK(rl->wc = wc | 0xE000; rl->dar =
				(rl->dar & ~RL_DAR_RW_SA_MASK) | sa);

		/* increment SA at end of sector */
		if(!--left) {
			RLV12WriteSector(ca, hs, sa, unit);

			sa++;
			left = 128;
			if(sa >= 40 && wc) {
				sa &= RL_DAR_RW_SA_MASK;
				rl->hs[unit] = !!hs;
				rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK)
					| sa;
				SET_ERROR(RL_ERR_HNF);
				RLV12Done();
				return;
			}

			memset(rl->dma, 0, 128 * sizeof(u16));
			data = rl->dma;
		}
	}

	/* fill sector and increment SA unless we did that already */
	if(left != 128) {
		/* fill the rest of the sector with zeros */
		RLV12WriteSector(ca, hs, sa, unit);
		sa++;
	}

	/* write back data */
	rl->wc = 0;
	rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK) | (sa & RL_DAR_RW_SA_MASK);
	rl->sa[unit] = sa;
	rl->hs[unit] = !!hs;

	RLV12Done();
}

void RLV12ReadData(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 ca = (rl->dar & RL_DAR_RW_CA_MASK) >> 7;
	u16 hs = rl->dar & RL_DAR_RW_HS;
	u16 sa = rl->dar & RL_DAR_RW_SA_MASK;
	u16 wc = (rl->wc & RL_MPR_RW_WCMASK) | RL_MPR_RW_WC_BITS;

	u16* data = rl->dma;

	u16 left = 128;

	u16 unit = RL_CSR_UNIT(rl->csr);
	if((ca != rl->ca[unit]) || (sa >= 40)) {
		/* wrong cylinder or invalid sector, report error */
		SET_ERROR(RL_ERR_HNF);
		RLV12Done();
		return;
	}

	RLV12ReadSector(ca, hs, sa, unit);
	while(wc++) {
		WRITE(rl->bar, *(data++));
		rl->bar += 2;
		if(rl->bar == 0 || rl->bar == 1) { /* overflow */
			rl->bae = (rl->bae + 1) & RL_BAE_MASK;
			rl->csr = (rl->csr & ~RL_CSR_BAE_MASK)
				| ((rl->bae & 3) << 4);
		}

		/* NXM? */
		CHECK(rl->wc = wc | 0xE000; rl->dar =
				(rl->dar & ~RL_DAR_RW_SA_MASK) | sa);

		/* increment SA at end of sector */
		if(!--left) {
			sa++;
			left = 128;
			if(sa >= 40 && wc) {
				sa &= RL_DAR_RW_SA_MASK;
				rl->hs[unit] = !!hs;
				rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK)
					| sa;
				SET_ERROR(RL_ERR_HNF);
				RLV12Done();
				return;
			} else {
				RLV12ReadSector(ca, hs, sa, unit);
				data = rl->dma;
			}
		}
	}

	/* increment SA unless we did that already */
	if(left != 128)
		sa++;

	/* write back data */
	rl->wc = 0;
	rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK) | (sa & RL_DAR_RW_SA_MASK);
	rl->sa[unit] = sa;
	rl->hs[unit] = !!hs;

	RLV12Done();
}

void RLV12ReadDataNoHeaderCheck(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 unit = RL_CSR_UNIT(rl->csr);
	u16 ca = rl->ca[unit];
	u16 hs = rl->dar & RL_DAR_RW_HS;
	u16 sa = rl->dar & RL_DAR_RW_SA_MASK;
	u16 wc = (rl->wc & RL_MPR_RW_WCMASK) | RL_MPR_RW_WC_BITS;

	u16* data = rl->dma;

	u16 left = 128;

	RLV12ReadSector(ca, hs, sa, unit);
	while(wc++) {
		WRITE(rl->bar, *(data++));
		rl->bar += 2;
		if(rl->bar == 0 || rl->bar == 1) { /* overflow */
			rl->bae = (rl->bae + 1) & RL_BAE_MASK;
			rl->csr = (rl->csr & ~RL_CSR_BAE_MASK)
				| ((rl->bae & 3) << 4);
		}

		/* NXM? */
		CHECK(rl->wc = wc | 0xE000; rl->dar =
				(rl->dar & ~RL_DAR_RW_SA_MASK)
				| (sa & RL_DAR_RW_SA_MASK));

		/* increment SA at end of sector */
		if(!--left) {
			sa++;
			left = 128;
			if(sa >= 40 && wc) {
				sa &= RL_DAR_RW_SA_MASK;
			}
			RLV12ReadSector(ca, hs, sa, unit);
			data = rl->dma;
		}
	}

	/* increment SA unless we did that already */
	if(left != 128)
		sa++;

	/* write back data */
	rl->wc = 0;
	rl->dar = (rl->dar & ~RL_DAR_RW_SA_MASK) | (sa & RL_DAR_RW_SA_MASK);
	rl->sa[unit] = sa;
	rl->hs[unit] = !!hs;

	RLV12Done();
}

u16 RLV12GetMPR(void)
{
	RLV12* rl = &lsi.rlv12;

	u16 i = rl->state;
	rl->state = (rl->state + 1) & 07;

	return rl->fifo[i];
}

void RLV12ExecuteCommand(void)
{
	RLV12* rl = &lsi.rlv12;
	u16 cmd = rl->csr & RL_FUNCTION_MASK;

	/* clear errors */
	rl->csr &= ~(RL_CSR_ERRMASK | RL_ERR);

	switch(cmd) {
		case RL_MAINTENANCE:
			RLV12Maintenance();
			break;
		case RL_WRITE_CHECK:
			RLV12WriteCheck();
			break;
		case RL_GET_STATUS:
			RLV12GetStatus();
			break;
		case RL_SEEK:
			RLV12Seek();
			break;
		case RL_READ_HEADER:
			RLV12ReadHeader();
			break;
		case RL_WRITE_DATA:
			RLV12WriteData();
			break;
		case RL_READ_DATA:
			RLV12ReadData();
			break;
		case RL_READ_DATA_NOCHECK:
			RLV12ReadDataNoHeaderCheck();
			break;
		default:
			/* unreachable */
			RLV12Done();
			break;
	}
}

u16 RLV12Read(u16 address)
{
	RLV12* rl = &lsi.rlv12;

	switch(address) {
		case 0174400:
			return rl->csr | RL_DRDY;
		case 0174402:
			return rl->bar;
		case 0174404:
			return rl->dar;
		case 0174406:
			return RLV12GetMPR();
		case 0174410:
			return rl->bae;
	}

	return 0;
}

void RLV12Write(u16 address, u16 value)
{
	RLV12* rl = &lsi.rlv12;

	switch(address) {
		case 0174400:
			rl->csr = (rl->csr & ~RL_CSR_WMASK)
				| (value & RL_CSR_WMASK);
			rl->bae = (rl->bae & ~3)
				| ((value >> 4) & 3);
			if(~rl->csr & RL_CRDY) {
				RLV12ExecuteCommand();
			}
			break;
		case 0174402:
			rl->bar = value;
			break;
		case 0174404:
			rl->dar = value;
			break;
		case 0174406:
			rl->wc = value;
			break;
		case 0174410:
			rl->bae = value & RL_BAE_MASK;
			rl->csr = (rl->csr & ~RL_CSR_BAE_MASK)
				| ((value & 3) << 4);
			break;
	}
}

void RLV12Init(void)
{
	RLV12Reset();
}

void RLV12Reset(void)
{
	RLV12* rl = &lsi.rlv12;

	rl->csr = RL_CSR_DEFAULT;
	rl->bar = 0;
	rl->dar = 0;
	rl->bae = 0;
	rl->wc = 0;

	memset(rl->sa, 0, sizeof(rl->sa));
	memset(rl->hs, 0, sizeof(rl->hs));
	memset(rl->ca, 0, sizeof(rl->ca));

	opi_timer = 0;
	rl->irq = 0;
}

void RLV12Step(void)
{
	RLV12* rl = &lsi.rlv12;
	if(rl->irq) {
		if(LSI11Interrupt(rl->irq))
			rl->irq = 0;
	} else if(opi_timer) {
		opi_timer--;
		if(!opi_timer) {
			SET_ERROR(RL_ERR_HNF);
			RLV12Done();
		}
	}
}
