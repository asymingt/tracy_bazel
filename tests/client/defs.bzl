"""cc_test variants built under fixed @tracy//config settings.

Each rule transitions its whole dependency graph (including @tracy//:client),
so one `bazel test //...` exercises several client configurations without
command-line flags. Every enabled variant listens on localhost only and does
not broadcast, so tests never touch the network beyond loopback.
"""

load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@with_cfg.bzl", "with_cfg")

# Runtime environment for every enabled test.
#
# On x86 Tracy refuses to start unless the CPU reports an invariant TSC, which
# virtualised CI runners (e.g. GitHub's macos-15-intel) do not. These tests
# check functionality, not timer quality, so use upstream's runtime override.
TRACY_TEST_ENV = {
    "TRACY_NO_INVARIANT_CHECK": "1",
}

def _flag(name):
    return Label("@tracy//config:" + name)

def _enabled(rule):
    return (
        with_cfg(rule)
            .set(_flag("enable"), True)
            .set(_flag("only_localhost"), True)
            .set(_flag("no_broadcast"), True)
    )

def _with_test_env(rule):
    def wrapper(name, env = {}, **kwargs):
        rule(name = name, env = TRACY_TEST_ENV | env, **kwargs)

    return wrapper

# TRACY_ENABLE with otherwise default options.
_tracy_enabled_cc_test, _tracy_enabled_cc_test_internal = _enabled(cc_test).build()

# Non-default option combinations (compile + link coverage).
_tracy_on_demand_cc_test, _tracy_on_demand_cc_test_internal = (
    _enabled(cc_test).set(_flag("on_demand"), True).build()
)

_tracy_minimal_cc_test, _tracy_minimal_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("no_callstack"), True)
        .set(_flag("no_sampling"), True)
        .set(_flag("no_system_tracing"), True)
        .set(_flag("no_context_switch"), True)
        .set(_flag("no_frame_image"), True)
        .set(_flag("no_crash_handler"), True)
        .build()
)

_tracy_fibers_cc_test, _tracy_fibers_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("fibers"), True)
        .set(_flag("callstack"), 8)
        .build()
)

_tracy_timer_fallback_cc_test, _tracy_timer_fallback_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("timer_fallback"), True)
        .set(_flag("disallow_hw_timer"), True)
        .build()
)

# Manual lifetime: the program must start and stop the profiler itself.
_tracy_manual_lifetime_cc_test, _tracy_manual_lifetime_cc_test_internal = (
    _enabled(cc_test)
        .set(_flag("manual_lifetime"), True)
        .set(_flag("delayed_init"), True)
        .build()
)

tracy_enabled_cc_test = _with_test_env(_tracy_enabled_cc_test)
tracy_on_demand_cc_test = _with_test_env(_tracy_on_demand_cc_test)
tracy_minimal_cc_test = _with_test_env(_tracy_minimal_cc_test)
tracy_fibers_cc_test = _with_test_env(_tracy_fibers_cc_test)
tracy_timer_fallback_cc_test = _with_test_env(_tracy_timer_fallback_cc_test)
tracy_manual_lifetime_cc_test = _with_test_env(_tracy_manual_lifetime_cc_test)
