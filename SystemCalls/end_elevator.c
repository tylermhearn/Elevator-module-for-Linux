#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

long (*STUB_end_elevator)(void) = NULL;
EXPORT_SYMBOL(STUB_end_elevator);

SYSCALL_DEFINE0(end_elevator){
    if(STUB_end_elevator != NULL)
        return STUB_end_elevator();
    else
        return -ENOSYS;
}