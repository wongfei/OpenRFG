# Third-party code that is not recovered from rfg.exe.

# zlib 1.2.11 (unmodified sources, see zlib/README). rfg.exe links zlib 1.2.3; the deflate stream format and the
# z_stream layout are the same, and vlib only uses inflate/deflate through zlib_stream.
add_library(openrfg_zlib STATIC
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)
target_include_directories(openrfg_zlib PUBLIC zlib)
if(MSVC)
    # Drop the project's /W4 first: cl warns (D9025) when /w overrides it.
    get_target_property(openrfg_zlib_options openrfg_zlib COMPILE_OPTIONS)
    list(REMOVE_ITEM openrfg_zlib_options /W4)
    set_target_properties(openrfg_zlib PROPERTIES COMPILE_OPTIONS "${openrfg_zlib_options}")
    target_compile_options(openrfg_zlib PRIVATE /w)
endif()
