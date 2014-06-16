import waflib, os.path

def build(ctx):
    source = []
    includes = []
    libpath = []
    lib = []
    cflags = []
    linkflags = []
    excl = []

    # C99
    cflags.append('-std=c99')
    
    # Optimization
    #cflags.append('-O3') # would affect backtrace support

    # Debugging
    cflags.append('-g')
    cflags.append('-DEXCEPTIONAL_BACKTRACE')
    linkflags.append('-rdynamic') # for improved backtrace support
    
    # OpenMP
    cflags.append('-fopenmp')
    linkflags.append('-fopenmp')

    # POSIX threads
    cflags.append('-pthread')
    lib.append('pthread')
    
    # Main
    source += ctx.path.find_node('src').ant_glob('*.c', excl='exception_scope_sdl.c')
    includes.append(ctx.path.find_node('include').abspath())
    
    # Example
    source += ctx.path.find_node('examples').ant_glob('*.c', excl='example_sdl.c')
    
    # Better String Library
    bstrlib = ctx.path.find_node('dependencies/bstrlib-05122010')
    source += bstrlib.ant_glob('*.c', excl=('*test*', 'bsafe.c'))
    includes.append(bstrlib.abspath())
    print source

    # SimCList
    simclist = ctx.path.find_node('dependencies/simclist-1.6')
    source += simclist.ant_glob('*.c')
    includes.append(simclist.abspath())

    ctx.program(
        target='example',
        source=source,
        lib=lib,
        includes=includes,
        libpath=(path(x) for x in libpath),
        cflags=' '.join(cflags),
        linkflags=linkflags)

def configure(ctx):
    ctx.load('compiler_c')

def options(ctx):
    ctx.load('compiler_c')
