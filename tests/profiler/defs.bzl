"""sh_test variants that build tracy-profiler for a given Linux backend."""

load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("@with_cfg.bzl", "with_cfg")

def _backend(value):
    return with_cfg(sh_test).set(Label("@tracy//config:linux_backend"), value).build()

wayland_sh_test, _wayland_sh_test_internal = _backend("wayland")
x11_sh_test, _x11_sh_test_internal = _backend("x11")
