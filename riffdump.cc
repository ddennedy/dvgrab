#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "riff.h"
#include "avi.h"

int main (int argc, char *argv[])
{
        try {
                AVIFile avi;
                avi.Open(argv[1]);
                avi.ParseRIFF();
                avi.PrintDirectory();
        } catch (char *s) {
                printf("Exception caught.\n%s\n", s);
        }

        catch (...) {
                printf("Unexpected exception caught.\n");
        }

        return 0;
}

