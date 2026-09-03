#include <meglang/version.h>

int main(void)
{
    return meg_version() == 1 ? 0 : 1;
}
