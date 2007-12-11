#include <arm9_utils.h>
#include <misc.h>
#include <sha1.h>
#include <tea.h>
#include <tokendefs.h>

#define NOR			((unsigned char *)	0xA0000000)
#define NORWORD			((unsigned short int *) 0xA0000000)

#define CHIPID_LOC		((unsigned char *) 0xF440006C)
#define CHIPID_LEN		12

#define NORID_LOC		((unsigned char *) 0xFFFF3BE2)
#define NORID_LEN		8

#define SCRATCH_OFFSET		0x3E0000

#define SECZONE_OFFSET		0x3FA000
#define SECZONE_SIZE		0x1000
#define LOCK_TABLE_OFFSET	0xC88
#define LOCK_TABLE_SIZE		0xe0

void eraseblock(int addr);
void writeblock(int block, int length, unsigned char *payload);

const unsigned char virgin[LOCK_TABLE_SIZE] = {
    0x00,0x01,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x01,0x05,0x01,0x00,0x00,0x00,0x00, 
    0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void virgintool_main() __attribute__ ((section (".virgintool_entry")));

/* #define DEBUG */

void
virgintool_entry()
{
    unsigned char payload[SECZONE_SIZE];

    unsigned char NOR_ID[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    
    unsigned char CHIP_ID[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    unsigned long sha1[5], key[4], *ltp;
    sha1_context ctx;
    unsigned char tmp[0x10];

    /* Copy the NOR and HWID */
    memcpy(CHIP_ID, CHIPID_LOC, CHIPID_LEN);
    memcpy(tmp, (void *)0xFFFF3C54L, 0x10);
    memcpy(NOR_ID, tmp + 2, NORID_LEN);

    /* Create the hash */
    sha1_starts(&ctx); 
    sha1_update(&ctx, NOR_ID,  0x10);
    sha1_update(&ctx, CHIP_ID, 0x10);
    sha1_finish(&ctx, (void *)sha1);

    /* Endian swap the first 16 bits of the SHA1 output */
    key[0] = htonl(sha1[0]);
    key[1] = htonl(sha1[1]);
    key[2] = htonl(sha1[2]);
    key[3] = htonl(sha1[3]);	

#if defined(DEBUG)

    memset(payload, 0, 1024);
    memcpy(payload + 0x00, NOR_ID,  NORID_LEN);
    memcpy(payload + 0x10, CHIP_ID, CHIPID_LEN);
    memcpy(payload + 0x20, (void *)key, 0x10);
    unsigned long n = htonl(0x12345655); /* version */
    memcpy(payload + 0x30, (void *)&n, 4);

    /* If DEBUG, just write to SCRATCH_OFFSET */
    eraseblock(SCRATCH_OFFSET);
    writeblock(SCRATCH_OFFSET, 1024, payload);

#else

    /* Copy the original seczone */
    memcpy(payload, NOR + SECZONE_OFFSET, SECZONE_SIZE);

    /* Set a pointer to the lock table */
    ltp = (unsigned long *)(payload + LOCK_TABLE_OFFSET);

    /* Zero out lock table */
    memcpy((unsigned char *)ltp, virgin, sizeof(virgin));

    /* Re-encrypt the lock table */
    encrypt_cbc(ltp, LOCK_TABLE_SIZE, key, ltp);
    
    /* Virginize! */
    eraseblock(SECZONE_OFFSET);
    writeblock(SECZONE_OFFSET, sizeof(payload), payload);

#endif	/* defined(DEBUG) */

    /* Hang until reset */
    arm9_stop_mode();
}

void
eraseblock(int addr)
{
    NOR[addr] = 0x60;	/* Lock setup */
    NOR[addr] = 0xD0;	/* Unlock block */
    NOR[addr] = 0xFF;	/* Read array */
    NOR[addr] = 0x20;	/* Erase setup */
    NOR[addr] = 0xD0;	/* Erase confirm */
    while(!(NOR[addr] & 0x80));	/* Wait for WSM ready */
}

void
writeblock(int addr, int length, unsigned char *payload)
{
  unsigned int i;
  unsigned short int j;

  /* Blocks are already unlocked at this point */
  for(i = 0; i < length; i += 4) {
      j  = *payload | (*(payload + 1) << 8);
      payload += 2;

      NOR[addr + i] = 0x40;		/* Program setup */
      NORWORD[(addr + i) >> 1] = j;	/* Data */
      while(!(NOR[addr+i] & 0x80));	/* Wait for WSM ready */
    
      j  = *payload | (*(payload + 1) << 8);
      payload += 2;

      NOR[addr + i] = 0x40;
      NORWORD[((addr + i) >> 1) + 1] = j;
      while(!(NOR[addr+i] & 0x80));
  }
}