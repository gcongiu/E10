
/* on bgl the fortran compiler doesn't add underscores to symbols, and the
 * -brename trick used on seaborg and blue doesn't work with the bgl linker */

#define  h5_close_file_           h5_close_file 
#define  h5_initialize_file_      h5_initialize_file 
#define  h5_write_header_info_    h5_write_header_info 
#define  h5_write_lrefine_        h5_write_lrefine 
#define  h5_write_nodetype_       h5_write_nodetype 
#define  h5_write_gid_            h5_write_gid 
#define  h5_write_coord_          h5_write_coord 
#define  h5_write_size_           h5_write_size 
#define  h5_write_bnd_box_        h5_write_bnd_box 
#define  h5_write_bnd_box_min_    h5_write_bnd_box_min 
#define  h5_write_bnd_box_max_    h5_write_bnd_box_max 
#define  h5_write_unknowns_       h5_write_unknowns 
#define  h5_write_header_info_sp_ h5_write_header_info_sp 
#define  h5_write_coord_sp_       h5_write_coord_sp 
#define  h5_write_size_sp_        h5_write_size_sp 
#define  h5_write_bnd_box_sp_     h5_write_bnd_box_sp 
#define  h5_write_bnd_box_min_sp_ h5_write_bnd_box_min_sp 
#define  h5_write_bnd_box_max_sp_ h5_write_bnd_box_max_sp 
#define  h5_write_unknowns_sp_    h5_write_unknowns_sp 
