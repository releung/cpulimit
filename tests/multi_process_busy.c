#include <unistd.h>

int main(void)
{
    fork();
    fork();
    while (1)
        ;
    return 0;
}
