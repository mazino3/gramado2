
#include "shell.h"



#ifndef PDESKTOPFOLDER
#define PDESKTOPFOLDER "/tmp/volume0"
#endif
char *primary_desktop_folder = PDESKTOPFOLDER;
#ifndef SDESKTOPFOLDER
#define SDESKTOPFOLDER "/tmp/volume1/users/fred/desktop"
#endif
char *secondary_desktop_folder = SDESKTOPFOLDER;
int desktopInitialize (void)
{
    printf ("Initializing desktop ...\n");
    printf ("%s\n",primary_desktop_folder);
    printf ("%s\n",secondary_desktop_folder);
    return 0;
}


//
// =================================================
//


