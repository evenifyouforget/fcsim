"""
Warnings taken from https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
Runs `scons` to find out what is actually warning.
Print a list of all warnings that we didn't trigger.
"""

import argparse
import logging
import re
import subprocess

gcc_wpedantic = [
    "-Wattributes",
    "-Wchanges-meaning",
    "-Wcomma-subscript",
    "-Wdeclaration-after-statement",
    "-Welaborated-enum-base",
    "-Wimplicit-int",
    "-Wimplicit-function-declaration",
    "-Wincompatible-pointer-types",
    "-Wint-conversion",
    "-Wlong-long",
    "-Wmain",
    "-Wnarrowing",
    "-Wpointer-arith",
    "-Wpointer-sign",
    "-Wincompatible-pointer-types",
    "-Wregister",
    "-Wvla",
    "-Wwrite-strings",
    ]
gcc_wall = [
    "-Waddress",
    "-Waligned-new",
    "-Warray-bounds=1",
    "-Warray-compare",
    "-Warray-parameter=2",
    "-Wbool-compare",
    "-Wbool-operation",
    "-Wc++11-compat",
    "-Wc++14-compat",
    "-Wc++17compat",
    "-Wc++20compat",
    "-Wcatch-value",
    "-Wchar-subscripts",
    "-Wclass-memaccess",
    "-Wcomment",
    "-Wdangling-else",
    "-Wdangling-pointer=2",
    "-Wdelete-non-virtual-dtor",
    "-Wduplicate-decl-specifier",
    "-Wenum-compare",
    "-Wenum-int-mismatch",
    "-Wformat=1",
    "-Wformat-contains-nul",
    "-Wformat-diag",
    "-Wformat-extra-args",
    "-Wformat-overflow=1",
    "-Wformat-truncation=1",
    "-Wformat-zero-length",
    "-Wframe-address",
    "-Wimplicit",
    "-Wimplicit-function-declaration",
    "-Wimplicit-int",
    "-Winfinite-recursion",
    "-Winit-self",
    "-Wint-in-bool-context",
    "-Wlogical-not-parentheses",
    "-Wmain",
    "-Wmaybe-uninitialized",
    "-Wmemset-elt-size",
    "-Wmemset-transposed-args",
    "-Wmisleading-indentation",
    "-Wmismatched-dealloc",
    "-Wmismatched-new-delete",
    "-Wmissing-attributes",
    "-Wmissing-braces",
    "-Wmultistatement-macros",
    "-Wnarrowing",
    "-Wnonnull",
    "-Wnonnull-compare",
    "-Wopenmp-simd",
    "-Woverloaded-virtual=1",
    "-Wpacked-not-aligned",
    "-Wparentheses",
    "-Wpessimizing-move",
    "-Wpointer-sign",
    "-Wrange-loop-construct",
    "-Wreorder",
    "-Wrestrict",
    "-Wreturn-type",
    "-Wself-move",
    "-Wsequence-point",
    "-Wsign-compare",
    "-Wsizeof-array-div",
    "-Wsizeof-pointer-div",
    "-Wsizeof-pointer-memaccess",
    "-Wstrict-aliasing",
    "-Wstrict-overflow=1",
    "-Wswitch",
    "-Wtautological-compare",
    "-Wtrigraphs",
    "-Wuninitialized",
    "-Wunknown-pragmas",
    "-Wunused",
    "-Wunused-but-set-variable",
    "-Wunused-const-variable=1",
    "-Wunused-function",
    "-Wunused-label",
    "-Wunused-local-typedefs",
    "-Wunused-value",
    "-Wunused-variable",
    "-Wuse-after-free=2",
    "-Wvla-parameter",
    "-Wvolatile-register-var",
    "-Wzero-length-bounds",
    ]
gcc_wextra = [
    "-Wabsolute-value",
    "-Walloc-size",
    "-Wcalloc-transposed-args",
    "-Wcast-function-type",
    "-Wclobbered",
    "-Wdangling-reference",
    "-Wdeprecated-copy",
    "-Wempty-body",
    "-Wenum-conversion",
    "-Wexpansion-to-defined",
    "-Wignored-qualifiers",
    "-Wimplicit-fallthrough=3",
    "-Wmaybe-uninitialized",
    "-Wmissing-field-initializers",
    "-Wmissing-parameter-name",
    "-Wmissing-parameter-type",
    "-Wold-style-declaration",
    "-Woverride-init",
    "-Wredundant-move",
    "-Wshift-negative-value",
    "-Wsign-compare",
    "-Wsized-deallocation",
    "-Wstring-compare",
    "-Wtype-limits",
    "-Wuninitialized",
    "-Wunterminated-string-initialization",
    "-Wunused-parameter",
    "-Wunused-but-set-parameter",
    ]
all_warnings = set(gcc_wpedantic + gcc_wall + gcc_wextra)

def main() -> None:
    logging.basicConfig(level=logging.INFO)
    # parse args
    parser = argparse.ArgumentParser(
                    prog='find_unused_warnings.py',
                    description='Run scons and find warnings that are not triggered but exist in gcc')
    parser.add_argument('-e', '--error', action='store_true', help="Print instead some Python code to make scons treat those warnings as errors")
    args = parser.parse_args()
    # run subprocess
    subprocess.run(['scons', '-c'], text=True, capture_output=True)
    proc = subprocess.run(['scons'], text=True, capture_output=True)
    hit_warnings_all = re.findall('(.*?src.+?(-W[^\\s]+\\w).*)', proc.stderr)
    hit_warnings = {match[1] for match in hit_warnings_all}
    hit_warnings_dict = {match[1]: match[0] for match in hit_warnings_all}
    missed_warnings = all_warnings - hit_warnings
    # output
    logging.info(f'{len(all_warnings)} total known warnings')
    logging.info(f'{len(hit_warnings)} hit warnings')
    logging.info(f'{len(missed_warnings)} missed warnings')
    if not args.error:
        print('\n'.join(sorted(missed_warnings)))
    else:
        for warning in sorted(all_warnings):
            if warning not in hit_warnings:
                print(f'    "-Werror={warning[2:]}",')
            else:
                print(f'# auto comment: build would fail with this: {hit_warnings_dict[warning]}')
                print(f'#   "-Werror={warning[2:]}",')

if __name__ == "__main__":
    main()