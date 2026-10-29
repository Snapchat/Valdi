"""Unit tests for constants exported by common.bzl."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load(":common.bzl", "ANDROID_OS_CONSTRAINT", "IOS_OS_CONSTRAINT", "LINUX_OS_CONSTRAINT")

def _assert_resolved_platforms_label(env, constraint, expected_suffix):
    # The OS constraints must be Labels constructed at Valdi's own load time,
    # not bare "@platforms//os:..." strings. A bare string passed to a
    # target_compatible_with attribute resolves against the *calling* package's
    # repo mapping, so any downstream module that calls a Valdi macro but does
    # not itself declare the `platforms` bazel_dep fails to build. A Label
    # bakes in Valdi's repo mapping regardless of caller. See
    # Valdi_Widgets#20.
    asserts.equals(env, "Label", type(constraint))

    constraint_str = str(constraint)
    asserts.true(
        env,
        constraint_str.endswith(expected_suffix),
        "expected constraint to target %s, got %s" % (expected_suffix, constraint_str),
    )
    asserts.true(
        env,
        "platforms" in constraint_str,
        "expected constraint to resolve against the platforms repo, got %s" % constraint_str,
    )

def _ios_os_constraint_is_resolved_label_test_impl(ctx):
    env = unittest.begin(ctx)
    _assert_resolved_platforms_label(env, IOS_OS_CONSTRAINT, "//os:ios")
    return unittest.end(env)

ios_os_constraint_is_resolved_label_test = unittest.make(_ios_os_constraint_is_resolved_label_test_impl)

def _linux_os_constraint_is_resolved_label_test_impl(ctx):
    env = unittest.begin(ctx)
    _assert_resolved_platforms_label(env, LINUX_OS_CONSTRAINT, "//os:linux")
    return unittest.end(env)

linux_os_constraint_is_resolved_label_test = unittest.make(_linux_os_constraint_is_resolved_label_test_impl)

def _android_os_constraint_is_resolved_label_test_impl(ctx):
    env = unittest.begin(ctx)
    _assert_resolved_platforms_label(env, ANDROID_OS_CONSTRAINT, "//os:android")
    return unittest.end(env)

android_os_constraint_is_resolved_label_test = unittest.make(_android_os_constraint_is_resolved_label_test_impl)

def common_test_suite(name):
    """Registers unit tests for common.bzl.

    Args:
        name: name of the generated test_suite target.
    """
    unittest.suite(
        name,
        ios_os_constraint_is_resolved_label_test,
        linux_os_constraint_is_resolved_label_test,
        android_os_constraint_is_resolved_label_test,
    )
