def make_application_icons(platform, src, round_src = None):
    return struct(
        _valdi_application_icons = platform,
        src = src,
        round_src = round_src,
    )

def is_application_icons(value, platform):
    return hasattr(value, "_valdi_application_icons") and value._valdi_application_icons == platform

def toolbox_attr():
    return attr.label(
        default = Label("//valdi/compiler/toolbox:valdi_compiler_toolbox"),
        executable = True,
        cfg = "exec",
    )

def convert_icon(ctx, src, output, size, round_icon = False):
    args = ctx.actions.args()
    args.add("image_convert")
    args.add("-i", src)
    args.add("-o", output)
    args.add("-w", str(size))
    args.add("-h", str(size))
    if round_icon:
        args.add("--round")

    ctx.actions.run(
        executable = ctx.executable._toolbox,
        inputs = [src],
        outputs = [output],
        arguments = [args],
        mnemonic = "ValdiApplicationIcon",
        progress_message = "Generating application icon {}".format(output.short_path),
    )

def write_apple_contents_json(ctx, output, images):
    ctx.actions.write(
        output = output,
        content = json.encode_indent({
            "images": images,
            "info": {
                "author": "xcode",
                "version": 1,
            },
        }) + "\n",
    )
