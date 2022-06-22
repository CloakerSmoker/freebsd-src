#include "btxv86.h"
#include "litepxe.h"
#include "pxe.h"

#include <stdlib.h>
#include <stdio.h>

void	(*pxe_call)(int func, void *ptr);

static pxenv_t *pxenv_p = NULL;	/* PXENV+ */
static pxe_t *pxe_p = NULL;		/* !PXE */

#ifdef PXE_DEBUG
static int	pxe_debug = 0;
#endif

void		pxe_enable(void *pxeinfo);
void	pxe_perror(PXENV_STATUS_t);
void	(*pxe_call)(int func, void *ptr);
void	pxenv_call(int func, void *ptr);
void	bangpxe_call(int func, void *ptr);

int	pxe_init(void);
int	pxe_print(int verbose);
void	pxe_cleanup(void);

extern uint16_t			__bangpxeseg;
extern uint16_t			__bangpxeoff;
extern void			__bangpxeentry(void);
extern uint16_t			__pxenvseg;
extern uint16_t			__pxenvoff;
extern void			__pxenventry(void);

/*
 * This function is called by the loader to enable PXE support if we
 * are booted by PXE. The passed in pointer is a pointer to the PXENV+
 * structure.
 */
void
pxe_enable(void *pxeinfo)
{
	pxenv_p  = (pxenv_t *)pxeinfo;
	pxe_p    = (pxe_t *)PTOV(pxenv_p->PXEPtr.segment * 16 +
				 pxenv_p->PXEPtr.offset);
	pxe_call = NULL;
}

void
pxe_perror(PXENV_STATUS_t code)
{
	printf("PXE Error %x\n", code);
}

/*
 * return true if pxe structures are found/initialized,
 * also figures out our IP information via the pxe cached info struct
 */
int
pxe_init(void)
{
	//t_PXENV_GET_CACHED_INFO *gci_p;
	int counter;
	uint8_t checksum;
	uint8_t *checkptr;

	if (pxenv_p == NULL)
		return (0);
	
	printf("PXE sig %s\n", pxenv_p->Signature);

	/* look for "PXENV+" */
	if (bcmp((void *)pxenv_p->Signature, S_SIZE("PXENV+"))) {
		pxenv_p = NULL;
		return (0);
	}

	/* make sure the size is something we can handle */
	if (pxenv_p->Length > sizeof(*pxenv_p)) {
		printf("PXENV+ structure too large, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}

	/*
	 * do byte checksum:
	 * add up each byte in the structure, the total should be 0
	 */
	checksum = 0;
	checkptr = (uint8_t *) pxenv_p;
	for (counter = 0; counter < pxenv_p->Length; counter++)
		checksum += *checkptr++;
	if (checksum != 0) {
		printf("PXENV+ structure failed checksum, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}

	/*
	 * PXENV+ passed, so use that if !PXE is not available or
	 * the checksum fails.
	 */
	pxe_call = pxenv_call;
	if (pxenv_p->Version >= 0x0200) {
		for (;;) {
			if (bcmp((void *)pxe_p->Signature, S_SIZE("!PXE"))) {
				pxe_p = NULL;
				break;
			}
			checksum = 0;
			checkptr = (uint8_t *)pxe_p;
			for (counter = 0; counter < pxe_p->StructLength;
			    counter++)
				checksum += *checkptr++;
			if (checksum != 0) {
				pxe_p = NULL;
				break;
			}
			pxe_call = bangpxe_call;
			break;
		}
	}

	printf("\nPXE version %d.%d, real mode entry point ",
	    (uint8_t) (pxenv_p->Version >> 8),
	    (uint8_t) (pxenv_p->Version & 0xFF));
	if (pxe_call == bangpxe_call)
		printf("@%04x:%04x\n",
		    pxe_p->EntryPointSP.segment,
		    pxe_p->EntryPointSP.offset);
	else
		printf("@%04x:%04x\n",
		    pxenv_p->RMEntry.segment, pxenv_p->RMEntry.offset);

	/*gci_p = bio_alloc(sizeof(*gci_p));
	if (gci_p == NULL) {
		pxe_p = NULL;
		return (0);
	}
	bzero(gci_p, sizeof(*gci_p));
	gci_p->PacketType = PXENV_PACKET_TYPE_BINL_REPLY;
	pxe_call(PXENV_GET_CACHED_INFO, gci_p);
	if (gci_p->Status != 0) {
		pxe_perror(gci_p->Status);
		bio_free(gci_p, sizeof(*gci_p));
		pxe_p = NULL;
		return (0);
	}
	free(bootp_response);
	if ((bootp_response = malloc(gci_p->BufferSize)) != NULL) {
		bootp_response_size = gci_p->BufferSize;
		bcopy(PTOV((gci_p->Buffer.segment << 4) + gci_p->Buffer.offset),
		    bootp_response, bootp_response_size);
	}
	bio_free(gci_p, sizeof(*gci_p));*/
	return (1);
}

void
pxe_cleanup(void)
{
	t_PXENV_UNLOAD_STACK *unload_stack_p;
	t_PXENV_UNDI_SHUTDOWN *undi_shutdown_p;

	if (pxe_call == NULL)
		return;

	undi_shutdown_p = bio_alloc(sizeof(*undi_shutdown_p));
	if (undi_shutdown_p != NULL) {
		bzero(undi_shutdown_p, sizeof(*undi_shutdown_p));
		pxe_call(PXENV_UNDI_SHUTDOWN, undi_shutdown_p);

#ifdef PXE_DEBUG
		if (pxe_debug && undi_shutdown_p->Status != 0)
			printf("pxe_cleanup: UNDI_SHUTDOWN failed %x\n",
			    undi_shutdown_p->Status);
#endif
		bio_free(undi_shutdown_p, sizeof(*undi_shutdown_p));
	}

	unload_stack_p = bio_alloc(sizeof(*unload_stack_p));
	if (unload_stack_p != NULL) {
		bzero(unload_stack_p, sizeof(*unload_stack_p));
		pxe_call(PXENV_UNLOAD_STACK, unload_stack_p);

#ifdef PXE_DEBUG
		if (pxe_debug && unload_stack_p->Status != 0)
			printf("pxe_cleanup: UNLOAD_STACK failed %x\n",
			    unload_stack_p->Status);
#endif
		bio_free(unload_stack_p, sizeof(*unload_stack_p));
	}
}

void
pxenv_call(int func, void *ptr)
{
#ifdef PXE_DEBUG
	if (pxe_debug)
		printf("pxenv_call %x\n", func);
#endif
	
	bzero(&v86, sizeof(v86));

	__pxenvseg = pxenv_p->RMEntry.segment;
	__pxenvoff = pxenv_p->RMEntry.offset;
	
	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.es   = VTOPSEG(ptr);
	v86.edi  = VTOPOFF(ptr);
	v86.addr = (VTOPSEG(__pxenventry) << 16) | VTOPOFF(__pxenventry);
	v86.ebx  = func;
	v86int();
	v86.ctl  = V86_FLAGS;
}

void
bangpxe_call(int func, void *ptr)
{
#ifdef PXE_DEBUG
	if (pxe_debug)
		printf("bangpxe_call %x\n", func);
#endif

	bzero(&v86, sizeof(v86));

	__bangpxeseg = pxe_p->EntryPointSP.segment;
	__bangpxeoff = pxe_p->EntryPointSP.offset;

	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.edx  = VTOPSEG(ptr);
	v86.eax  = VTOPOFF(ptr);
	v86.addr = (VTOPSEG(__bangpxeentry) << 16) | VTOPOFF(__bangpxeentry);
	v86.ebx  = func;
	v86int();
	v86.ctl  = V86_FLAGS;
}


