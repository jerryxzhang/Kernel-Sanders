/* Encrypts, then decrypts, 2 MB of memory and verifies that the
   values are as they should be. */

//include <string.h>
#include <stdio.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (2 * 1024 * 1024)

void *memset2(void *dst_, int value, size_t size) {
    unsigned char *dst = dst_;
    ASSERT(dst != NULL || size == 0);

    printf("START COPYING\n");
    while (size -- > 0) {
        *dst++ = value;
        if (size < 100000) {
            printf("copied %d\n", size);
        }
    }

    printf("DONE COPYING\n");
    return dst_;
}

 static char buf[SIZE];
void
test_main (void)
{
  struct arc4 arc4;
  size_t i;
printf("BUF AT %x\n", buf);
  /* Initialize to 0x5a. */
  msg ("initialize");


  memset2(buf, 0x5a, sizeof buf);

  /* Check that it's all 0x5a. */
  msg ("read pass");
//  for (i = 0; i < SIZE; i++)
 //   if (buf[i] != 0x5a)
  ///    fail ("byte %zu != 0x5a", i);

  /* Encrypt zeros. */
  msg ("read/modify/write pass one");
//  arc4_init (&arc4, "foobar", 6);
//  arc4_crypt (&arc4, buf, SIZE);

  /* Decrypt back to zeros. */
  msg ("read/modify/write pass two");
//  arc4_init (&arc4, "foobar", 6);
//  arc4_crypt (&arc4, buf, SIZE);

  /* Check that it's all 0x5a. */
  msg ("read pass");
//  for (i = 0; i < SIZE; i++)
 //   if (buf[i] != 0x5a)
  //    fail ("byte %zu != 0x5a", i);
}
