import pathlib
import re
import shutil

def main():
    tinystl_dir = pathlib.Path("TinySTL/TinySTL")
    tinystl_files = {
        'Algorithm': None,
        'Alloc': None,
        'Allocator': None,
        'Construct': None,
        'Iterator': None,
        'ReverseIterator': None,
        'String': None,
        'TypeTraits': 'type_traits',
        'UninitializedFunctions': None,
        'Utility': None,
        'Vector': None,
        }
    remove_lib = {'assert', 'new'}
    target_dir = pathlib.Path("include/tinystl")
    if target_dir.exists():
        shutil.rmtree(target_dir)
    target_dir.mkdir()
    for libfn, libname in tinystl_files.items():
        libname = libname or libfn.lower()
        filepath = tinystl_dir / (libfn + '.h')
        with open(filepath, encoding='iso8859_16') as file:
            raw = file.read()
        raw = re.sub('#include <c(\\w+)>', '#include "\\1.h"', raw)
        def repl_func(match):
            req_libname = match[1]
            if req_libname in remove_lib:
                return ''
            req_libfn = (tinystl_files.get(req_libname, None) or req_libname.lower()) + '.h'
            return '#include "' + req_libfn + '"'
        raw = re.sub('#include "(\\w+).h"', repl_func, raw)
        raw = re.sub('#include <(\\w+)>', repl_func, raw)
        target_filepath = target_dir / (libname + '.h')
        with open(target_filepath, 'w') as file:
            file.write(raw)

if __name__ == "__main__":
    main()