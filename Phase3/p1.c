#include <stdio.h>
#include <usloss.h>

void
p1_fork(int pid)
{
    if (DEBUG3 )
        USLOSS_Console("p1_fork(): pid = %d\n", pid);
}

void
p1_switch(int old, int new)
{
    if (DEBUG3 )
        USLOSS_Console("p1_switch(): old = %d, new = %d\n", old, new);

}

void
p1_quit(int pid)
{
    if (DEBUG3 )
        USLOSS_Console("p1_quit(): pid = %d\n", pid);
}
