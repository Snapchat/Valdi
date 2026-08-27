cc_library(
    name = "sqlite",
    srcs = ["sqlite3.c"],
    hdrs = [
        "sqlite3.h",
        "sqlite3ext.h",
    ],
    copts = [
        "-DSQLITE_DEFAULT_MEMSTATUS=0",
        "-DSQLITE_OMIT_LOAD_EXTENSION",
        "-DSQLITE_THREADSAFE=1",
        "-Wno-cast-qual",
        "-Wno-implicit-fallthrough",
        "-Wno-pedantic",
        "-Wno-shorten-64-to-32",
        "-Wno-sign-compare",
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unknown-warning-option",
    ],
    strip_include_prefix = ".",
    visibility = ["//visibility:public"],
)
