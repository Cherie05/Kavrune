#!/usr/bin/env python3
"""Create an explicitly scoped SPDX 2.3 direct runtime dependency inventory."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
import uuid

dependencies = {
    'libgtk-4-1': ('LGPL-2.1-or-later', 'https://www.gtk.org/'),
    'libadwaita-1-0': ('LGPL-2.1-or-later', 'https://gitlab.gnome.org/GNOME/libadwaita'),
    'libglib2.0-0t64': ('LGPL-2.1-or-later', 'https://gitlab.gnome.org/GNOME/glib'),
    'libssl3t64': ('Apache-2.0', 'https://openssl-library.org/'),
    'libstdc++6': ('GPL-3.0-or-later WITH GCC-exception-3.1', 'https://gcc.gnu.org/'),
    'libc6': ('NOASSERTION', 'https://www.gnu.org/software/libc/'),
    'udisks2': ('NOASSERTION', 'https://github.com/storaged-project/udisks'),
    'polkitd': ('NOASSERTION', 'https://gitlab.freedesktop.org/polkit/polkit'),
    'dosfstools': ('NOASSERTION', 'https://github.com/dosfstools/dosfstools'),
    'exfatprogs': ('NOASSERTION', 'https://github.com/exfatprogs/exfatprogs'),
    'e2fsprogs': ('NOASSERTION', 'https://e2fsprogs.sourceforge.net/'),
}
packages = [{
    'SPDXID': 'SPDXRef-Kavrune', 'name': 'Kavrune', 'versionInfo': '0.1.0',
    'downloadLocation': 'NOASSERTION', 'filesAnalyzed': False,
    'licenseConcluded': 'MIT', 'licenseDeclared': 'MIT',
    'copyrightText': 'Copyright (c) 2026 Kavrune contributors',
}]
relationships = [{'spdxElementId': 'SPDXRef-DOCUMENT', 'relationshipType': 'DESCRIBES', 'relatedSpdxElement': 'SPDXRef-Kavrune'}]
for name, (license_id, upstream) in dependencies.items():
    result = subprocess.run(['dpkg-query', '-W', '-f=${Status}\t${Version}', name], text=True, capture_output=True)
    if result.returncode or not result.stdout.startswith('install ok installed\t'):
        continue
    identifier = 'SPDXRef-' + name.replace('+', 'plus')
    packages.append({
        'SPDXID': identifier, 'name': name, 'versionInfo': result.stdout.split('\t', 1)[1],
        'downloadLocation': 'NOASSERTION', 'homepage': upstream, 'filesAnalyzed': False,
        'licenseConcluded': 'NOASSERTION', 'licenseDeclared': license_id,
        'copyrightText': 'NOASSERTION',
    })
    relationships.append({'spdxElementId': 'SPDXRef-Kavrune', 'relationshipType': 'DEPENDS_ON', 'relatedSpdxElement': identifier})
document = {
    'spdxVersion': 'SPDX-2.3', 'dataLicense': 'CC0-1.0', 'SPDXID': 'SPDXRef-DOCUMENT',
    'name': 'Kavrune-0.1.0-direct-runtime-inventory',
    'documentNamespace': 'https://spdx.org/spdxdocs/kavrune-' + str(uuid.uuid4()),
    'creationInfo': {'created': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'), 'creators': ['Tool: Kavrune direct dependency inventory']},
    'comment': 'Direct system dependency inventory only. Does not enumerate transitive graphics dependencies, build tools or package contents. Not a completed license or vulnerability audit.',
    'packages': packages, 'relationships': relationships,
}
output = Path(sys.argv[1] if len(sys.argv) > 1 else 'dist/sbom.spdx.json')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(document, indent=2) + '\n')
print(output)
