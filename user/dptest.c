#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print_pte(uint64 va)
{
    pte_t pte = (pte_t) pgpte((void *) va);
    printf("va 0x%lx pte 0x%lx pa 0x%lx perm 0x%lx\n", va, pte, PTE2PA(pte), PTE_FLAGS(pte));
}

void
print_pgtbl()
{
  printf("print_pgtbl starting\n");
  for (uint64 i = 0; i < 10; i++) {
    print_pte(i * PGSIZE);
  }
  uint64 top = MAXVA/PGSIZE;
  for (uint64 i = top-10; i < top; i++) {
    print_pte(i * PGSIZE);
  }
  printf("print_pgtbl: OK\n");
}

int
main(int argc, char *argv[])
{
	printf("4 page test\n");
  char *old = sbrk(PGSIZE * 4);
	print_pgtbl();
	for (int i = 0; i < PGSIZE * 4; i++) {
		old[i] = (char)i;
  }
  char result = 0;
	for (int i = 0; i < PGSIZE * 4; i++) {
		result |= old[i];
  }
	printf("result %x\n", result);

	printf("large sbrk\n");
  int large = 100 * 1024 * 1024;
  old = sbrk(large);
  printf("old brk %p\n", old);
  old = sbrk(large);
  printf("old brk %p\n", old);
  old = sbrk(-large);
  printf("old brk %p\n", old);
  old = sbrk(0);
  printf("old brk %p\n", old);
	printf("%s: OK\n", argv[0]);
  exit(0);
}
