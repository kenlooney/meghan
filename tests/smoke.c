#include <meglang/version.h>

int main(void)
{
    return meg_version() == 3 ? 0 : 1;
}
