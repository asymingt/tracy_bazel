"""cc_test/cc_binary variants built under fixed @tracy//config settings.

Each rule transitions its whole dependency graph (including @tracy//:client),
so one `bazel test //...` exercises several client configurations without
command-line flags. Every enabled variant listens on localhost only and does
not broadcast, so tests never touch the network beyond loopback.
"""

load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@with_cfg.bzl", "with_cfg")

def _flag(name):
    return Label("@tracy//config:" + name)

def _enabled(rule):
    return (
        with_cfg(rule)
            .set(_flag("enable"), True)
            .set(_flag("only_localhost"), True)
            .set(_flag("no_broadcast"), True)
    )

# TRACY_ENABLE with otherwise default options.
tracy_enabled_cc_test, _tracy_enabled_cc_test_internal = _enabled(cc_test).build()

# Non-default option combinations (compile + link coverage).
tracy_on_demand_cc_test, _tracy_on_demand_cc_test_internal = (
    _enabled(cc_test).set(_flag("on_demand"), True).build()
)

tracy_minimal_cc_test, _tracy_minimal_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("no_callstack"), True)
        .set(_flag("no_sampling"), True)
        .set(_flag("no_system_tracing"), True)
        .set(_flag("no_context_switch"), True)
        .set(_flag("no_frame_image"), True)
        .set(_flag("no_crash_handler"), True)
        .build()
)

tracy_fibers_cc_test, _tracy_fibers_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("fibers"), True)
        .set(_flag("callstack"), 8)
        .build()
)

tracy_timer_fallback_cc_test, _tracy_timer_fallback_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("timer_fallback"), True)
        .set(_flag("disallow_hw_timer"), True)
        .build()
)

# Manual lifetime: the program must start and stop the profiler itself.
tracy_manual_lifetime_cc_test, _tracy_manual_lifetime_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("manual_lifetime"), True)
        .set(_flag("delayed_init"), True)
        .build()
)

