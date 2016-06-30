/*
   (C) 2004 by Argonne National Laboratory.
       See COPYRIGHT in top-level directory.
*/
#include "collchk.h" 

int MPI_File_write_ordered(MPI_File fh, const void *buff, int cnt,
                           MPI_Datatype dtype, MPI_Status *st)
{
    int g2g = 1;
    char call[COLLCHK_SM_STRLEN];
    char err_str[COLLCHK_STD_STRLEN];
    MPI_Comm comm;

    sprintf(call, "FILE_WRITE_ORDERED");

    /* Check if init has been called */
    g2g = CollChk_is_init();

    if(g2g) {
        /* get the communicator */
        if (!CollChk_get_fh(fh, &comm)) {
            return CollChk_err_han("File has not been opened.", 
                                   COLLCHK_ERR_FILE_NOT_OPEN,
                                   call, MPI_COMM_WORLD);
        }

        /* check for call consistancy */
        CollChk_same_call(comm, call);

        /* check not after a begin; */
        if(COLLCHK_CALLED_BEGIN) {
            sprintf(err_str, "A MPI_File_%s_begin operation has been called, "
                             "you must call MPI_File_%s_end first.",
                             CollChk_begin_str, CollChk_begin_str);
            return CollChk_err_han(err_str, COLLCHK_ERR_PREVIOUS_BEGIN,
                                   call, MPI_COMM_WORLD);
        }

        /* make the call */
        return PMPI_File_write_ordered(fh, buff, cnt, dtype, st);
    }
    else {
        /* init not called */
        return CollChk_err_han("MPI_Init() has not been called.",
                               COLLCHK_ERR_NOT_INIT, call, MPI_COMM_WORLD);
    }
}
