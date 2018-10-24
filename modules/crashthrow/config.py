#!/usr/bin/env python

def can_build(env, platform):
    return True

def configure(env):
    if env["platform"] == "android":
		env.android_add_java_dir("android")
