// lwIP's synchronous unit-test OS port, with normal invalid-handle semantics.
// Its stock invalidation asserts that an uninitialized mailbox is already zero;
// malloc/ASan correctly does not promise that for netconn's unused TCP acceptmbox.
#include <lwip/sys.h>
#define sys_mbox_set_invalid unused_unit_mbox_set_invalid
#include <sys_arch.c>
#undef sys_mbox_set_invalid
void sys_mbox_set_invalid(sys_mbox_t *box) { memset(box,0,sizeof(*box)); }
