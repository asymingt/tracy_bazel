"""Enabled-client variants with one option turned off each (issue #1 bisect)."""

load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@with_cfg.bzl", "with_cfg")

def _flag(name):
    return Label("@tracy//config:" + name)

def _enabled():
    return (
        with_cfg(cc_test)
            .set(_flag("enable"), True)
            .set(_flag("only_localhost"), True)
            .set(_flag("no_broadcast"), True)
    )

baseline_test, _baseline_test_internal = _enabled().build()
no_sampling_test, _no_sampling_test_internal = _enabled().set(_flag("no_sampling"), True).build()
no_system_tracing_test, _no_system_tracing_test_internal = _enabled().set(_flag("no_system_tracing"), True).build()
no_context_switch_test, _no_context_switch_test_internal = _enabled().set(_flag("no_context_switch"), True).build()
no_callstack_test, _no_callstack_test_internal = _enabled().set(_flag("no_callstack"), True).build()
no_crash_handler_test, _no_crash_handler_test_internal = _enabled().set(_flag("no_crash_handler"), True).build()
no_frame_image_test, _no_frame_image_test_internal = _enabled().set(_flag("no_frame_image"), True).build()
