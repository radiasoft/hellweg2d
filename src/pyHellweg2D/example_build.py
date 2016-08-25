

from cffi import FFI
ffibuilder = FFI()

ffibuilder.set_source("pyhellweg",
    """
    #include <libHellweg2D.h>
    """,
    libraries=['Hellweg2D'], include_dirs=['../libHellweg2D'])   

ffibuilder.cdef("""
enum lib_hellweg_err {
    INPUT_FILE_ERR,
    OUTPUT_FILE_ERR,
    GEOMETRY_STEP_ERR,
    BEAM_STEP_ERR,
    SOLVE_STEP_ERR,
    SOLVER_INIT_ERR,
    INI_FILE_ERR,
    NO_ERR
};

enum lib_hellweg_err lib_hellweg_run_beam_solver(const char*, const char*, const char*);
""")

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)