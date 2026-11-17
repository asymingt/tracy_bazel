"""Bazel port of the profiler's Embed() CMake function.

Each embedded file is LZ4HC-compressed into a C array by upstream's
profiler/helpers/embed.cpp (built for the execution platform), producing
gen/data/<Name>.{cpp,hpp}. Sources include them as "data/<Name>.hpp".
"""

def _tracy_embed_impl(ctx):
    srcs = []
    hdrs = []
    for target, spec in ctx.attr.files.items():
        name, _, kind = spec.partition(":")
        src = target.files.to_list()
        if len(src) != 1:
            fail("%s must provide exactly one file" % target.label)
        src = src[0]
        cpp = ctx.actions.declare_file("gen/data/%s.cpp" % name)
        hpp = ctx.actions.declare_file("gen/data/%s.hpp" % name)
        args = ctx.actions.args()
        if kind == "text":
            # Normalise line endings to LF, as Embed(... TEXT).
            args.add("-t")
        elif kind != "binary":
            fail("files values are '<Name>:text' or '<Name>:binary', got %r" % spec)
        args.add(name)
        args.add(src)
        args.add(cpp.path.removesuffix(".cpp"))
        ctx.actions.run(
            executable = ctx.executable._embed,
            arguments = [args],
            inputs = [src],
            outputs = [cpp, hpp],
            mnemonic = "TracyEmbed",
            progress_message = "Embedding %s as %s" % (src.short_path, name),
        )
        srcs.append(cpp)
        hdrs.append(hpp)
    return [
        DefaultInfo(files = depset(srcs + hdrs)),
        OutputGroupInfo(
            srcs = depset(srcs),
            hdrs = depset(hdrs),
        ),
    ]

tracy_embed = rule(
    implementation = _tracy_embed_impl,
    doc = "Compresses files into C arrays (gen/data/<Name>.{cpp,hpp}).",
    attrs = {
        "files": attr.label_keyed_string_dict(
            allow_files = True,
            mandatory = True,
            doc = "file -> '<Name>:text' or '<Name>:binary'",
        ),
        "_embed": attr.label(
            default = ":embed",
            executable = True,
            cfg = "exec",
        ),
    },
)
