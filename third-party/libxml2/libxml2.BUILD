# BUILD file for the @libxml2 external repo (GNOME/libxml2 v2.9.14 GitHub
# archive). Produces a static libxml2 that the Linux Valdi compiler links so
# the Swift static runtime's libFoundationXML.a resolves hermetically instead
# of depending on a host libxml2.so.2 at runtime.
#
# config.h and xmlversion.h are hand-maintained in
# @valdi//third-party/libxml2 (the GitHub archive only ships the .in
# templates; release tarballs generate them via autoconf). The module
# selection in xmlversion.h covers exactly the libxml2 API surface
# libFoundationXML.a references (parser/push, tree, SAX2, DTD validation +
# regexps, XPath, xmlsave, hash/dict) and disables everything with external
# library dependencies (iconv, icu, zlib, lzma) plus the network modules.
#
# The .c files of disabled modules are still compiled; their contents are
# fully guarded by LIBXML_*_ENABLED so they produce empty objects.

genrule(
    name = "config_h",
    srcs = ["@valdi//third-party/libxml2:config.h"],
    outs = ["config.h"],
    cmd = "cp $< $@",
)

genrule(
    name = "xmlversion_h",
    srcs = ["@valdi//third-party/libxml2:xmlversion.h"],
    outs = ["include/libxml/xmlversion.h"],
    cmd = "cp $< $@",
)

cc_library(
    name = "libxml2",
    srcs = [
        "HTMLparser.c",
        "HTMLtree.c",
        "SAX.c",
        "SAX2.c",
        "buf.c",
        "c14n.c",
        "catalog.c",
        "chvalid.c",
        "debugXML.c",
        "dict.c",
        "encoding.c",
        "entities.c",
        "error.c",
        "globals.c",
        "hash.c",
        "legacy.c",
        "list.c",
        "nanoftp.c",
        "nanohttp.c",
        "parser.c",
        "parserInternals.c",
        "pattern.c",
        "relaxng.c",
        "schematron.c",
        "threads.c",
        "tree.c",
        "uri.c",
        "valid.c",
        "xinclude.c",
        "xlink.c",
        "xmlIO.c",
        "xmlmemory.c",
        "xmlreader.c",
        "xmlregexp.c",
        "xmlsave.c",
        "xmlschemas.c",
        "xmlschemastypes.c",
        "xmlstring.c",
        "xmlunicode.c",
        "xmlwriter.c",
        "xpath.c",
        "xpointer.c",
        "xzlib.c",
        # Private headers used via quote-includes from the .c files.
        "buf.h",
        "elfgcchack.h",
        "enc.h",
        "libxml.h",
        "save.h",
        "timsort.h",
        "xzlib.h",
        ":config_h",
    ],
    hdrs = glob(["include/libxml/*.h"]) + [":xmlversion_h"],
    copts = [
        "-DLIBXML_STATIC",
        # Third-party code; not ours to keep warning-clean.
        "-w",
    ],
    includes = [
        ".",
        "include",
    ],
    linkstatic = True,
    visibility = ["//visibility:public"],
)
