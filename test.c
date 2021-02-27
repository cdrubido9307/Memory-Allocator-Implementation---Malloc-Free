/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

// LD_PRELOAD=./th_alloc.so ./test

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

int main()
{
    // Simple malloc and free tests
    printf("Simple malloc and free tests\n");
    void *x = malloc(5);
    printf("Hello x %p\n", x);
    free(x);
    printf("After free() %p\n", x);

    void *y = malloc(5);
    printf("Hello y %p\n", y);
    free(y);
    printf("After free() %p\n", y);

    void *z_1 = malloc(1000);
    printf("Hello z_1 %p\n", z_1);
    free(z_1);
    printf("After free() %p\n", z_1);

    //Memory poisoning in malloc and free
    void *tmp = malloc(50);
    unsigned char *ch1 = (unsigned char *)tmp;
    printf("Memory poisoning in malloc => ");
    for (int i = 0; i < 50; i++)
        printf("%x", ch1[i]);
    printf("\n");

    free(tmp);
    unsigned char *ch2 = (unsigned char *)tmp;
    printf("Memory poisoning in free => ");
    for (int i = 0; i < 50; i++)
        printf("%x", ch2[i]);
    printf("\n");

    //Allocating multiple variables in same superblock
    printf("multiple small variables allocation\n");
    void *z1 = malloc(100);
    printf("z1 %p\n", z1);

    void *z2 = malloc(100);
    printf("z2 %p\n", z2);

    void *z3 = malloc(100);
    printf("z3 %p\n", z3);

    void *z4 = malloc(100);
    printf("z4 %p\n", z4);

    void *z5 = malloc(100);
    printf("z5 %p\n", z5);

    //free(z);
    free(z1);
    free(z2);
    free(z3);
    free(z4);
    free(z5);

    printf("After free() %p\n", z1);
    printf("After free() %p\n", z2);
    printf("After free() %p\n", z3);
    printf("After free() %p\n", z4);
    printf("After free() %p\n", z5);

    printf("multiple big variables allocation. Needs to call super_alloc(new SB)\n");
    void *x1 = malloc(1000);
    printf("x1 %p\n", x1);

    void *x2 = malloc(1000);
    printf("x2 %p\n", x2);

    void *x3 = malloc(1000);
    printf("x3 %p\n", x3);

    void *x4 = malloc(1000);
    printf("x4 %p\n", x4);

    void *x5 = malloc(1000);
    printf("x5 %p\n", x5);

    //free(z);
    free(x1);
    free(x2);
    free(x3);
    free(x4);
    free(x5);

    printf("After free() %p\n", x1);
    printf("After free() %p\n", x2);
    printf("After free() %p\n", x3);
    printf("After free() %p\n", x4);
    printf("After free() %p\n", x5);

    return errno;
}
