#include <meglang/version.h>

int main(void)
{
    return meg_version() == 6 ? 0 : 1;
}
