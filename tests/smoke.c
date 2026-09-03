#include <meglang/version.h>

int main(void)
{
    return meg_version() == 2 ? 0 : 1;
}
