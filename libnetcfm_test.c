#include <stdio.h>
#include <stdlib.h>

#include <libnetcfm/libnetcfm.h>

int main(void) {
    
    printf("Running with: %s\n", netcfm_lib_version());

    return EXIT_SUCCESS;
}