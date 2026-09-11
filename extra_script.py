# PlatformIO extra script.
# Native: link gcov for coverage. All envs: put toolchain -I into compiledb
# so host clangd can resolve Arduino/ESP-IDF headers.
Import("env")

env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

if env["PIOENV"] == "native":
    env.Append(LIBS=["gcov"])
