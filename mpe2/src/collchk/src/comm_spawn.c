/*
   (C) 2004 by Argonne National Laboratory.
       See COPYRIGHT in top-level directory.
*/
#include "collchk.h" 

int MPI_Comm_spawn(const char *command, char **argv, int maxprocs, MPI_Info info,
                   int root, MPI_Comm comm, MPI_Comm *intercomm,
                   int *array_of_errcodes)
{
    int g2g = 1;
    char call[COLLCHK_SM_STRLEN];

    sprintf(call, "COMM_SPAWN");

    /* Check if init has been called */
    g2g = CollChk_is_init();

    if(g2g) {
        /* check for call consistancy */
        CollChk_same_call(comm, call);
        /* check for root consistancy */
        CollChk_same_root(comm, root, call);

        /* make the call */
        return PMPI_Comm_spawn(command, argv, maxprocs, info, root, comm,
                               intercomm, array_of_errcodes);
    }
    else {
        /* init not called */
        return CollChk_err_han("MPI_Init() has not been called!",
                               COLLCHK_ERR_NOT_INIT, call, comm);
    }
}
