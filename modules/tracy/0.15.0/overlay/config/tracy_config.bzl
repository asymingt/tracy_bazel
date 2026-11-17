"""Turns the //config build settings into Tracy compile definitions."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

# Boolean upstream options (CMakeLists.txt `set_option(TRACY_<NAME> ...)`),
# all OFF by default. Flag //config:<name> maps to the define TRACY_<NAME>.
#
# Deliberately absent, because they need dependencies this module does not
# provide: TRACY_LIBUNWIND_BACKTRACE (libunwind), TRACY_DEBUGINFOD
# (libdebuginfod) and TRACY_ROCPROF* (ROCm).
BOOL_OPTIONS = [
    "enable",
    "on_demand",
    "no_callstack",
    "no_callstack_inlines",
    "only_localhost",
    "no_broadcast",
    "only_ipv4",
    "no_code_transfer",
    "no_context_switch",
    "no_exit",
    "no_sampling",
    "no_verify",
    "no_vsync_capture",
    "no_frame_image",
    "no_system_tracing",
    "patchable_nopsleds",
    "delayed_init",
    "manual_lifetime",
    "fibers",
    "no_crash_handler",
    "timer_fallback",
    "disallow_hw_timer",
    "symbol_offline_resolve",
    "libbacktrace_elf_dynload_support",
    "ignore_memory_faults",
    "opengl_auto_calibration",
    "verbose",
    "no_internal_message",
    "demangle",
]

def _tracy_config_impl(ctx):
    enabled = {}
    for target, name in ctx.attr.bool_options.items():
        enabled[name] = target[BuildSettingInfo].value

    # Upstream CMake rejects this combination at configure time.
    if enabled.get("disallow_hw_timer") and not enabled.get("timer_fallback"):
        fail("--@tracy//config:disallow_hw_timer requires --@tracy//config:timer_fallback")

    defines = ["TRACY_" + name.upper() for name in BOOL_OPTIONS if enabled.get(name)]

    callstack = ctx.attr.callstack[BuildSettingInfo].value
    if callstack < 0:
        fail("--@tracy//config:callstack must be >= 0, got %d" % callstack)
    if callstack > 0:
        defines.append("TRACY_CALLSTACK=%d" % callstack)

    platform_header = ctx.attr.platform_header[BuildSettingInfo].value
    if platform_header:
        defines.append('TRACY_PLATFORM_HEADER="%s"' % platform_header)

    own = CcInfo(
        compilation_context = cc_common.create_compilation_context(
            defines = depset(defines),
        ),
    )
    return [cc_common.merge_cc_infos(
        direct_cc_infos = [own],
        cc_infos = [ctx.attr.platform_header_lib[CcInfo]],
    )]

tracy_config = rule(
    implementation = _tracy_config_impl,
    doc = "Provides CcInfo whose `defines` reflect the //config build settings.",
    attrs = {
        "bool_options": attr.label_keyed_string_dict(
            providers = [BuildSettingInfo],
            mandatory = True,
        ),
        "callstack": attr.label(providers = [BuildSettingInfo], mandatory = True),
        "platform_header": attr.label(providers = [BuildSettingInfo], mandatory = True),
        "platform_header_lib": attr.label(providers = [CcInfo], mandatory = True),
    },
    provides = [CcInfo],
)
