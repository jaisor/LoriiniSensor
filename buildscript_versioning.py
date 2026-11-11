FILENAME_VERSION_H = 'include/version.h'

import datetime
import time
import os

build_no = 0

semver = os.environ.get('SEMVER', '').strip()
if semver:
    version = f"{semver}"
else:
    version = f"dev.{int(time.time())}"


hf = f"""
#ifndef VERSION
  #define VERSION "{version} - {datetime.datetime.now()}"
#endif
#ifndef VERSION_SHORT
  #define VERSION_SHORT "{version}"
#endif
"""

with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(hf)
