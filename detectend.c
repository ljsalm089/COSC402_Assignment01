#include "common.h"

union test_union {
    short val;
    char buff[sizeof(short)];
};

int main(int argc, char *argv)
{
    union test_union var;
    var.val = 0x0102;

    if (sizeof(union test_union) == 2) 
    {
        I("MAIN", "size is 2");
        if (var.buff[0] == 0x01 && var.buff[1] == 0x02)
        {
            I("MAIN", "little end machine");
        }
        else 
        {

            I("MAIN", "big end machine");
        }
    }
    return 0;
}
