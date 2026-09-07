#!/usr/bin/env python3
"""Safety-gate tests only: no compiler, executable, game or database is opened."""
from contextlib import ExitStack
from types import SimpleNamespace
import subprocess
import sys
import unittest
from unittest.mock import patch

import probe_pc_animation as probe


class ProbeSafetyTests(unittest.TestCase):
    def environment(self, pristine=True):
        stack = ExitStack()
        self.addCleanup(stack.close)
        stack.enter_context(patch.object(sys, 'argv', ['probe_pc_animation.py']))
        stack.enter_context(patch.object(probe.Path, 'read_bytes', return_value=b'test input'))
        if pristine:
            # The trusted-fixture path is mocked; these tests exercise control
            # flow, not SHA collision resistance or original instruction bodies.
            stack.enter_context(patch.object(probe, 'sha256', return_value=probe.PC_SHA256))
        mkdir = stack.enter_context(patch.object(probe.Path, 'mkdir'))
        run = stack.enter_context(patch.object(probe.subprocess, 'run'))
        return mkdir, run

    def test_non_pristine_rejected_before_build_or_execution(self):
        mkdir, run = self.environment(pristine=False)
        with self.assertRaisesRegex(SystemExit, 'Refusing non-pristine'):
            probe.main()
        mkdir.assert_not_called()
        run.assert_not_called()

    def test_compile_failure_never_executes_probe(self):
        _, run = self.environment()
        run.return_value = SimpleNamespace(returncode=23)
        self.assertEqual(probe.main(), 23)
        self.assertEqual(run.call_count, 1)
        self.assertEqual(run.call_args.kwargs['timeout'], 120)

    def test_compile_timeout_never_executes_probe(self):
        _, run = self.environment()
        run.side_effect = subprocess.TimeoutExpired('compiler', 120)
        with self.assertRaises(subprocess.TimeoutExpired):
            probe.main()
        self.assertEqual(run.call_count, 1)

    def test_native_execution_has_ten_second_timeout_and_propagates_failure(self):
        _, run = self.environment()
        run.side_effect = [SimpleNamespace(returncode=0), SimpleNamespace(returncode=4)]
        self.assertEqual(probe.main(), 4)
        native_call = run.call_args_list[1]
        self.assertEqual(native_call.kwargs['timeout'], 10)
        self.assertNotIn('shell', native_call.kwargs)
        self.assertEqual(probe.Path(native_call.args[0][0]).name, 'probe_pc_animation.exe')
        self.assertEqual(len(native_call.args[0]), 2)

    def test_native_timeout_is_not_reported_as_success(self):
        _, run = self.environment()
        run.side_effect = [SimpleNamespace(returncode=0), subprocess.TimeoutExpired('probe', 10)]
        with self.assertRaises(subprocess.TimeoutExpired):
            probe.main()
        self.assertEqual(run.call_count, 2)


if __name__ == '__main__':
    unittest.main()
