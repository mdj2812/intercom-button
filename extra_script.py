# PlatformIO extra script for native test coverage.
# Injects -lgcov into the linker flags for the native environment.
Import("env")

if env["PIOENV"] == "native":
    env.Append(LIBS=["gcov"])
