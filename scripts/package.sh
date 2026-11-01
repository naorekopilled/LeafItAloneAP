#!/bin/bash
# Builds dist/leaf_it_alone.apworld and dist/version.dll.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$R/dist"
"$R/client/build.sh" >/dev/null
cp "$R/client/build/version.dll" "$R/LICENSE" "$R/THIRD_PARTY_LICENSES.txt" "$R/dist/"
cd "$R/apworld"
rm -f "$R/dist/leaf_it_alone.apworld"
python3 -I -c "
import zipfile,pathlib
z=zipfile.ZipFile('$R/dist/leaf_it_alone.apworld','w',zipfile.ZIP_DEFLATED)
for f in sorted(pathlib.Path('leaf_it_alone').rglob('*')):
    if f.is_file() and '__pycache__' not in f.parts: z.write(f)
z.close()"
ls -la "$R/dist"
