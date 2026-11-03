# SPDX-License-Identifier: AGPL-3.0-or-later
"""Actual child-process regressions for Windows redirected encoding failures."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import sys
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('flysnapper_builder_encoding',ROOT/'build_plugin.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)

class BuilderEncodingTests(unittest.TestCase):
    def test_cp1252_console_keeps_full_unicode_in_log(self):
        data=io.BytesIO();console=io.TextIOWrapper(data,encoding='cp1252',newline='',write_through=True)
        log=io.StringIO();text='Olav L\u00e6greid \u6f22 \ufffd'
        with contextlib.redirect_stdout(console),mock.patch.object(b,'_log',log):b.say(text)
        self.assertEqual(data.getvalue().decode('cp1252'),'Olav L\u00e6greid \\u6f22 \\ufffd\n')
        self.assertEqual(log.getvalue(),text+'\n')
        console.detach()

    def test_python_child_utf8_response_and_caller_environment_preserved(self):
        env=os.environ.copy();env.update(PYTHONIOENCODING='cp1252',PYTHONUTF8='0',FLYSNAPPER_ENCODING_TEST='untouched')
        original=env.copy()
        script="import os; print('Olav L\\u00e6greid \\u6f22 \\ufffd'); print(os.environ['FLYSNAPPER_ENCODING_TEST'])"
        with contextlib.redirect_stdout(io.StringIO()):out=b.run([sys.executable,'-c',script],capture=True,env=env)
        self.assertEqual(out,'Olav L\u00e6greid \u6f22 \ufffd\nuntouched')
        self.assertEqual(env,original)

    def test_nonzero_child_preserves_exit_status_under_cp1252(self):
        data=io.BytesIO();console=io.TextIOWrapper(data,encoding='cp1252',newline='',write_through=True)
        with contextlib.redirect_stdout(console),self.assertRaises(b.CommandError) as caught:
            b.run([sys.executable,'-c',"import sys; print('\\u6f22 \\ufffd'); sys.exit(7)"],capture=True)
        self.assertEqual(caught.exception.returncode,7)
        self.assertEqual(caught.exception.output,'\u6f22 \ufffd')
        self.assertIn(b'\\u6f22 \\ufffd',data.getvalue())
        console.detach()

if __name__=='__main__':unittest.main()
