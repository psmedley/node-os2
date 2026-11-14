#!/usr/bin/env python3
"""Generate an OS/2 module definition (.def) file for a kLIBC DLL.

Used by the OS/2 'solink' rule of the make generator:

  os2_gendef.py [--os2-exports=FILE] OUTPUT.dll OUTPUT.def INPUT.a|INPUT.o ...

The EXPORTS section lists the global symbols defined in the inputs, as
reported by nm(1). Weak and common symbols are skipped (users of the DLL get
their own copies of inline functions and template instances), as are names
the LX format and emxomf cannot carry.

With --os2-exports=FILE only symbols matching the patterns in FILE are
exported. A gyp target passes it as an ldflag, which the rule keeps away from
the linker. FILE has one Python regular expression per line, matched against
the symbol name as nm prints it (with the leading underscore): "+regex" to
include and "-regex" to exclude; "#" starts a comment. Excludes win over
includes.
"""

import os
import re
import subprocess
import sys

# LX entry tables use 16-bit ordinals.
MAX_EXPORTS = 65535
# emxomf shortens long names (OMF records hold at most 255 characters), so an
# export by the a.out name doesn't resolve: in the libnode.dll link every name
# of 228 characters or more failed and none shorter did. Keep a small margin.
MAX_NAME = 224
EXPORTED_TYPES = set("TDBR")
VALID_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def read_patterns(path):
    include, exclude = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line[0] == "+":
                include.append(re.compile(line[1:]))
            elif line[0] == "-":
                exclude.append(re.compile(line[1:]))
            else:
                sys.exit("%s: pattern must start with + or -: %s" % (path, line))
    return include, exclude


def defined_symbols(inputs, nm):
    # Batch the inputs to stay well below command line limits.
    batch = 50
    for i in range(0, len(inputs), batch):
        proc = subprocess.run(
            [nm, "-g", "-P", "--defined-only"] + inputs[i : i + batch],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        # nm warns about every object without symbols; only pass on the rest.
        for line in proc.stderr.splitlines():
            if not line.endswith(": no symbols"):
                print(line, file=sys.stderr)
        if proc.returncode != 0:
            sys.exit("%s failed with exit code %d" % (nm, proc.returncode))
        out = proc.stdout
        for line in out.splitlines():
            fields = line.split()
            # Archive member headers end with ':' and have no type field.
            if len(fields) >= 2 and len(fields[1]) == 1:
                yield fields[0], fields[1]


def main(argv):
    exports_file = None
    args = []
    for arg in argv[1:]:
        if arg.startswith("--os2-exports="):
            exports_file = arg[len("--os2-exports=") :]
        else:
            args.append(arg)
    if len(args) < 2:
        sys.exit(__doc__)
    dll, def_path, inputs = args[0], args[1], args[2:]
    inputs = [i for i in inputs if i.endswith((".a", ".o"))]

    include, exclude = read_patterns(exports_file) if exports_file else ([], [])
    nm = os.environ.get("NM", "nm")

    names = set()
    skipped_long = 0
    for name, sym_type in defined_symbols(inputs, nm):
        if sym_type not in EXPORTED_TYPES or not VALID_NAME.match(name):
            continue
        if include and not any(p.search(name) for p in include):
            continue
        if any(p.search(name) for p in exclude):
            continue
        if len(name) > MAX_NAME:
            skipped_long += 1
            continue
        names.add(name)

    if len(names) > MAX_EXPORTS:
        sys.exit(
            "%s: %d exports, more than the %d an LX module can have; "
            "narrow them with --os2-exports" % (dll, len(names), MAX_EXPORTS)
        )

    module = os.path.splitext(os.path.basename(dll))[0]
    if len(module) > 8:
        print(
            "os2_gendef.py: warning: DLL name %r is longer than 8 characters"
            % module,
            file=sys.stderr,
        )
    tmp = def_path + ".tmp"
    with open(tmp, "w") as f:
        f.write("LIBRARY %s INITINSTANCE TERMINSTANCE\n" % module)
        f.write("DATA MULTIPLE NONSHARED\n")
        f.write("EXPORTS\n")
        for name in sorted(names):
            f.write("  %s\n" % name)
    os.replace(tmp, def_path)
    print(
        "os2_gendef.py: %s: %d exports (%d names too long to export)"
        % (os.path.basename(def_path), len(names), skipped_long)
    )


if __name__ == "__main__":
    main(sys.argv)
