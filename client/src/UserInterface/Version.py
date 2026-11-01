import os
import re
import sys
import subprocess as sp

def P4_GetVersion(path):
    cmd = "p4 changes -m1 %s" % path
    proc = sp.Popen(cmd, stdout=sp.PIPE)
    return proc.stdout.read()

try:
	oldData = open("Version.h").read()
	mo = re.search(r"\d+;", oldData)
	oldVersion = int(mo.group()[:-1]) if mo else 0
except IOError:
# dmVldmVl
	oldVersion = 0

newVersion = int(P4_GetVersion("..\\..\\...").split()[1])
if oldVersion != newVersion:
	with open("Version.h", "wb") as f:
		f.write(("#define VER_FILE_VERSION   1,0,%d,0\r\n" % newVersion).encode("ascii"))
		f.write(("#define VER_FILE_VERSION_STR   \"1.0.%d.1\"\r\n" % newVersion).encode("ascii"))
		f.write(("int METIN2_GET_VERSION() { return %d; }\r\n" % newVersion).encode("ascii"))
